#pragma once

#include <elf.h>
#include <filesystem>
#include <unordered_map>
#include <map>
#include <libsdb/error.hpp>
#include <libsdb/bit.hpp>

namespace sdb {
    struct elf {
        explicit elf(std::filesystem::path);
        ~elf();

        elf(const elf&) = delete;
        elf& operator=(const elf&) = delete;

        elf(elf&&) noexcept;
        elf& operator=(elf&&) noexcept;

        virt_addr load_bias() const { return load_bias_; }
        void notify_loaded(virt_addr address) { load_bias_ = address; }

        [[nodiscard]] constexpr const std::filesystem::path& get_path() const { return path; }
        [[nodiscard]] const Elf64_Ehdr& get_header() const {
            return *reinterpret_cast<const Elf64_Ehdr*>(data_.data());
        }

        [[nodiscard]] std::span<const Elf64_Shdr> get_section_headers() const {
            const auto section_header_view = data_.subspan(get_header().e_shoff);

            if (get_header().e_shentsize != sizeof(Elf64_Shdr))
                error::send("Invalid ELF section header size");

            const auto n_sections = [&] -> std::size_t {
                if (get_header().e_shnum != 0)
                    return get_header().e_shnum;

                // Go to the first section header for the number of sections.
                return from_bytes_to<decltype(Elf64_Shdr::sh_size)>(section_header_view.data() + offsetof(Elf64_Shdr, sh_size));
            }();

            return { reinterpret_cast<const Elf64_Shdr*>(section_header_view.data()), n_sections };
        }
        [[nodiscard]] std::string_view get_section_name(std::size_t shstr_index) const;
        [[nodiscard]] std::optional<const Elf64_Shdr&> get_section_header(std::string_view name) const;
        [[nodiscard]] std::span<const std::byte> get_section_contents(std::string_view name) const;

        [[nodiscard]] std::string_view get_string(std::size_t string_index) const;

        [[nodiscard]] std::optional<const Elf64_Shdr&> get_section_containing_address(file_addr addr) const;
        [[nodiscard]] std::optional<const Elf64_Shdr&> get_section_containing_address(virt_addr addr) const;

        [[nodiscard]] std::span<const Elf64_Sym> get_symbol_entries() const {
            const auto symbol_header = get_section_header(".symtab").or_else([this] { return get_section_header(".dynsym"); });
            if (!symbol_header) return {};
            const auto section_start_address = symbol_header->sh_offset;
            const auto entry_count = symbol_header->sh_size / symbol_header->sh_entsize;
            return { reinterpret_cast<const Elf64_Sym*>(data_.subspan(section_start_address).data()), entry_count };
        }
        [[nodiscard]] std::vector<const Elf64_Sym*> get_symbols_by_name(std::string_view name) const;
        [[nodiscard]] std::optional<const Elf64_Sym&> get_symbol_at_address(file_addr addr) const;
        [[nodiscard]] std::optional<const Elf64_Sym&> get_symbol_at_address(virt_addr addr) const;
        [[nodiscard]] std::optional<const Elf64_Sym&> get_symbol_containing_address(file_addr addr) const;
        [[nodiscard]] std::optional<const Elf64_Sym&> get_symbol_containing_address(virt_addr addr) const;
    private:
        void build_section_map();   // Section name to entry
        void build_symbol_map();    // Address to symbol entry, and symbol name to symbol entry

        int fd_;
        std::filesystem::path path;
        std::span<std::byte> data_;

        virt_addr load_bias_;

        std::unordered_map<std::string_view, const Elf64_Shdr*> section_map_;

        struct range_comparator {
            constexpr bool operator()(const std::pair<file_addr, file_addr>& lhs, const std::pair<file_addr, file_addr>& rhs) const {
                return lhs.first < rhs.first;
            }
        };
        std::map<std::pair<file_addr, file_addr>, const Elf64_Sym*, range_comparator> symbol_addr_map_;

        std::unordered_multimap<std::string_view, const Elf64_Sym*> symbol_name_map_;
        std::vector<std::string> demangled_names;
    };
}
