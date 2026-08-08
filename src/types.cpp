#include <libsdb/types.hpp>
#include <libsdb/elf.hpp>

std::optional<sdb::virt_addr> sdb::file_addr::to_virt_addr() const {
    contract_assert(elf_ != nullptr);

    if (elf_->get_section_containing_address(*this) == std::nullopt) return std::nullopt;
    return virt_addr { addr_ + elf_->load_bias().addr() };
}

std::optional<sdb::file_addr> sdb::virt_addr::to_file_addr(const elf& obj) const {
    if (obj.get_section_containing_address(*this) == std::nullopt) return std::nullopt;
    return file_addr{ obj, addr_ - obj.load_bias().addr()};
}
