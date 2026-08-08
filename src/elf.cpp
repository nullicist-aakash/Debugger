#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libsdb/elf.hpp>
#include <libsdb/error.hpp>
#include <libsdb/bit.hpp>
#include <utility>

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

std::optional<const Elf64_Shdr*> sdb::elf::get_section_header(std::string_view name) const {
    if (const auto it = section_map_.find(name); it != section_map_.end())
        return it->second;
    return std::nullopt;
}

std::span<const std::byte> sdb::elf::get_section_contents(std::string_view name) const {
    if (const auto header = get_section_header(name); header) {
        const auto section_file_offset = header.value()->sh_offset;
        const auto section_size = header.value()->sh_size;
        return data_.subspan(section_file_offset, section_size);
    }
    return {};
}

std::string_view sdb::elf::get_string(std::size_t string_index) const {
    if (const auto string_header = get_section_header(".strtab").or_else([this] { return get_section_header(".dynstr"); })) {
        const auto file_offset = string_header.value()->sh_offset;
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

void sdb::elf::build_section_map() {
    for (const auto& header: get_section_headers())
        section_map_[get_section_name(header.sh_name)] = &header;
}
