#pragma once

#include <memory>
#include <libsdb/elf.hpp>
#include <libsdb/process.hpp>

namespace sdb {
    class target {
    public:
        target() = delete;

        target(const target&) = delete;
        target& operator=(const target&) = delete;

        static std::unique_ptr<target> launch(std::filesystem::path path, std::optional<int> stdout_replacement = std::nullopt);
        static std::unique_ptr<target> attach(pid_t pid);

        decltype(auto) get_process(this auto&& self) { return *self.process_; }
        decltype(auto) get_elf(this auto&& self) { return *self.elf_; }
    private:
        target(std::unique_ptr<process> proc, std::unique_ptr<elf> obj) : process_(std::move(proc)), elf_(std::move(obj)) {}
        std::unique_ptr<process> process_;
        std::unique_ptr<elf> elf_;
    };
}
