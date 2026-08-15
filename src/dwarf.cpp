#include <numeric>
#include <span>
#include <generator>
#include <libsdb/dwarf.hpp>
#include <libsdb/types.hpp>
#include <libsdb/bit.hpp>
#include <libsdb/elf.hpp>

namespace {
    struct cursor {
        explicit constexpr cursor(std::span<const std::byte> data) : data(data) {

        }

        constexpr cursor& operator++() noexcept {
            data = data.subspan(1);
            return *this;
        }

        constexpr cursor& operator+=(std::size_t n) noexcept {
            data = data.subspan(n);
            return *this;
        }

        [[nodiscard]] constexpr const std::byte& position() const {
            return data.at(0);
        }

        [[nodiscard]] constexpr bool finished() const {
            return data.empty();
        }

        template <typename T> requires std::is_trivially_copyable_v<T>
        constexpr T fixed_int() {
            auto t = sdb::from_bytes_to<T>(data.data());
            data = data.subspan(sizeof(T));
            return t;
        }

        constexpr std::uint8_t u8() { return fixed_int<std::uint8_t>(); }
        constexpr std::uint16_t u16() { return fixed_int<std::uint16_t>(); }
        constexpr std::uint32_t u32() { return fixed_int<std::uint32_t>(); }
        constexpr std::uint64_t u64() { return fixed_int<std::uint64_t>(); }
        constexpr std::int8_t s8() { return fixed_int<std::int8_t>(); }
        constexpr std::int16_t s16() { return fixed_int<std::int16_t>(); }
        constexpr std::int32_t s32() { return fixed_int<std::int32_t>(); }
        constexpr std::int64_t s64() { return fixed_int<std::int64_t>(); }
        constexpr std::string_view string() {
            const auto null_terminator = std::ranges::find(data, std::byte{0});
            const auto string_size = static_cast<std::size_t>(null_terminator - data.begin());
            auto return_value = std::string_view{reinterpret_cast<const char*>(data.data()), string_size };
            data = data.subspan(string_size + 1);
            // ReSharper disable once CppDFALocalValueEscapesFunction
            return return_value;
        }
        constexpr std::uint64_t uleb128() {
            std::uint64_t output = 0;
            int shift = 0;
            std::uint8_t byte;
            do {
                byte = u8();
                output |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
                shift += 7;
            } while ((byte & 0x80) != 0);
            return output;
        }
        constexpr std::int64_t sleb128() {
            std::uint64_t output = 0;
            int shift = 0;
            std::uint8_t byte;
            do {
                byte = u8();
                output |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
                shift += 7;
            } while ((byte & 0x80) != 0);

            if (shift < sizeof(output) * 8 && (byte & 0x40))
                output |= (~static_cast<std::uint64_t>(0) << shift);

            return static_cast<std::int64_t>(output);
        }

        constexpr void skip_form(DW_FORM form) {
            switch (form) {
                case DW_FORM::flag_present: break;
                case DW_FORM::data1:
                case DW_FORM::ref1:
                case DW_FORM::block1:
                case DW_FORM::flag: data = data.subspan(1); break;
                case DW_FORM::data2:
                case DW_FORM::block2:
                case DW_FORM::ref2: data = data.subspan(2); break;
                case DW_FORM::data4:
                case DW_FORM::ref4:
                case DW_FORM::block4:
                case DW_FORM::ref_addr:
                case DW_FORM::sec_offset:
                case DW_FORM::strp: data = data.subspan(4); break;
                case DW_FORM::data8:
                case DW_FORM::ref8:
                case DW_FORM::ref_sig8:
                case DW_FORM::addr: data = data.subspan(8); break;
                case DW_FORM::sdata: sleb128(); break;
                case DW_FORM::udata:
                case DW_FORM::block:
                case DW_FORM::exprloc:
                case DW_FORM::ref_udata: uleb128(); break;
                case DW_FORM::string: string(); break;
                case DW_FORM::indirect: skip_form(static_cast<DW_FORM>(uleb128())); break;

                default: sdb::error::send("Unrecognised DWARF form");
            }
        }

    private:
        std::span<const std::byte> data;
    };

    std::generator<sdb::abbrev::attr_spec> read_attr_specs(cursor& cur) {
        while (true) {
            const auto attr = sdb::abbrev::attr_spec(cur.uleb128(), static_cast<DW_FORM>(cur.uleb128()));
            if (attr.attr == 0)
                co_return;
            co_yield attr;
        }
    }

    std::generator<sdb::abbrev> read_attr_abbrev(cursor& cur) {
        while (true) {
            const auto code = cur.uleb128();
            if (code == 0)
                co_return;
            const auto tag = cur.uleb128();
            const auto has_children = static_cast<bool>(cur.u8());
            std::vector<sdb::abbrev::attr_spec> attr_specs = read_attr_specs(cur) | std::ranges::to<std::vector>();
            co_yield sdb::abbrev{code, tag, has_children, std::move(attr_specs)};
        }
    }

