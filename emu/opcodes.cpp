#pragma once
#include <stack>
#include <bitset>
#include <unordered_map>
#include "opcodes.h"
#include "common.h"
#include "instructions.h"

#define ALL {}

struct opcode_part_desc_t
{
    uint32_t bit_count;
    std::string name;
    std::vector<uint16_t> possible_values;
};

struct opcode_desc_t
{
    std::string mnemonic;
    inst_func_ptr_t func_ptr;
    std::vector<opcode_part_desc_t> parts;
};

const uint32_t D = 1 << 0;      // Data register direct
const uint32_t A = 1 << 1;      // Address register direct
const uint32_t Ai = 1 << 2;     // Address register indirect
const uint32_t AiPi = 1 << 3;   // Address register indirect with post increment
const uint32_t AiPd = 1 << 4;   // Address register indirect with post decrement
const uint32_t AiD = 1 << 5;    // Address register indirect with displacement
const uint32_t AiI = 1 << 6;    // Address register indirect with index
const uint32_t AbsS = 1 << 7;   // Absolute short
const uint32_t AbsL = 1 << 8;   // Absolute long
const uint32_t PcD = 1 << 9;    // Program counter with displacement
const uint32_t PcI = 1 << 10;   // Program counter with index
const uint32_t ImmSr = 1 << 11; // Immediate or status register

std::vector<uint16_t> make_addressing_modes(uint32_t modes)
{
    std::vector<uint16_t> result;
    for (uint32_t i = 0; i <= 11; i++)
    {
        if ((i & modes) != 0)
        {
            if (i <= 6)
            {
                for (uint16_t j = 0; j < 8; j++)
                {
                    result.push_back((i << 3) | j);
                }
            }
            else
            {
                result.push_back((7 << 3) | (i - 7));
            }
        }
    }
    return result;
}

#define MODES(modes) make_addressing_modes(modes)
#define ALL_MODES make_addressing_modes(0xffffffff)
#define ALL_MODES_EXCEPT(modes) make_addressing_modes(~(modes))

void inst_unimplemented(machine_state&, uint16_t opcode)
{
    THROW("Unimplemented instruction with opcode: 0x" << std::hex << opcode << " (" << std::bitset<16>(opcode) << ")");
}

std::vector<opcode_desc_t> opcode_descriptions = {

    {"MOVE", inst_move, {
        {2, "Fixed", {0}},
        {2, "Size", {1 /* Byte */, 2 /* Long */, 3 /* Word */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)},
        {6, "Effective address, source", ALL_MODES}}},

    {"MOVEQ", inst_moveq, {
        {4, "Fixed", {0x7}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Fixed", {0}},
        {8, "Data", ALL}}},

    {"MOVEA", inst_movea, {
        {2, "Fixed", {0}},
        {2, "Size", {2 /* Long */, 3 /* Word */}},
        {3, "Destination Register (always an A register)", ALL},
        {3, "Fixed", {1}},
        {3, "Effective address, source", ALL_MODES}}},

    {"CLR", inst_clr, {
        {8, "Prefix", {0x42}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Effective address", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ADD", inst_add, {
        {4, "Fixed", {0xd}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {0 /* Write to dst (D register) */, 1 /* Write to src (ea) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Source Addressing Mode", ALL},
        {3, "Source Register", ALL}}},

    {"AND", inst_and, {
        {4, "Fixed", {0xc}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {0 /* Write to dst (D register) */, 1 /* Write to src (ea) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Source Addressing Mode", ALL},
        {3, "Source Register", ALL}}},

    {"EOR", inst_eor, {
        {4, "Fixed", {0xb}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {0 /* Write to dst (D register) */, 1 /* Write to src (ea) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Source Addressing Mode", ALL},
        {3, "Source Register", ALL}}},
    
    {"RTE", inst_unimplemented, {
        {16, "Fixed", {0x4e73}}}},

    {"MOVEP", inst_unimplemented, {
        {4, "Fixed", {0}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Fixed", {1}},
        {1, "Direction", {0 /* Memory to register */, 1 /* Register to memory */}},
        {1, "Size", {0 /* Word */, 1 /* Long */}},
        {3, "Fixed", {1}},
        {3, "Source Register (always an A register)", ALL}}},

    {"NEGX", inst_unimplemented, {
        {8, "Fixed", {0x40}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Addressing Mode", ALL},
        {3, "Register", ALL}}},

    {"ORI", inst_ori, {
        {8, "Fixed", {0}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Mode", ALL},
        {3, "Register", ALL}}},

    {"ANDI", inst_andi, {
        {8, "Fixed", {0x2}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Mode", ALL},
        {3, "Register", ALL}}},

    {"EORI", inst_eori, {
        {8, "Fixed", {0xa}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Mode", ALL},
        {3, "Register", ALL}}},

    {"SUBI", inst_subi, {
        {8, "Fixed", {0x4}},
        {2, "Size", ALL},
        {3, "Mode", ALL},
        {3, "Register", ALL}}},

    {"ADDI", inst_addi, {
        {8, "Fixed", {0x6}},
        {2, "Size", ALL},
        {3, "Mode", ALL},
        {3, "Register", ALL}}},

    {"CMPI", inst_cmpi, {
        {8, "Fixed", {0xc}},
        {2, "Size",  {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address", ALL_MODES_EXCEPT(A | ImmSr)}}},
    
    {"BTST (immediate)", inst_btst, {
        {10, "Fixed", {0x20}},
        {6, "Effective address, destination ", ALL_MODES_EXCEPT(A | ImmSr)}} },

    {"BTST (register)", inst_btst, {
        {4, "Fixed", {0}},
        {3, "Source register (always a D register)", ALL},
        {3, "Fixed", {4}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | ImmSr)}}},
};

void make_opcode_table_range(const opcode_desc_t& desc, inst_func_ptr_t* table_ptr)
{
    struct item_t
    {
        size_t part_index;
        uint32_t bit_index;
        uint16_t value;
    };

    std::unordered_map<inst_func_ptr_t, std::string> mnemonics;

    std::stack<item_t> items;
    items.push({ 0 });

    while (!items.empty())
    {
        const auto item = items.top();
        items.pop();

        if (item.part_index >= desc.parts.size())
        {
            IF_FALSE_THROW(item.bit_index == 16, "Invalid bit count in [" << desc.mnemonic << "] opcode description");
            IF_FALSE_THROW(table_ptr[item.value] == nullptr, "Cannot assign opcode [" << desc.mnemonic << "] to table slot 0x" << std::hex << item.value << std::dec << " (" << std::bitset<16>(item.value) << ") since the slot is already occupied by opcode [" << mnemonics[desc.func_ptr] << "]");
            table_ptr[item.value] = desc.func_ptr;
            mnemonics[desc.func_ptr] = desc.mnemonic;
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

void make_opcode_table(std::vector<inst_func_ptr_t>& table)
{
    table.resize(0xffff + 1);
    auto table_ptr = table.data();
    ::memset(table_ptr, 0x0, table.size() * sizeof(inst_func_ptr_t));

    for (const auto& desc : opcode_descriptions)
    {
        make_opcode_table_range(desc, table_ptr);
    }
}