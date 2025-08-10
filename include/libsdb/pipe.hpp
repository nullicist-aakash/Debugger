#pragma once
#include <array>
#include <vector>
#include <span>
#include <cstddef>

namespace sdb {
    class pipe {
    public:
        /**
         * Constructor.
         * @param close_on_exec Sets the close-on-exec flag for the file descriptor, which causes the file descriptor
         * to be automatically (and atomically) closed when any of the exec-family functions succeed.
         */
        explicit pipe(bool close_on_exec);

        /**
         * Returns the fd for read end.
         */
        int get_read_fd() const noexcept { return fds.at(read_fd_idx); }
        /**
         * Returns the fd for write end.
         */
        int get_write_fd() const noexcept { return fds.at(write_fd_idx); }

        /**
         * Close the read end. no-op if already closed.
         */
        void close_read() noexcept;

        /**
         * Close the write end. no-op if already closed.
         */
        void close_write() noexcept;

        /**
         * Read the existing data in pipe.
         * @return The data currently present in pipe (and cleans the pipe).
         */
        [[nodiscard]] std::vector<std::byte> read();

        /**
         * Writes the data to byte.
         * @param data Data to write to pipe.
         */
        void write(std::span<std::byte> data);

        /**
         * Destructor.
         */
        ~pipe();

    private:
        // Helper constants
        static constexpr auto read_fd_idx = 0;
        static constexpr auto write_fd_idx = 1;

        // fds
        std::array<int, 2> fds;
    };
}
