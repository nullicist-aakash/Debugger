#pragma once
#include <cstring>
#include <string>
#include <stdexcept>
#include <libsdb/pipe.hpp>

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

        /**
         * Writes the exception message to the pipe and exits via `exit(-1)`.
         * @param channel Pipe where the exception message will be stored.
         * @param prefix Prefix string to use for an errno error description.
         */
        [[noreturn]] static void exit_with_errno(sdb::pipe& channel, const std::string& prefix) {
            auto message = prefix + ": " + std::strerror(errno);

            std::span byte_span{
                reinterpret_cast<std::byte*>(message.data()),
                message.size()
            };

            channel.write(byte_span);
            exit(-1);
        }

    private:
        explicit error(const std::string& error_str) : std::runtime_error(error_str) {}
    };
}