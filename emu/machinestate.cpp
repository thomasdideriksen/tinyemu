#include <stack>
#include <bitset>
#include <iostream>

#include "common.h"
#include "machinestate.h"

extern std::vector<opcode_desc_t> g_opcode_descs;

void machine_state::populate_lut(const opcode_desc_t& desc)
{
    struct item_t
    {
        size_t part_index;
        uint32_t bit_index;
        uint16_t value;
    };

    std::stack<item_t> items;
    items.push({ 0 });

    while (!items.empty())
    {
        const auto item = items.top();
        items.pop();

        if (item.part_index >= desc.parts.size())
        {
            inst_func_ptr_t* table = m_opcode_lut.data();
            IF_FALSE_THROW(item.bit_index == 16, "Invalid bit count in [" << desc.mnemonic << "] opcode description");
            IF_FALSE_THROW(table[item.value] == nullptr, "Cannot assign opcode [" << desc.mnemonic << "] to table slot 0x" << std::hex << item.value << std::dec << " (" << std::bitset<16>(item.value) << ") since it has already been assigned");
            table[item.value] = desc.func_ptr;
            continue;
        }

        const auto& part = desc.parts[item.part_index];
        const uint32_t max_value_plus_one = uint32_t(std::pow(uint32_t(2), part.bit_count));
        
        // An empty list means ALL possible values
        auto possible_values = part.possible_values;
        if (possible_values.empty())
        {
            // Generate all possible values
            for (uint16_t i = 0; i < max_value_plus_one; i++)
            {
                possible_values.push_back(i);
            }
        }

        for (const auto& value : possible_values)
        {
            IF_FALSE_THROW(value < max_value_plus_one, "Value [" << value << "] in opcode part [" << desc.mnemonic << " :: " << part.name << "] is too large");
            items.push({
                size_t(item.part_index + 1),
                uint32_t(item.bit_index + part.bit_count),
                uint16_t(item.value | (value << (16 - item.bit_index - part.bit_count))) });
        }
    }
}

machine_state::machine_state()
{
    uint32_t size = uint32_t(std::pow(int32_t(2), int32_t(24)));
    m_memory = (uint8_t*)::malloc(size);
    IF_FALSE_THROW(m_memory != nullptr, "Allocation failed");
    ::memset(&m_registers, 0x0, sizeof(m_registers));

    m_opcode_lut.resize(0xffff);
    for (const auto desc : g_opcode_descs)
    {
        populate_lut(desc);
    }
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
    auto inst_func = m_opcode_lut[opcode];
    IF_FALSE_THROW(inst_func != nullptr, "Invalid or unimplemented opcode: 0x" << std::hex << opcode << std::dec << " (" << std::bitset<16>(opcode) << ")");
    inst_func(*this, opcode);
}
