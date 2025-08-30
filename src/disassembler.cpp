#include <libsdb/disassembler.hpp>
#include <Zydis/Zydis.h>

std::vector<sdb::disassembler::instruction> sdb::disassembler::disassemble(std::size_t n_instructions, std::optional<virt_addr> address) {
    std::vector<instruction> ret;
    ret.reserve(n_instructions);
    auto _address = address.value_or(m_process->get_pc());

    // The largest x64 instruction is 15 bytes, so if we
    // read n_instructions * 15, we’re guaranteed to have enough memory to disassemble n_instructions,
    // so long as there are that many instructions left in memory
    auto code = m_process->read_memory_without_traps(_address, n_instructions * 15);

    ZyanUSize offset = 0;   // Specifies the offset from the beginning of read memory contents.
    ZydisDisassembledInstruction instr;

    while (ZYAN_SUCCESS(ZydisDisassembleATT(
        ZYDIS_MACHINE_MODE_LONG_64,
        _address.addr(),
        code.data() + offset,
        code.size() - offset,
        &instr
        )) and n_instructions > 0)
    {
        ret.push_back(instruction{ _address, std::string(instr.text) });
        offset += instr.info.length;
        _address += instr.info.length;
        --n_instructions;
    }
    return ret;
}
