#include <iostream>
#include <sstream>
#include <vector>
#include <print>
#include <stack>
#include <string_view>
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
        std::print(std::cerr, "Process {} ", process.pid());
        switch (reason.reason) {
            case sdb::process_state::STOPPED:
                std::println(std::cerr, "stopped with signal {} at {:#x}", sigabbrev_np(reason.info), process.get_pc().addr());
                break;

            case sdb::process_state::TERMINATED:
                std::println(std::cerr, "terminated with signal {}", sigabbrev_np(reason.info));
                break;

            case sdb::process_state::EXITED:
                std::println(std::cerr, "exited with exit status {}", reason.info);
                break;

            default:
                break;
        }
    }

    void print_help(const std::vector<std::string>& args) {
        if (args.size() == 1) {
            std::cout << R"(Available commands:
breakpoint - Command for operating on breakpoints
continue   - Resume the process
register   - Commands for operating on registers
step       - Step over a single instruction
help       - Display the help panel
exit       - Exits the debugger
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
        else if (is_prefix(args[1], "breakpoint")) {
            std::cout << R"(Available commands:
            list
            delete <id>
            disable <id>
            enable <id>
            set <address>
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
                if (info.size == 16) {
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


    void handle_breakpoint_command(sdb::process& process, const std::vector<std::string>& args) {
        if (args.size() < 2) {
            print_help({ "help", "breakpoint" });
            return;
        }

        const auto& command = args[1];

        if (is_prefix(command, "list")) {
            if (process.breakpoint_sites().empty()) {
                std::println("No breakpoints set!");
                return;
            }

            std::println("Current breakpoints:");
            process.breakpoint_sites().for_each([](auto &site) {
                std::println("{}: address = {:#x}, {}", site.id(), site.address().addr(), site.is_enabled() ? "enabled" : "disabled");
            });
            return;
        }

        if (args.size() < 3) {
            print_help({ "help", "breakpoint" });
            return;
        }

        if (is_prefix(command, "set")) {
            auto address = sdb::to_integral<std::uint64_t>(args[2], 16);

            if (!address) {
                std::println(std::cerr, "Breakpoint command expects address in hexadecimal format, prefixed with 0x");
                return;
            }

            process.create_breakpoint_site(sdb::virt_addr{*address}).enable();
            return;
        }

        auto id = sdb::to_integral<sdb::breakpoint_site::id_type>(args[2]);
        if (!id) {
            std::println(std::cerr, "Command expects breakpoint id");
            return;
        }

        if (is_prefix(command, "enable"))
            process.breakpoint_sites().get_by_id(*id).enable();
        else if (is_prefix(command, "disable"))
            process.breakpoint_sites().get_by_id(*id).disable();
        else if (is_prefix(command, "delete"))
            process.breakpoint_sites().remove_by_id(*id);
    }

    std::unique_ptr<sdb::process> attach(const std::vector<std::string>& args) {
        if (args[0] == "-p")
            return sdb::process::attach(std::stoi(args[1]));

        return sdb::process::launch(args[0]);
    }

    void handle_command(const std::unique_ptr<sdb::process>& process, std::string_view line) {
        const auto args = split(line, ' ');
        const auto& command = args[0];

        if (is_prefix(command, "continue")) {
            process->resume();
            auto reason = process->wait_on_signal();
            print_stop_reason(*process, reason);
        }
        else if (is_prefix(command, "help")) {
            print_help(args);
        }
        else if (is_prefix(command, "register")) {
            handle_register_command(*process, args);
        }
        else if (is_prefix(command, "breakpoint")) {
            handle_breakpoint_command(*process, args);
        }
        else if (is_prefix(command, "step")) {
            auto reason = process->step_instruction();
            print_stop_reason(*process, reason);
        }
        else
            sdb::error::send(std::format("Unknown command: {}", command));
    }

    void main_loop(const std::unique_ptr<sdb::process> &process) {
        std::println("Launch process with PID {}", process->pid());

        while (true) {
            std::print("sdb> ");
            std::string line_str;
            std::getline(std::cin >> std::ws, line_str);

            if (line_str.empty())
                continue;

            if (is_prefix(line_str, "exit"))
                break;

            try {
                handle_command(process, line_str);
            } catch (const sdb::error& e) {
                std::println(std::cerr, "{}", e.what());
            }
        }
    }
}

int main(int argc, const char** argv) {
    std::vector<std::string> arr(argc - 1);
    for (int i = 1; i < argc; ++i)
        arr[i - 1] = std::string{ argv[i] };

    try {
        main_loop(attach(arr));
    } catch (const sdb::error& e) {
        std::println(std::cerr, "{}", e.what());
    }
}