    std::unordered_map<std::uint64_t, sdb::abbrev> parse_abbrev_table(const sdb::elf& elf, std::size_t offset) {
        cursor cur(elf.get_section_contents(".debug_abbrev"));
        cur += offset;

        std::unordered_map<std::uint64_t, sdb::abbrev> output;
        for (auto abbrev : read_attr_abbrev(cur)) {
            const auto code = abbrev.code;
            output[code] = std::move(abbrev);
        }
        return output;
    }

    [[nodiscard]] std::unique_ptr<sdb::compile_unit> parse_compile_unit(sdb::dwarf& dwarf, cursor& cur) {
        const auto start = &cur.position();
        const auto size = cur.u32();
        const auto version = cur.u16();
        const auto debug_abbrev_offset = cur.u32();
        const auto address_size = cur.u8();

        if (size == 0xFF'FF'FF'FF)
            sdb::error::send("Only DWARF32 is supported");
        if (version != 4)
            sdb::error::send("Only DWARF version 4 is supported");
        if (address_size != 8)
            sdb::error::send("Only 64-bit addresses are supported");

        return std::make_unique<sdb::compile_unit>(sdb::compile_unit{
            .dwarf_ = dwarf,
            .debug_info_view = std::span {start, size + sizeof(std::uint32_t)},    // size doesn't contain the size
            .debug_abbrev_offset = debug_abbrev_offset
        });
    }

    [[nodiscard]] std::vector<std::unique_ptr<sdb::compile_unit>> parse_compile_units(sdb::dwarf& dwarf, const sdb::elf& elf) {
        cursor cur(elf.get_section_contents(".debug_info"));
        std::vector<std::unique_ptr<sdb::compile_unit>> output;
        while (!cur.finished()) {
            auto unit = parse_compile_unit(dwarf, cur);
            cur += unit->debug_info_view.size();
            output.push_back(std::move(unit));
        }
        return output;
    }

    sdb::die parse_die(const sdb::compile_unit& cu, cursor cur) {
        const auto pos = &cur.position();
        const auto abbrev_code = cur.uleb128();
        if (abbrev_code == 0)
            return sdb::die {
                .compile_unit = &cu,
                .abbrev = std::nullopt,
                .debug_info_die_view = std::span {pos, &cur.position()},
                .next = cu.debug_info_view.subspan(std::distance(cu.debug_info_view.data(), &cur.position())),
                .form_values_start_locs = {}
            };

        auto& abbrev_table = cu.abbrev_table();
        const sdb::abbrev& abbrev = abbrev_table.at(abbrev_code);
        std::vector<const std::byte*> attr_locs;
        attr_locs.reserve(abbrev.attr_specs.size());
        for (const auto [attr, form]: abbrev.attr_specs) {
            attr_locs.push_back(&cur.position());
            cur.skip_form(form);
        }

        return sdb::die {
            .compile_unit = &cu,
            .abbrev = abbrev,
            .debug_info_die_view = std::span {pos, &cur.position()},
            .next = cu.debug_info_view.subspan(std::distance(cu.debug_info_view.data(), &cur.position())),
            .form_values_start_locs = std::move(attr_locs)
        };
    }
}

sdb::dwarf::dwarf(const elf &parent) : elf_(parent) {
    compile_units_ = parse_compile_units(*this, parent);
}

sdb::die sdb::compile_unit::root() const {
    constexpr static auto header_size = 11;
    return parse_die(*this, cursor(debug_info_view.subspan(header_size)));
}

const std::unordered_map<std::uint64_t, sdb::abbrev>& sdb::dwarf::get_abbrev_table(std::uint64_t offset) {
    if (!abbrev_tables_.contains(offset))
        abbrev_tables_.emplace(offset, parse_abbrev_table(elf_, offset));
    return abbrev_tables_.at(offset);
}

const std::unordered_map<std::uint64_t, sdb::abbrev>& sdb::compile_unit::abbrev_table() const {
    return dwarf_.get_abbrev_table(debug_abbrev_offset);
}

std::generator<std::span<const std::byte>> sdb::die::get_form_values() const {
    for (auto [first, second]: form_values_start_locs | std::views::adjacent<2>)
        co_yield std::span(first, second);

    if (!form_values_start_locs.empty())
        co_yield std::span(form_values_start_locs.back(), next.data());
}

std::generator<sdb::die> sdb::die::get_children_with_null() const {
    if (!has_children()) co_return;

    auto child_die = parse_die(*compile_unit, cursor(next));
    while (!child_die.is_null())
    {
        co_yield child_die;
        for (const auto grand_child: child_die.get_children())
            child_die = grand_child;

        child_die = parse_die(*compile_unit, cursor(child_die.next));
    }

    co_yield child_die;
}


std::generator<sdb::die> sdb::die::get_children() const {
    for (auto child_die: get_children_with_null())
        if (!child_die.is_null())
            co_yield child_die;
}
