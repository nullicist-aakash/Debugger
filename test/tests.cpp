#include <catch2/catch_test_macros.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
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
