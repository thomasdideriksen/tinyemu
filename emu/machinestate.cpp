#include <stack>
#include <bitset>
#include <iostream>

#include "common.h"
#include "machinestate.h"
#include "opcodes.h"

machine_state::machine_state()
    : m_storage_index(0)
{
    m_memory_size = size_t(std::pow(int32_t(2), int32_t(24)));
    m_memory = (uint8_t*)::malloc(m_memory_size);
    IF_FALSE_THROW(m_memory != nullptr, "Allocation failed");
    ::memset(m_memory, 0x0, m_memory_size);
    ::memset(&m_registers, 0x0, sizeof(m_registers));
    ::memset(m_storage, 0x0, sizeof(m_storage));

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
    set_program_counter(init_pc);
    set_status_bit<bit::supervisor>(true); // Initialize the CPU in supervisor mode
}

void machine_state::tick()
{
    auto opcode = next<uint16_t>();
    auto inst_func = m_opcode_table[opcode];
    IF_FALSE_THROW(inst_func != nullptr, "Invalid or unimplemented opcode: 0x" << std::hex << opcode << std::dec << " (" << std::bitset<16>(opcode) << ")");
    inst_func(*this, opcode);
}

void machine_state::set_program_counter(uint32_t value)
{
    IF_FALSE_THROW(value < m_memory_size, "Invalid program counter value: " << value);
    m_registers.PC = value;
}

void machine_state::offset_program_counter(int32_t offset)
{
    int64_t pc = int64_t(m_registers.PC);
    pc += int64_t(offset);
    IF_FALSE_THROW(pc < 0 || pc > int64_t(m_memory_size), "Invalid program counter offset value: " << offset);
    m_registers.PC = uint32_t(pc);
}

void machine_state::push_program_counter()
{
    push<uint32_t>(m_registers.PC);
}

void machine_state::pop_program_counter()
{
    m_registers.PC = pop<uint32_t>();
}

void machine_state::push_status_register()
{
    push<uint16_t>(m_registers.SR);
}

void machine_state::pop_status_register()
{
    m_registers.SR = pop<uint16_t>();
}

void machine_state::exception(uint32_t vector_index)
{
    push_program_counter();
    push_status_register();
    set_status_bit<bit::supervisor>(true);

    uint32_t* vector_table = (uint32_t*)m_memory;
    uint32_t vector_offset = read(vector_table + vector_index);
    
    set_program_counter(vector_offset);
}

void machine_state::reset()
{
    // TODO
    THROW("Reset not implemented");
}

void machine_state::stop()
{
    // TODO
    THROW("Stop not implemented");
}

void machine_state::set_condition_code_register(uint8_t ccr)
{
    m_registers.SR = (m_registers.SR & 0xff00) | uint16_t(ccr);
}
