#include <iostream>
#include <sstream>
#include <vector>
#include <print>
#include <string_view>
#include <editline/readline.h>
#include <libsdb/process.hpp>
#include <libsdb/parse.hpp>
#include <libsdb/error.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

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

    void print_help(const std::vector<std::string>& args) {
        if (args.size() == 1) {
            std::cout << R"(Available commands:
continue - Resume the process
register - Commands for operating on registers
help     - Display the help panel
exit     - Exits the debugger
)";
        }
        else if (is_prefix(args[1], "register")) {
            std::cout << R"(Available commands:
            read
            read <register>
            read all
            write <register> <value>
)";
        }
        else {
            std::cout << "No help available on that\n";
        }
    }

    void handle_register_read(sdb::process& process, const std::vector<std::string>& args) {
        auto format = [](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_floating_point_v<T>) {
                return std::format("{}", t);
            }
            else if constexpr (std::is_integral_v<T>) {
                return std::format("{:#0{}x}", t, sizeof(T) * 2 + 2);  // Integer → hex with "0x" prefix, padded width
            }
            else {
                return fmt::format("[{:#04x}]", fmt::join(t, ","));
            }
        };

        if (args.size() == 2 or (args.size() == 3 and args[2] == "all")) {
            for (auto& info : sdb::g_register_infos) {
                if (const auto should_print = (args.size() == 3 or info.type == sdb::register_type::gpr) and info.name != "orig_rax"; !should_print)
                    continue;

                auto value = process.get_registers().read(info);
                std::println("{:10}:\t{}", info.name, std::visit(format, value));
            }
        }
        else if (args.size() == 3) {
            try {
                auto info = sdb::register_info_by_name(args[2]);
                auto value = process.get_registers().read(info);
                std::println("{}:\t{}", info.name, std::visit(format, value));
            }
            catch (sdb::error& err) {
                std::cerr << "No such register\n";
            }
        }
        else {
            print_help({ "help", "register" });
        }
    }

    sdb::registers::value parse_register_value(sdb::register_info info, std::string_view text) {
        try {
            if (info.format == sdb::register_format::uint) {
                switch (info.size) {
                    case 1: return sdb::to_integral<std::uint8_t>(text, 16).value();
                    case 2: return sdb::to_integral<std::uint16_t>(text, 16).value();
                    case 4: return sdb::to_integral<std::uint32_t>(text, 16).value();
                    case 8: return sdb::to_integral<std::uint64_t>(text, 16).value();
                }
            }
            else if (info.format == sdb::register_format::double_float) {
                return sdb::to_float<double>(text).value();
            }
            else if (info.format == sdb::register_format::long_double) {
                return sdb::to_float<long double>(text).value();
            }
            else if (info.format == sdb::register_format::vector) {
                if (info.size == 8) {
                    return sdb::parse_vector<8>(text);
                }
                else if (info.size == 16) {
                    return sdb::parse_vector<16>(text);
                }
            }
        } catch (...) {

        }

        sdb::error::send("Invalid format");
    }

    void handle_register_write(sdb::process& process, const std::vector<std::string>& args) {
        if (args.size() != 4) {
            print_help({ "help", "register" });
            return;
        }

        try {
            auto info = sdb::register_info_by_name(args[2]);
            auto value = parse_register_value(info, args[3]);
            process.get_registers().write(info, value);
        }
        catch (sdb::error& err) {
            std::cerr << err.what() << '\n';
            return;
        }
    }

    void handle_register_command(sdb::process& process, const std::vector<std::string>& args) {
        if (args.size() < 2) {
            print_help({ "help", "register" });
            return;
        }

        if (is_prefix(args[1], "read"))
            handle_register_read(process, args);
        else if (is_prefix(args[1], "write"))
            handle_register_write(process, args);
        else
            print_help({ "help", "register" });
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
        else if (is_prefix(command, "help")) {
            print_help(args);
            return true;
        }
        else if (is_prefix(command, "register")) {
            handle_register_command(*process, args);
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
    std::vector<std::string> arr(argc);
    for (int i = 0; i < argc; ++i)
        arr[i] = std::string{ argv[i] };

    try {
        main_loop(attach(arr));
    } catch (const sdb::error& e) {
        std::println("{}", e.what());
    }
}
