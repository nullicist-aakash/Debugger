#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <format>

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

std::unique_ptr<sdb::process> sdb::process::launch(const std::filesystem::path& path, bool debug) {
    pid_t pid;
    pipe channel(true);

    if ((pid = fork()) < 0)
        error::send_errno("fork failed");

    // If child process, prepare itself for PTRACE before exec, so that it never
    // starts running, and we can do something in debugger before executing it.
    if (pid == 0) {
        channel.close_read();

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
