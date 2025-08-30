#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <libsdb/bit.hpp>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ptrace.h>
#include <sys/personality.h>
#include <sys/wait.h>
#include <unistd.h>
#include <format>
#include <iostream>

sdb::stop_reason::stop_reason(int wait_status) {
    if (WIFEXITED(wait_status)) {
        reason = process_state::EXITED;
        info = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        reason = process_state::TERMINATED;
        info = WTERMSIG(wait_status);
    } else if (WIFSTOPPED(wait_status)) {
        reason = process_state::STOPPED;
        info = WSTOPSIG(wait_status);
    } else {
        error::send(std::format("Got a wait_status which doesn't represent a non-running child: {}", wait_status));
    }
}

std::unique_ptr<sdb::process> sdb::process::launch(const std::filesystem::path& path, bool debug, std::optional<int> stdout_replacement) {
    pid_t pid;
    pipe channel(true);

    if ((pid = fork()) < 0)
        error::send_errno("fork failed");

    // If child process, prepare itself for PTRACE before exec, so that it never
    // starts running, and we can do something in debugger before executing it.
    if (pid == 0) {
        personality(ADDR_NO_RANDOMIZE);
        channel.close_read();

        if (stdout_replacement)
            if (dup2(*stdout_replacement, STDOUT_FILENO) < 0)
                error::exit_with_errno(channel, "stdout replacement failed");

        if (debug && ptrace(PTRACE_TRACEME, pid, nullptr, nullptr) < 0)
            error::exit_with_errno(channel, "Tracing failed");

        if (execlp(path.c_str(), path.c_str(), nullptr) < 0) {
            error::exit_with_errno(channel, "exec failed");
        }
    }

    // Since we started this process, we want to terminate it on debugger end.
    std::unique_ptr<process> _process(new process(pid, true, debug));
    if (debug)
        _process->wait_on_signal();

    channel.close_write();
    auto data = channel.read();
    channel.close_read();

    if (!data.empty()) {
        auto chars = reinterpret_cast<char*>(data.data());
        error::send(std::string{chars, chars + data.size()});;
    }

    return _process;
}

std::unique_ptr<sdb::process> sdb::process::attach(pid_t pid) {
    if (pid <= 0)
        error::send(std::format("Invalid PID: {}", pid));

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0)
        error::send_errno("Could not attach");

    // The process is already running. So on termination, we should not kill it
    std::unique_ptr<process> _process(new process(pid, false, true));
    _process->wait_on_signal();
    return _process;
}

void sdb::process::resume() {
    // If we stopped on SIGTRAP and PC-1 is breakpoint, then reset the instruction, run one assembly instruction, re-enter breakpoint
    if (auto pc = get_pc(); m_breakpoints.enabled_stoppoint_at_address(pc)) {
        auto& bp = m_breakpoints.get_by_address(pc);
        bp.disable();
        if (ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr) < 0)
            error::send_errno("Failed a single step");
        wait_on_signal();
        bp.enable();
    }

    if (ptrace(PTRACE_CONT, m_pid, nullptr, nullptr) < 0)
        error::send_errno("Could not resume");

    this->m_state = process_state::RUNNING;
}

sdb::stop_reason sdb::process::wait_on_signal() {
    int status = 0;

    if (waitpid(m_pid, &status, 0) < 0)
        error::send_errno("waitpid failed");

    const auto reason = stop_reason(status);
    this->m_state = reason.reason;

    if (m_is_attached && this->m_state == process_state::STOPPED) {
        read_all_registers();
        // If we stopped on SIGTRAP and PC-1 is breakpoint, then reset the instruction, run one assembly instruction, re-enter breakpoint
        if (const auto instruction_start = get_pc() - 1; reason.info == SIGTRAP && m_breakpoints.enabled_stoppoint_at_address(instruction_start)) {
            set_pc(instruction_start);
        }
    }

    return reason;
}

sdb::process::~process() {
    if (m_pid == 0)
        return;

    if (m_is_attached) {
        // Detach the process
        if (m_state == process_state::RUNNING) {
            // Before PTRACE_DETACH, we need the process to stop.
            kill(m_pid, SIGSTOP);
            waitpid(m_pid, nullptr, 0);
        }

        ptrace(PTRACE_DETACH, m_pid, nullptr, nullptr);
        // Continue the process after detach
        kill(m_pid, SIGCONT);
    }

    if (m_terminate_on_end) {
        kill(m_pid, SIGKILL);
        waitpid(m_pid, nullptr, 0);
    }
}


