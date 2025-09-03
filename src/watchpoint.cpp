#include <libsdb/watchpoint.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

namespace {
    auto get_next_id() {
        static sdb::watchpoint::id_type id = 0;
        return ++id;
    }
}

sdb::watchpoint::watchpoint(process& proc, virt_addr address, stoppoint_mode mode, std::size_t size) :
    m_id {get_next_id()},
    m_process{ &proc },
    m_address{ address },
    m_mode{ mode },
    m_size{ size },
    m_is_enabled{ false } {

    // Make sure that address are properly aligned
    if ((address.addr() & (size - 1)) != 0)
        error::send("Watchpoint must be aligned to size");
}

void sdb::watchpoint::enable() {
    if (m_is_enabled) return;
    m_hardware_register_index = m_process->set_watchpoint(m_id, m_address, m_mode, m_size);
    m_is_enabled = true;
}

void sdb::watchpoint::disable() {
    if (!m_is_enabled) return;

    m_process->clear_hardware_stoppoint(m_hardware_register_index);
    m_is_enabled = false;
}