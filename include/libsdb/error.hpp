#pragma once
#include <cstring>
#include <string>
#include <stdexcept>

namespace sdb {
    /**
     * sdb-specific error type that we can use to differentiate our errors from system errors
     * that we need to handle.
     */
    class error final : public std::runtime_error {
    public:
        /**
         * Throws an error with user message.
         * @param what The message to use as the error description.
         */
        [[noreturn]] static void send(const std::string& what) {
            throw error(what);
        }

        /**
         * Throws an error with strerror description.
         * @param prefix Prefix string to use for an errno error description.
         */
        [[noreturn]] static void send_errno(const std::string& prefix) {
            throw error(prefix + ": " + std::strerror(errno));
        }

    private:
        explicit error(const std::string& error_str) : std::runtime_error(error_str) {}
    };
}