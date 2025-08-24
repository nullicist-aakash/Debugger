#pragma once
#include <cstddef>
#include <array>

namespace sdb {
    using byte64 = std::array<std::byte, 8>;
    using byte128 = std::array<std::byte, 16>;

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

    private:
        std::uint64_t addr_ = 0;
    };
}