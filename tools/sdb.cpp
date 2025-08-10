#include <iostream>
#include <sstream>
#include <vector>
#include <print>
#include <string_view>
#include <editline/readline.h>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

using namespace std::string_view_literals;

namespace {
    std::vector<std::string> split(std::string_view str, char delim) {
        std::vector<std::string> output;
        std::stringstream sstr{ std::string{ str } };
        std::string item;

        while (std::getline(sstr, item, delim)) {
            output.push_back(std::move(item));
        }
        return output;
    }

    /**
     * Checks if `str` is a prefix of `of`.
     *
     * @param str The potential prefix string.
     * @param of The string to check against.
     * @return true if `str` is a prefix of `of`, false otherwise.
     */
    bool is_prefix(std::string_view str, std::string_view of) {
        if (str.size() > of.size()) return false;

        return std::equal(str.begin(), str.end(), of.begin());
    }
}

namespace {
    void print_stop_reason(const sdb::process& process, const sdb::stop_reason& reason) {
        std::print("Process {} ", process.pid());
        switch (reason.reason) {
            case sdb::process_state::STOPPED:
                std::println("stopped with signal {}", sigabbrev_np(reason.info));
                break;

            case sdb::process_state::TERMINATED:
                std::println("terminated with signal {}", sigabbrev_np(reason.info));
                break;

            case sdb::process_state::EXITED:
                std::println("exited with exit status {}", reason.info);
                break;

            default:
                break;
        }
    }

    std::unique_ptr<sdb::process> attach(const std::vector<std::string>& args) {
        if (args[0] == "-p")
            return sdb::process::attach(std::stoi(args[1]));

        return sdb::process::launch(args[0]);
    }

    bool handle_command(const std::unique_ptr<sdb::process>& process, std::string_view line) {
        const auto args = split(line, ' ');
        const auto& command = args[0];

        if (is_prefix(command, "continue")) {
            process->resume();
            auto reason = process->wait_on_signal();
            print_stop_reason(*process, reason);
            return true;
        }

        if (is_prefix(command, "exit"))
            return false;

        std::println("Unknown command: {}", command);
        return true;
    }

    void main_loop(const std::unique_ptr<sdb::process> &process) {
        char* line = nullptr;
        std::println("Started process {} ", process->pid());

        while ((line = readline("sdb> ")) != nullptr) {
            std::string line_str;

            if (line != ""sv)
                add_history(line);

            free(line);

            if (history_length > 0)
                line_str = history_list()[history_length - 1]->line;

            if (line_str.empty())
                continue;

            try {
                if (!handle_command(process, line_str))
                    return;
            } catch (const sdb::error& e) {
                std::println("{}", e.what());
            }
        }
    }
}

int main(int argc, const char** argv) {
    try {
        main_loop(attach({"ls"}));
    } catch (const sdb::error& e) {
        std::println("{}", e.what());
    }
}
