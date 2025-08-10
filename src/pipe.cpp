#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <fcntl.h>
#include <unistd.h>

static_assert(sizeof(char) == sizeof(std::byte), "Size of char and std::byte are different");

sdb::pipe::pipe(bool close_on_exec) {
    if (pipe2(fds.data(), close_on_exec ? O_CLOEXEC : 0) < 0)
        error::send_errno("Pipe creation failed");
}

void sdb::pipe::close_read() noexcept {
    if (fds.at(read_fd_idx) == -1)
        return;

    close(fds[read_fd_idx]);
    fds[read_fd_idx] = -1;
}

void sdb::pipe::close_write() noexcept {
    if (fds.at(write_fd_idx) == -1)
        return;

    close(fds[write_fd_idx]);
    fds[write_fd_idx] = -1;
}

std::vector<std::byte> sdb::pipe::read() {
    char buff[1024];
    int n_read = ::read(fds.at(read_fd_idx), buff, sizeof(buff));
    if (n_read < 0)
        error::send_errno("Could not read from pipe");

    auto bytes = reinterpret_cast<std::byte*>(buff);
    std::vector<std::byte> data;
    data.reserve(n_read);
    data.insert(data.end(), bytes, bytes + n_read);
    return data;
}

void sdb::pipe::write(std::span<std::byte> data) {
    if (::write(fds.at(write_fd_idx), data.data(), data.size()) < 0)
        error::send_errno("Could not write to pipe");
}


sdb::pipe::~pipe() {
    close_read();
    close_write();
}
