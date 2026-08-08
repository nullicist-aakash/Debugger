#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libsdb/elf.hpp>
#include <libsdb/error.hpp>
#include <libsdb/bit.hpp>
#include <utility>
#include <cxxabi.h>

namespace {
    std::optional<std::string> demangle(std::string_view name) {
        int status = 0;

        const auto demangled_name = abi::__cxa_demangle(name.data(), nullptr, nullptr, &status);
        if (status != 0) return std::nullopt;

        std::string ret{ demangled_name };
        free(demangled_name);
        return ret;
    }
}

sdb::elf::elf(std::filesystem::path  path) : path(std::move(path)) {
    if ((fd_ = open(this->path.c_str(), O_RDONLY)) < 0)
        error::send_errno("Failed to open file");

    struct stat stats{};
    if (fstat(fd_, &stats) < 0)
        error::send_errno("Could not retrieve ELF file stats");

    const std::size_t size = stats.st_size;

    if (void* ret = nullptr; (ret = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd_, 0)) == MAP_FAILED) {
        close(fd_);
        error::send_errno("Could not map ELF file");
    } else
        data_ = std::span{static_cast<std::byte*>(ret), size};

    build_section_map();
    build_symbol_map();
}

sdb::elf::~elf() {
    munmap(data_.data(), data_.size());
    close(fd_);
}

sdb::elf::elf(elf && other_) noexcept {
    this->path = std::move(other_.path);
    this->data_ = std::move(other_.data_);
    this->fd_ = other_.fd_;
}

sdb::elf& sdb::elf::operator=(elf && other_) noexcept {
    if (this == &other_) return *this;
    this->path = std::move(other_.path);
    this->data_ = std::move(other_.data_);
    this->fd_ = other_.fd_;
    return *this;
}

std::string_view sdb::elf::get_section_name(std::size_t shstr_index) const {
    const auto& shstr_header = get_section_headers().at(get_header().e_shstrndx);
    return { reinterpret_cast<char*>(data_.subspan(shstr_header.sh_offset + shstr_index).data()) };
}

std::optional<const Elf64_Shdr&> sdb::elf::get_section_header(std::string_view name) const {
    if (const auto it = section_map_.find(name); it != section_map_.end())
        return *it->second;
    return std::nullopt;
}

std::span<const std::byte> sdb::elf::get_section_contents(std::string_view name) const {
    if (const auto header = get_section_header(name); header) {
        const auto section_file_offset = header->sh_offset;
        const auto section_size = header->sh_size;
        return data_.subspan(section_file_offset, section_size);
    }
    return {};
}

std::string_view sdb::elf::get_string(std::size_t string_index) const {
    if (const auto string_header = get_section_header(".strtab").or_else([this] { return get_section_header(".dynstr"); })) {
        const auto file_offset = string_header->sh_offset;
        return { reinterpret_cast<char*>(data_.subspan(file_offset + string_index).data()) };
    }

    return {};
}

std::optional<const Elf64_Shdr&> sdb::elf::get_section_containing_address(file_addr addr) const {
    if (&addr.elf_file().value() != this) return std::nullopt;

    for (const auto& section_header: get_section_headers())
        if (section_header.sh_addr <= addr.addr() && addr.addr() < section_header.sh_addr + section_header.sh_size)
            return section_header;

    return std::nullopt;
}

std::optional<const Elf64_Shdr&> sdb::elf::get_section_containing_address(virt_addr addr) const {
    for (const auto& section : get_section_headers())
        if (load_bias_ + section.sh_addr <= addr && load_bias_ + section.sh_addr + section.sh_size > addr)
            return section;

    return std::nullopt;
}

std::vector<const Elf64_Sym *> sdb::elf::get_symbols_by_name(std::string_view name) const {
    auto [begin, end] = symbol_name_map_.equal_range(name);
    std::vector<const Elf64_Sym*> ret;
    ret.reserve(std::distance(begin, end));
    std::transform(begin, end, std::back_inserter(ret), [](const auto& pair) { return pair.second; });
    return ret;
}

std::optional<const Elf64_Sym&> sdb::elf::get_symbol_at_address(file_addr addr) const {
    if (&addr.elf_file().value() != this) return std::nullopt;
    if (const auto it = symbol_addr_map_.find({addr, {}}); it != symbol_addr_map_.end())
        return *it->second;
    return std::nullopt;
}

std::optional<const Elf64_Sym&> sdb::elf::get_symbol_at_address(virt_addr addr) const {
    return get_symbol_at_address(addr.to_file_addr(*this).value_or({}));
}

std::optional<const Elf64_Sym&> sdb::elf::get_symbol_containing_address(file_addr addr) const {
    if (&addr.elf_file().value() != this) return std::nullopt;
    auto it = symbol_addr_map_.lower_bound({addr, {}});
    if (it != symbol_addr_map_.end() && it->first.first == addr)
        return *it->second;

    if (it == symbol_addr_map_.begin()) return std::nullopt;
    if (const auto [key, value] = *--it; key.first < addr && key.second > addr)
        return *value;

    return std::nullopt;
}

std::optional<const Elf64_Sym&> sdb::elf::get_symbol_containing_address(virt_addr addr) const {
    return get_symbol_containing_address(addr.to_file_addr(*this).value_or({}));
}


void sdb::elf::build_section_map() {
    for (const auto& header: get_section_headers())
        section_map_[get_section_name(header.sh_name)] = &header;
}

void sdb::elf::build_symbol_map() {
    std::vector<const Elf64_Sym*> demangled_ptrs;
    for (const auto& symbol_entry: get_symbol_entries()) {
        const auto mangled_name = get_string(symbol_entry.st_name);
        if (!mangled_name.empty())
            symbol_name_map_.insert({mangled_name, &symbol_entry});

        if (auto demangled_name = demangle(mangled_name)) {
            demangled_names.push_back(std::move(*demangled_name));
            demangled_ptrs.push_back(&symbol_entry);
        }

        if (symbol_entry.st_name != 0 && symbol_entry.st_value != 0 && ELF64_ST_TYPE(symbol_entry.st_info) != STT_TLS) {
            const auto start_addr = file_addr {*this, symbol_entry.st_value};
            const auto end_addr = file_addr { *this, symbol_entry.st_value + symbol_entry.st_size };
            symbol_addr_map_.insert({std::make_pair(start_addr, end_addr), &symbol_entry});
        }
    }

    demangled_names.shrink_to_fit();
    for (const auto idx: std::views::iota(0uz, demangled_ptrs.size()))
        symbol_name_map_.insert({demangled_names.at(idx), demangled_ptrs.at(idx)});
}