void sdb::process::read_all_registers() {
    if (ptrace(PTRACE_GETREGS, m_pid, nullptr, &get_registers().m_data.regs) < 0)
        error::send_errno("Could not read GPR registers");

    if (ptrace(PTRACE_GETFPREGS, m_pid, nullptr, &get_registers().m_data.i387) < 0)
        error::send_errno("Could not read FPR registers");

    for (int i = 0; i < 8; ++i) {
        auto id = static_cast<int>(register_id::dr0) + i;
        auto info = register_info_by_id(static_cast<register_id>(id));

        errno = 0;
        std::int64_t data = ptrace(PTRACE_PEEKUSER, m_pid, info.offset, nullptr);
        if (errno != 0)
            error::send_errno("Could not read debug registers");
        get_registers().m_data.u_debugreg[i] = data;
    }
}


void sdb::process::write_user_struct(std::size_t offset, std::uint64_t data) {
    if (ptrace(PTRACE_POKEUSER, m_pid, offset, data) < 0) {
        error::send_errno("Could not write to user struct");
    }
}


void sdb::process::write_gprs(const user_regs_struct& gprs) {
    if (ptrace(PTRACE_SETREGS, m_pid, nullptr, &gprs) < 0) {
        error::send_errno("Could not set GPR registers");
    }
}


void sdb::process::write_fprs(const user_fpregs_struct& fprs) {
    if (ptrace(PTRACE_SETFPREGS, m_pid, nullptr, &fprs) < 0) {
        error::send_errno("Could not set GPR registers");
    }
}

sdb::breakpoint_site &sdb::process::create_breakpoint_site(const virt_addr& address) {
    if (m_breakpoints.contains_address(address))
        error::send(std::format("Breakpoint site already created at address {}", std::to_string(address.addr())));

    return m_breakpoints.push(std::unique_ptr<breakpoint_site>(new breakpoint_site { *this, address }));
}

sdb::stop_reason sdb::process::step_instruction() {
    // If we stopped on SIGTRAP and PC is breakpoint, then reset the instruction, run one assembly instruction, re-enter breakpoint
    std::optional<breakpoint_site*> bp_to_enable;
    if (auto pc = get_pc(); m_breakpoints.enabled_stoppoint_at_address(pc)) {
        auto& bp = m_breakpoints.get_by_address(pc);
        bp.disable();
        bp_to_enable = &bp;
    }

    if (ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr) < 0)
        error::send_errno("Could not single step");

    auto reason = wait_on_signal();

    if (bp_to_enable)
        bp_to_enable.value()->enable();

    return reason;
}


std::vector<std::byte> sdb::process::read_memory(virt_addr address, std::size_t amount) const {
    std::vector<std::byte> ret(amount);
    iovec local_desc{ ret.data(), ret.size() };

    std::vector<iovec> remote_descs;

    while (amount > 0) {
        const auto up_to_next_page = 0x1000 - (address.addr() & 0xfff);
        const auto chunk_size = std::min(amount, up_to_next_page);
        remote_descs.push_back({ reinterpret_cast<void*>(address.addr()), chunk_size });
        amount -= chunk_size;
        address += chunk_size;
    }

    if (process_vm_readv(m_pid, &local_desc, 1, remote_descs.data(), remote_descs.size(), 0) < 0)
        error::send_errno("Could not read process memory");

    return ret;
}

void sdb::process::write_memory(virt_addr address, std::span<const std::byte> data) {
    std::size_t written = 0;

    while (written < data.size()) {
        auto remaining = data.size() - written;
        std::uint64_t word;
        if (remaining >= 8)
            word = sdb::from_bytes_to<std::uint64_t>(data.data() + written);
        else {
            auto read = read_memory(address + written, 8);
            auto word_data = reinterpret_cast<char*>(&word);
            std::memcpy(word_data, data.data() + written, remaining);
            std::memcpy(word_data + remaining, read.data() + remaining, 8 - remaining);
        }
        if (ptrace(PTRACE_POKEDATA, m_pid, address + written, word) < 0)
            error::send_errno("Failed to write memory");
        written += 8;
    }
}