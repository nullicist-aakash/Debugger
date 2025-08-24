#pragma once

#include <vector>
#include <memory>
#include <libsdb/types.hpp>

namespace sdb {
    template <class StopPoint>
    class stoppoint_collection {
        using id_type = typename StopPoint::id_type;
    public:
        /**
         * Adds a new stoppoint to the collection.
         * @param point The point to add to collection.
         * @return Reference to the point that was just added.
         */
        StopPoint& push(std::unique_ptr<StopPoint> point) {
            m_stoppoints.push_back(std::move(point));
            return *m_stoppoints.back();
        }

        [[nodiscard]] constexpr bool contains_id(const id_type& id) const noexcept {
            return find_by_id(id) != std::end(m_stoppoints);
        }

        [[nodiscard]] constexpr bool contains_address(const virt_addr& address) const noexcept {
            return find_by_address(address) != std::end(m_stoppoints);
        }

        [[nodiscard]] constexpr bool enabled_stoppoint_at_address(const virt_addr& address) const noexcept {
            return contains_address(address) && get_by_address(address).is_enabled();
        }

        void remove_by_id(const id_type& id) {
            auto it = find_by_id(id);
            if (it == std::end(m_stoppoints))
                error::send("Invalid stoppoint id");
            (**it).disable();
            m_stoppoints.erase(it);
        }

        void remove_by_address(const virt_addr& address) {
            auto it = find_by_address(address);
            if (it == std::end(m_stoppoints))
                error::send("Stoppoint doesn't exists at given address");
            (**it).disable();
            m_stoppoints.erase(it);
        }

        template <typename Self>
        auto& get_by_id(this Self&& self, const id_type& id) {
            auto it = std::forward<Self>(self).find_by_id(id);
            if (it == std::end(std::forward<Self>(self).m_stoppoints))
                error::send("Invalid stoppoint id");
            return **it;
        }

        template <typename Self>
        auto& get_by_address(this Self&& self, const virt_addr& address) {
            auto it = std::forward<Self>(self).find_by_address(address);
            if (it == std::end(std::forward<Self>(self).m_stoppoints))
                error::send("Stoppoint doesn't exists at given address");
            return **it;
        }

        template <typename Self, class F>
        void for_each(this Self&& self, F f) {
            for (auto &point: std::forward<Self>(self).m_stoppoints)
                f(*point);
        }

    private:
        using points_t = std::vector<std::unique_ptr<StopPoint>>;

        template <typename Self>
        typename points_t::iterator find_by_id(this Self&& self, const id_type& id) {
            auto begin = std::begin(std::forward<Self>(self).m_stoppoints);
            auto end = std::end(std::forward<Self>(self).m_stoppoints);

            return std::find_if(begin, end, [id](const auto& point) {
                return point->id() == id;
            });
        }

        template <typename Self>
        typename points_t::iterator find_by_address(this Self&& self, const virt_addr& address) {
            auto begin = std::begin(std::forward<Self>(self).m_stoppoints);
            auto end = std::end(std::forward<Self>(self).m_stoppoints);

            return std::find_if(begin, end, [address](const auto& point) {
                return point->address() == address;
            });
        }

        points_t m_stoppoints;
    };
}