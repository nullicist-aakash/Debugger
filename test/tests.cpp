#include <catch2/catch_test_macros.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <libsdb/bit.hpp>
#include <sys/types.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <format>


namespace {
    bool process_exists(pid_t pid) {
        auto ret = kill(pid, 0);
        return ret != -1 && errno != ESRCH;
    }

    char get_process_status(pid_t pid) {
        std::ifstream stat(std::format("/proc/{}/stat", pid));
        std::string line;
        std::getline(stat, line);
        auto last_paren_index = line.rfind(')');
        return line[last_paren_index + 2];
    }
}

TEST_CASE("process::launch success", "[process]") {
    auto proc = sdb::process::launch("yes");
    REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("process::launch could not execute", "[process]") {
    REQUIRE_THROWS_AS(sdb::process::launch("some_random_non_existent_program"), sdb::error);
}

TEST_CASE("process::attach success", "[process]") {
    auto target = sdb::process::launch("targets/run_endlessly", false);
    auto proc = sdb::process::attach(target->pid());
    REQUIRE(get_process_status(target->pid()) == 't');  // Process is stopped due to tracing
}

TEST_CASE("process::attach invalid PID", "[process]") {
    REQUIRE_THROWS_AS(sdb::process::attach(0), sdb::error);
}

TEST_CASE("process::resume success", "[process]") {
    {
        auto proc = sdb::process::launch("targets/run_endlessly");
        proc->resume();
        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }
    {
        auto target = sdb::process::launch("targets/run_endlessly", false);
        auto proc = sdb::process::attach(target->pid());
        proc->resume();
        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }
}


TEST_CASE("process::resume already terminated", "[process]") {
    auto proc = sdb::process::launch("targets/end_immediately");
    proc->resume();
    proc->wait_on_signal();
    REQUIRE_THROWS_AS(proc->resume(), sdb::error);
}

TEST_CASE("Write register works", "[register]") {
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);

    auto proc = sdb::process::launch("targets/reg_write", true, channel.get_write_fd());
    channel.close_write();

    proc->resume();
    proc->wait_on_signal();

    auto& regs = proc->get_registers();
    regs.write_by_id(sdb::register_id::rsi, 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    auto output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "0xcafecafe");

    regs.write_by_id(sdb::register_id::mm0, 0xba5eba11);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "0xba5eba11");

    regs.write_by_id(sdb::register_id::xmm0, 42.42);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "42.42");

    // Write to floating point stack
    regs.write_by_id(sdb::register_id::st0, 42.42l);
    // Set the size of stack to 1
    regs.write_by_id(sdb::register_id::fsw, std::uint16_t(0b0011'1000'0000'0000));
    // Set the tag to specify that st0 contains some value
    regs.write_by_id(sdb::register_id::ftw, std::uint16_t(0b0011'1111'1111'1111));

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(sdb::to_string_view(output) == "42.42");
}
