#include <sys/ptrace.h>
#include <libsdb/breakpoint_site.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

constexpr std::uint64_t SOFTWARE_BREAKPOINT_INSTRUCTION = 0xcc; // int3 interrupt

namespace {
    auto get_next_id() {
        static sdb::breakpoint_site::id_type id_ = 0;
        return ++id_;
    }
}

sdb::breakpoint_site::breakpoint_site(const process &proc, virt_addr addr) : m_process(&proc), m_id(get_next_id()), m_address(addr), m_is_enabled(false), m_saved_data{} {

}

void sdb::breakpoint_site::enable() {
    if (m_is_enabled)
        return;

    // Check address validity by fetching the contents at specified address
    errno = 0;
    const std::uint64_t data = ptrace(PTRACE_PEEKDATA, m_process->pid(), m_address, nullptr);
    if (errno != 0)
        error::send_errno("Enabling breakpoint site failed: Failed to fetch the contents at specified memory location");

    // Data contains 8 bytes of consecutive data. We only need 1 byte in x86 assembly to enable a breakpoint
    m_saved_data = static_cast<std::byte>(data & 0xff);

    const std::uint64_t data_with_int3 = ((data & ~0xff) | SOFTWARE_BREAKPOINT_INSTRUCTION);

    if (ptrace(PTRACE_POKEDATA, m_process->pid(), m_address, data_with_int3) < 0)
        error::send_errno("Enabling breakpoint site failed: Failed to set the breakpoint instruction at specified memory location");

    m_is_enabled = true;
}

void sdb::breakpoint_site::disable() {
    if (!m_is_enabled)
        return;

    errno = 0;
    const std::uint64_t data = ptrace(PTRACE_PEEKDATA, m_process->pid(), m_address, nullptr);
    if (errno != 0)
        error::send_errno("Disabling breakpoint site failed: Failed to fetch the contents at specified memory location");

    const auto restored = ((data & ~0xff) | static_cast<std::uint8_t>(m_saved_data));

    if (ptrace(PTRACE_POKEDATA, m_process->pid(), m_address, restored) < 0)
        error::send_errno("Disabling breakpoint site failed: Failed to remove the breakpoint instruction at specified memory location");

    m_is_enabled = false;
}