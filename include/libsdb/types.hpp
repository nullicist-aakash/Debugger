#pragma once
#include <cstddef>
#include <array>
#include <cstdint>
#include <optional>

namespace sdb {
    using byte64 = std::array<std::byte, 8>;
    using byte128 = std::array<std::byte, 16>;

    struct file_addr;
    struct elf;

    class virt_addr {
    public:
        constexpr virt_addr() = default;

        explicit constexpr virt_addr(std::uint64_t addr) : addr_(addr) {}

        [[nodiscard]] constexpr std::uint64_t addr() const noexcept {
            return addr_;
        }

        constexpr virt_addr operator+(std::int64_t offset) const noexcept {
            return virt_addr(addr_ + offset);
        }

        constexpr virt_addr operator-(std::int64_t offset) const noexcept {
            return virt_addr(addr_ - offset);
        }

        constexpr virt_addr& operator+=(std::int64_t offset) noexcept {
            addr_ += offset;
            return *this;
        }

        constexpr virt_addr& operator-=(std::int64_t offset) noexcept {
            addr_ -= offset;
            return *this;
        }

        constexpr bool operator==(const virt_addr& other) const noexcept {
            return addr_ == other.addr_;
        }

        constexpr bool operator!=(const virt_addr& other) const noexcept {
            return addr_ != other.addr_;
        }

        constexpr bool operator<(const virt_addr& other) const noexcept {
            return addr_ < other.addr_;
        }

        constexpr bool operator<=(const virt_addr& other) const noexcept {
            return addr_ <= other.addr_;
        }

        constexpr bool operator>(const virt_addr& other) const noexcept {
            return addr_ > other.addr_;
        }

        constexpr bool operator>=(const virt_addr& other) const noexcept {
            return addr_ >= other.addr_;
        }

        std::optional<file_addr> to_file_addr(const elf& obj) const;
    private:
        std::uint64_t addr_ = 0;
    };

    struct file_addr {
        constexpr file_addr() = default;
        constexpr file_addr(const elf& obj, std::uint64_t addr) : elf_(&obj), addr_(addr) {}

        constexpr std::uint64_t addr() const { return addr_; }
        constexpr std::optional<const elf&> elf_file() const {
            if (elf_ == nullptr) return std::nullopt;
            return *elf_;
        }

        std::optional<virt_addr> to_virt_addr() const;

        constexpr file_addr operator+(std::int64_t offset) const {
            return file_addr(*elf_, addr_ + offset);
        }

        constexpr file_addr operator-(std::int64_t offset) const {
            return file_addr(*elf_, addr_ - offset);
        }

        constexpr file_addr& operator+=(std::int64_t offset) {
            addr_ += offset;
            return *this;
        }

        constexpr file_addr& operator-=(std::int64_t offset) {
            addr_ -= offset;
            return *this;
        }

        constexpr bool operator==(const file_addr& other) const {
            return addr_ == other.addr_ and elf_ == other.elf_;
        }

        constexpr bool operator!=(const file_addr& other) const {
            return addr_ != other.addr_ or elf_ != other.elf_;
        }

        constexpr bool operator<(const file_addr& other) const {
            contract_assert(elf_ == other.elf_);
            return addr_ < other.addr_;
        }

        constexpr bool operator<=(const file_addr& other) const {
            contract_assert(elf_ == other.elf_);
            return addr_ <= other.addr_;
        }

        constexpr bool operator>(const file_addr& other) const {
            contract_assert(elf_ == other.elf_);
            return addr_ > other.addr_;
        }

        constexpr bool operator>=(const file_addr& other) const {
            contract_assert(elf_ == other.elf_);
            return addr_ >= other.addr_;
        }

    private:
        const elf* elf_ = nullptr;
        std::uint64_t addr_ = 0;
    };

    struct file_offset {
        constexpr file_offset() = default;
        constexpr file_offset(const elf& obj, std::uint64_t off) : elf_(&obj), off_(off) {}
        constexpr std::uint64_t off() const { return off_; }
        constexpr const elf* elf_file() const { return elf_; }

    private:
        const elf* elf_ = nullptr;
        std::uint64_t off_ = 0;
    };

    enum class stoppoint_mode {
        WRITE,
        READ_WRITE,
        EXECUTE
    };
}