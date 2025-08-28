#include <catch2/catch_test_macros.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <libsdb/bit.hpp>
#include <sys/types.h>
#include <signal.h>
#include <iostream>
#include <fstream>
#include <format>

using namespace sdb;

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
    auto proc = process::launch("yes");
    REQUIRE(process_exists(proc->pid()));
}

TEST_CASE("process::launch could not execute", "[process]") {
    REQUIRE_THROWS_AS(process::launch("some_random_non_existent_program"), error);
}

TEST_CASE("process::attach success", "[process]") {
    auto target = process::launch("targets/run_endlessly", false);
    auto proc = process::attach(target->pid());
    REQUIRE(get_process_status(target->pid()) == 't');  // Process is stopped due to tracing
}

TEST_CASE("process::attach invalid PID", "[process]") {
    REQUIRE_THROWS_AS(process::attach(0), error);
}

TEST_CASE("process::resume success", "[process]") {
    {
        auto proc = process::launch("targets/run_endlessly");
        proc->resume();
        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }
    {
        auto target = process::launch("targets/run_endlessly", false);
        auto proc = process::attach(target->pid());
        proc->resume();
        auto status = get_process_status(proc->pid());
        auto success = status == 'R' or status == 'S';
        REQUIRE(success);
    }
}


TEST_CASE("process::resume already terminated", "[process]") {
    auto proc = process::launch("targets/end_immediately");
    proc->resume();
    proc->wait_on_signal();
    REQUIRE_THROWS_AS(proc->resume(), error);
}

TEST_CASE("Write register works", "[register]") {
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);

    auto proc = process::launch("targets/reg_write", true, channel.get_write_fd());
    channel.close_write();

    proc->resume();
    proc->wait_on_signal();

    auto& regs = proc->get_registers();
    regs.write_by_id(register_id::rsi, 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    auto output = channel.read();
    REQUIRE(to_string_view(output) == "0xcafecafe");

    regs.write_by_id(register_id::mm0, 0xba5eba11);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "0xba5eba11");

    regs.write_by_id(register_id::xmm0, 42.42);

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.42");

    // Write to floating point stack
    regs.write_by_id(register_id::st0, 42.42l);
    // Set the size of stack to 1
    regs.write_by_id(register_id::fsw, std::uint16_t(0b0011'1000'0000'0000));
    // Set the tag to specify that st0 contains some value
    regs.write_by_id(register_id::ftw, std::uint16_t(0b0011'1111'1111'1111));

    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.42");
}

TEST_CASE("Read register works", "[register]") {
    auto proc = process::launch("targets/reg_read");
    auto& regs = proc->get_registers();

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint64_t>(register_id::r13) == 0xcafecafe);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint8_t>(register_id::r13b) == 42);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<byte64>(register_id::mm0) == to_byte64(0xba5eba11ull));

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<byte128>(register_id::xmm0) == to_byte128(64.125));

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<long double>(register_id::st0) == 64.125L);
}

TEST_CASE("Can create breakpoint site", "[breakpoint]") {
    auto proc = process::launch("targets/run_endlessly");
    auto& site = proc->create_breakpoint_site(virt_addr{ 42 });
    REQUIRE(site.address().addr() == 42);
}

TEST_CASE("Breakpoint site ids increase", "[breakpoint]") {
    auto proc = process::launch("targets/run_endlessly");

    auto& s1 = proc->create_breakpoint_site(virt_addr{ 42 });
    REQUIRE(s1.address().addr() == 42);

    auto& s2 = proc->create_breakpoint_site(virt_addr{ 43 });
    REQUIRE(s2.id() == s1.id() + 1);

    auto& s3 = proc->create_breakpoint_site(virt_addr{ 44 });
    REQUIRE(s3.id() == s1.id() + 2);

    auto& s4 = proc->create_breakpoint_site(virt_addr{ 45 });
    REQUIRE(s4.id() == s1.id() + 3);
}

TEST_CASE("Can find breakpoint site", "[breakpoint]") {
    auto proc = process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    proc->create_breakpoint_site(virt_addr{ 42 });
    proc->create_breakpoint_site(virt_addr{ 43 });
    proc->create_breakpoint_site(virt_addr{ 44 });
    proc->create_breakpoint_site(virt_addr{ 45 });

    auto& s1 = proc->breakpoint_sites().get_by_address(virt_addr{ 44 });
    REQUIRE(proc->breakpoint_sites().contains_address(virt_addr{ 44 }));
    REQUIRE(s1.address().addr() == 44);

    auto& cs1 = cproc->breakpoint_sites().get_by_address(virt_addr{ 44 });
    REQUIRE(cproc->breakpoint_sites().contains_address(virt_addr{ 44 }));
    REQUIRE(cs1.address().addr() == 44);

    auto& s2 = proc->breakpoint_sites().get_by_id(s1.id() + 1);
    REQUIRE(proc->breakpoint_sites().contains_id(s1.id() + 1));
    REQUIRE(s2.id() == s1.id() + 1);
    REQUIRE(s2.address().addr() == 45);

    auto& cs2 = proc->breakpoint_sites().get_by_id(cs1.id() + 1);
    REQUIRE(cproc->breakpoint_sites().contains_id(cs1.id() + 1));
    REQUIRE(cs2.id() == cs1.id() + 1);
    REQUIRE(cs2.address().addr() == 45);
}

TEST_CASE("Cannot find breakpoint site", "[breakpoint]") {
    auto proc = process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    REQUIRE_THROWS_AS(proc->breakpoint_sites().get_by_address(virt_addr{ 44 }), error);
    REQUIRE_THROWS_AS(proc->breakpoint_sites().get_by_id(44), error);
    REQUIRE_THROWS_AS(cproc->breakpoint_sites().get_by_address(virt_addr{ 44 }), error);
    REQUIRE_THROWS_AS(cproc->breakpoint_sites().get_by_id(44), error);
}

TEST_CASE("Breakpoint site list size and emptiness", "[breakpoint]") {
    auto proc = process::launch("targets/run_endlessly");

    const auto& cproc = proc;
    REQUIRE(proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 0);
    REQUIRE(cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 0);

    proc->create_breakpoint_site(virt_addr{ 42 });
    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 1);
    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 1);

    proc->create_breakpoint_site(virt_addr{ 43 });
    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 2);
    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 2);
}

TEST_CASE("Can iterate breakpoint sites", "[breakpoint]") {
    auto proc = process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    proc->create_breakpoint_site(virt_addr{ 42 });
    proc->create_breakpoint_site(virt_addr{ 43 });
    proc->create_breakpoint_site(virt_addr{ 44 });
    proc->create_breakpoint_site(virt_addr{ 45 });

    proc->breakpoint_sites().for_each(
        [addr = 42](auto& site) mutable {
        REQUIRE(site.address().addr() == addr++);
    });

    cproc->breakpoint_sites().for_each(
        [addr = 42](auto& site) mutable {
        REQUIRE(site.address().addr() == addr++);
        });
}