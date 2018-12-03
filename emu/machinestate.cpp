#include <stack>
#include <bitset>
#include <iostream>

#include "common.h"
#include "machinestate.h"
#include "opcodes.h"

machine_state::machine_state()
{
    m_memory_size = size_t(std::pow(int32_t(2), int32_t(24)));
    m_memory = (uint8_t*)::malloc(m_memory_size);
    IF_FALSE_THROW(m_memory != nullptr, "Allocation failed");
    ::memset(&m_registers, 0x0, sizeof(m_registers));

    make_opcode_table(m_opcode_table);
}

machine_state::~machine_state()
{
    if (m_memory)
    {
        ::free(m_memory);
    }
}

void machine_state::load_program(size_t memory_offset, void* program, size_t program_size, uint32_t init_pc)
{
    ::memset(&m_registers, 0x0, sizeof(m_registers));
    ::memcpy(&m_memory[memory_offset], program, program_size);
    m_registers.PC = init_pc;
}

void machine_state::tick()
{
    auto opcode = next<uint16_t>();
    auto inst_func = m_opcode_table[opcode];
    IF_FALSE_THROW(inst_func != nullptr, "Invalid or unimplemented opcode: 0x" << std::hex << opcode << std::dec << " (" << std::bitset<16>(opcode) << ")");
    inst_func(*this, opcode);
}