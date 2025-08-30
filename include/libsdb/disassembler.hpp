#pragma once

#include <libsdb/process.hpp>
#include <optional>

namespace sdb {
    class disassembler {
        struct instruction {
            virt_addr address;
            std::string text;
        };

    public:
        explicit disassembler(process& proc) : m_process(&proc) {}

        /**
         * Method to read instructions from the attached process.
         * @param n_instructions Number of instructions to read.
         * @param address The starting address to read instructions from. Defaults to PC.
         * @return The read instructions.
         */
        std::vector<instruction> disassemble(std::size_t n_instructions, std::optional<virt_addr> address = std::nullopt);

    private:
        process* m_process;
    };
}