#pragma once

#include <unordered_map>
#include <cstdint>
#include <memory>
#include <vector>
#include <libsdb/detail/dwarf.h>

/**
 * One executable ->
 * - `.debug_info` is unique to each compile-unit.
 *   - An executable is composed of multiple compile units.
 *   - Each compile-unit has its own independent "tree".
 *   - Trees in `.debug_info` are laid out sequentially in memory.
 *   - Node in the tree are called DIE.
 *   - In one DIE, we have abbrev_code, followed by form values. Form types and other stuff is derived from `.debug_abbrev`.
 * - One `.debug_abbrev` section, representing the table entries used by `.debug_info`.
 */
namespace sdb {
    struct dwarf;

    struct abbrev {
        struct attr_spec {
            std::uint64_t attr;
            DW_FORM form;
        };

        std::uint64_t code;
        std::uint64_t tag;
        bool has_children;
        std::vector<attr_spec> attr_specs;
    };
    struct die;
    struct compile_unit {
        dwarf& dwarf_;
        std::span<const std::byte> debug_info_view;
        std::uint64_t debug_abbrev_offset;

        [[nodiscard]] const std::unordered_map<std::uint64_t, abbrev>& abbrev_table() const;
        [[nodiscard]] die root() const;
    };

    struct die {
        compile_unit const* compile_unit;
        std::optional<const abbrev&> abbrev;

        std::span<const std::byte> debug_info_die_view;
        /**
         * Points to the memory from the beginning of the next DIE to the end of CU.
         */
        std::span<const std::byte> next;

        std::vector<const std::byte*> form_values_start_locs;

        [[nodiscard]] std::generator<std::span<const std::byte>> get_form_values() const;
        [[nodiscard]] std::generator<die> get_children_with_null() const;
        [[nodiscard]] std::generator<die> get_children() const;
        [[nodiscard]] constexpr bool is_null() const { return !abbrev.has_value(); }
        [[nodiscard]] constexpr bool has_children() const { return abbrev.has_value() && abbrev->has_children; }
    };

    struct elf;
    struct dwarf {
        explicit dwarf(const elf& parent);
        const elf& elf_file() const { return elf_; }

        const std::unordered_map<std::uint64_t, abbrev>& get_abbrev_table(std::uint64_t offset);
        const std::vector<std::unique_ptr<compile_unit>>& compile_units() const { return compile_units_; }
    private:
        const elf& elf_;
        std::unordered_map<std::uint64_t, std::unordered_map<std::uint64_t, abbrev>> abbrev_tables_;
        std::vector<std::unique_ptr<compile_unit>> compile_units_;
    };
}
