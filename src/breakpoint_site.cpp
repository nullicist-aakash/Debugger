#include <libsdb/breakpoint_site.hpp>

namespace {
    auto get_next_id() {
        static sdb::breakpoint_site::id_type id_ = 0;
        return ++id_;
    }
}

sdb::breakpoint_site::breakpoint_site(const process &proc, virt_addr addr) : m_process(&proc), m_id(get_next_id()), m_address(addr), m_is_enabled(false), m_saved_data{} {

}

