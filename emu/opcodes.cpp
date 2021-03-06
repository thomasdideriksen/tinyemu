#pragma once
#include <stack>
#include <bitset>
#include <unordered_map>
#include <iostream>
#include "opcodes.h"
#include "common.h"
#include "instructions.h"

#if false

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

enum class effective_address_order
{
    mode_register,
    register_mode
};

uint16_t make_addressing_mode(uint16_t mode, uint16_t reg, effective_address_order order)
{
    switch (order)
    {
    case effective_address_order::mode_register:
        return (mode << 3) | reg;
    case effective_address_order::register_mode:
        return (reg << 3) | mode;
    default:
        THROW("Invalid order");
    }
}

std::vector<uint16_t> make_addressing_modes(
    uint32_t modes,
    effective_address_order order)
{
    std::vector<uint16_t> result;
    for (uint32_t i = 0; i <= 11; i++)
    {
        const uint32_t mask = (1 << i);

        if ((mask & modes) != 0)
        {
            if (i <= 6)
            {
                for (uint16_t j = 0; j < 8; j++)
                {
                    result.push_back(make_addressing_mode(i, j, order));
                }
            }
            else
            {
                result.push_back(make_addressing_mode(7, i - 7, order));
            }
        }
    }
    return result;
}

#define ALL {}
#define MODES(modes) make_addressing_modes(modes, effective_address_order::mode_register)
#define ALL_MODES make_addressing_modes(0xffffffff, effective_address_order::mode_register)
#define ALL_MODES_EXCEPT(modes) make_addressing_modes(~(modes), effective_address_order::mode_register)
#define REGISTER_FIRST_ALL_MODES_EXCEPT(modes) make_addressing_modes(~(modes), effective_address_order::register_mode)

std::vector<opcode_desc_t> opcode_descriptions = {

    {"MOVE", inst_move, {
        {2, "Fixed", {0}},
        {2, "Size", {1 /* Byte */, 2 /* Long */, 3 /* Word */}},
        {6, "Effective address, destination", REGISTER_FIRST_ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)},
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
        {6, "Effective address, source", ALL_MODES}}},

    {"CLR", inst_clr, {
        {8, "Prefix", {0x42}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ADD", inst_add, {
        {4, "Fixed", {0xd}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {0 /* Write to dst (D register) */, 1 /* Write to src (ea) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Source", ALL_MODES}}},

    {"AND (write to dst)", inst_and, {
        {4, "Fixed", {0xc}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {0 /* Write to dst (D register) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A)}}},

    {"AND (write to src)", inst_and, {
        {4, "Fixed", {0xc}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {1 /* Write to src (ea) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A | D /* Note: When "direction" is 1 (write to src) src cannot be a D register */)}}},

    {"EOR", inst_eor, {
        {4, "Fixed", {0xb}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {0 /* Write to dst (D register) */, 1 /* Write to src (ea) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"OR", inst_or, {
        {4, "Fixed", {0x8}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Direction", {0 /* Write to dst (D register) */, 1 /* Write to src (ea) */}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"MOVEP", inst_movep, {
        {4, "Fixed", {0}},
        {3, "Source register (always a D register)", ALL},
        {1, "Fixed", {1}},
        {1, "Direction", {0 /* Memory to register */, 1 /* Register to memory */}},
        {1, "Size", {0 /* Word */, 1 /* Long */}},
        {3, "Fixed", {1}},
        {3, "Destination register (always an A register)", ALL}}},

    {"NEGX", inst_negx, {
        {8, "Fixed", {0x40}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ORI", inst_ori, {
        {8, "Fixed", {0}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ANDI", inst_andi, {
        {8, "Fixed", {0x2}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"EORI", inst_eori, {
        {8, "Fixed", {0xa}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"SUBI", inst_subi, {
        {8, "Fixed", {0x4}},
        {2, "Size", ALL},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ADDI", inst_addi, {
        {8, "Fixed", {0x6}},
        {2, "Size", ALL},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"CMPI", inst_cmpi, {
        {8, "Fixed", {0xc}},
        {2, "Size",  {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | ImmSr)}}},

    {"BTST (immediate)", inst_btst, {
        {10, "Fixed", {0x20}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | ImmSr)}}},

    {"BTST (register)", inst_btst, {
        {4, "Fixed", {0}},
        {3, "Source register (always a D register)", ALL},
        {3, "Fixed", {4}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | ImmSr)}}},

    {"JMP", inst_jmp, {
        {10, "Fixed", {0x13b}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(D | A | AiPd | AiPi | ImmSr)}}},

    {"JSR", inst_jsr, {
        {10, "Fixed", {0x13a}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(D | A | AiPd | AiPi | ImmSr)}}},

    {"RTS", inst_rts, {
        {16, "Fixed", {0x4e75}}}},

    {"ADDQ", inst_addq, {
        {4, "Fixed", {0x5}},
        {3, "Data", ALL},
        {1, "Fixed", {0}},
        {2, "Size",  {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(PcD | PcI | ImmSr)}}},

    {"SUBQ", inst_subq, {
        {4, "Fixed", {0x5}},
        {3, "Data", ALL},
        {1, "Fixed", {1}},
        {2, "Size",  {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(PcD | PcI | ImmSr)}}},

    {"TRAPV", inst_trapv, {
        {16, "Fixed", {0x4e76}}}},

    {"TRAP", inst_trap, {
        {12, "Fixed", {0x4e4}},
        {4, "Vector", ALL}}},

    {"RTE", inst_rte, {
        {16, "Fixed", {0x4e73}}}},

    {"LEA", inst_lea, {
        {4, "Fixed", {0x4}},
        {3, "Destination register, always an A register", ALL},
        {3, "Fixed", {0x7}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(D | A | AiPd | AiPi | ImmSr)}}},

    {"PEA", inst_pea, {
        {10, "Fixed", {0x121}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(D | A | AiPd | AiPi | ImmSr)}}},

    {"CHK", inst_chk, {
        {4, "Fixed", {0x4}},
        {3, "Register, comparand", ALL},
        {3, "Fixed", {0x6}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"MOVEM (reg to mem)", inst_movem, {
        {5, "Fixed", {0x9}},
        {1, "Direction", {0 /* Register to memory */}},
        {3, "Fixed", {1}},
        {1, "Size", {0 /* Word*/ , 1 /* Long */}},
        {6, "Effective address, source", MODES(Ai | AiPd | AiD | AiI | AbsS | AbsL)}}},

    {"MOVEM (mem to reg)", inst_movem, {
        {5, "Fixed", {0x9}},
        {1, "Direction", {1 /* Memory to register */}},
        {3, "Fixed", {1}},
        {1, "Size", {0 /* Word*/ , 1 /* Long */}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(D | A | AiPd | ImmSr)}}},

    {"Scc", inst_scc, {
        {4, "Fixed", {0x5}},
        {4, "Condition", ALL},
        {2, "Fixed", {0x3}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"EXT", inst_ext, {
        {9, "Fixed", {0x91}},
        {1, "Size", {0 /* Word */, 1 /* Long */}},
        {3, "Fixed", {0}},
        {3, "Data register", ALL}}},

    {"SWAP", inst_swap, {
        {13, "Fixed", {0x908}},
        {3, "Data register", ALL}}},

    {"ILLEGAL", inst_illegal, {
        {16, "Fixed", {0x4afc}}}},

    {"NOT", inst_not, {
        {8, "Fixed", {0x46}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"LINK", inst_link, {
        {13, "Fixed", {0x9ca}},
        {3, "Address register", ALL}}},

    {"UNLK", inst_unlk, {
        {13, "Fixed", {0x9cb}},
        {3, "Address register", ALL}}},

    {"TAS", inst_tas, {
        {10, "Fixed", {0x12b}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"TST", inst_tst, {
        {8, "Fixed", {0x4a}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A | ImmSr)}}},

    {"RESET", inst_reset, {
        {16, "Fixed", {0x4e70}}}},

    {"NOP", inst_nop, {
        {16, "Fixed", {0x4e71}}}},

    {"NEG", inst_neg, {
        {8, "Fixed", {0x44}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {6, "Effective address, source", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"RTR", inst_rtr, {
        {16, "Fixed", {0x4e77}}}},

    {"DIVU", inst_divu, {
        {4, "Fixed", {0x8}},
        {3, "Source register", ALL},
        {3, "Fixed", {0x3}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A)}}},

    {"DIVS", inst_divs, {
       {4, "Fixed", {0x8}},
       {3, "Source register", ALL},
       {3, "Fixed", {0x7}},
       {6, "Effective address, destination", ALL_MODES_EXCEPT(A)}}},

    {"EXG", inst_exg, {
        {4, "Fixed", {0xc}},
        {3, "Register 1 (prefer D)", ALL},
        {1, "Fixed", {1}},
        {5, "Operation", {0x8 /* D/D */, 0x9 /* A/A */, 0x11 /* D/A */}},
        {3, "Register 2 (prefer A)", ALL}}},

    {"LSL (memory)", inst_lsl_mem, {
        {10, "Fixed", {0x38f}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"LSR (memory)", inst_lsr_mem, {
        {10, "Fixed", {0x38b}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"LSL (register)", inst_lsl_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {1}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {1}},
        {3, "Data register", ALL}}},

    {"LSR (register)", inst_lsr_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {0}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {1}},
        {3, "Data register", ALL}}},

    {"ROL (memory)", inst_rol_mem, {
        {10, "Fixed", {0x39f}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ROR (memory)", inst_ror_mem, {
        {10, "Fixed", {0x39b}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}} },

    {"ROL (register)", inst_rol_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {1}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {3}},
        {3, "Data register", ALL}} },

    {"ROR (register)", inst_ror_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {0}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {3}},
        {3, "Data register", ALL}} },

    {"ROXL (memory)", inst_roxl_mem, {
        {10, "Fixed", {0x397}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ROXR (memory)", inst_roxr_mem, {
        {10, "Fixed", {0x393}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ROXL (register)", inst_roxl_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {1}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {2}},
        {3, "Data register", ALL}}},

    {"ROXR (register)", inst_roxr_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {0}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {2}},
        {3, "Data register", ALL}}},

    {"ASL (memory)", inst_asl_mem, {
        {10, "Fixed", {0x387}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ASR (memory)", inst_asr_mem, {
        {10, "Fixed", {0x383}},
        {6, "Effective address, destination", ALL_MODES_EXCEPT(A | PcD | PcI | ImmSr)}}},

    {"ASL (register)", inst_asl_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {1}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {0}},
        {3, "Data register", ALL}}},

    {"ASR (register)", inst_asr_reg, {
        {4, "Fixed", {0xe}},
        {3, "Immediate or data register", ALL},
        {1, "Fixed", {0}},
        {2, "Size", {0 /* Byte */, 1 /* Word */, 2 /* Long */}},
        {1, "Mode", {0 /* Immediate */, 1 /* Register */}},
        {2, "Fixed", {0}},
        {3, "Data register", ALL}}},

    {"Bcc", inst_bcc, {
        {4, "Fixed", {0x6}},
        {4, "Condition", {0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf}},
        {8, "Displacement", ALL}}},

    {"DBcc", inst_dbcc, {
        {4, "Fixed", {0x5}},
        {4, "Condition", ALL},
        {5, "Fixed", {0x19}},
        {3, "Data register", ALL}}},

    {"BRA", inst_bra, {
        {8, "Fixed", {0x60}},
        {8, "Displacement", ALL}}},

    {"BSR", inst_bsr, {
        {8, "Fixed", {0x61}},
        {8, "Displacement", ALL}}},
};

void make_opcode_table_range(
    const opcode_desc_t& desc,
    std::unordered_map<uint16_t, std::string>& mnemonics,
    inst_func_ptr_t* table_ptr)
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
            IF_FALSE_THROW(item.bit_index == 16, "Invalid bit count in [" << desc.mnemonic << "] opcode description");
            IF_FALSE_THROW(table_ptr[item.value] == nullptr, "Cannot assign opcode [" << desc.mnemonic << "] to table slot 0x" << std::hex << item.value << std::dec << " (" << std::bitset<16>(item.value) << ") since the slot is already occupied by opcode [" << mnemonics[item.value] << "]");
            table_ptr[item.value] = desc.func_ptr;
            mnemonics[item.value] = desc.mnemonic;
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
    std::unordered_map<uint16_t, std::string> mnemonics;

    for (const auto& desc : opcode_descriptions)
    {
        make_opcode_table_range(desc, mnemonics, table_ptr);
    }

    int set_count = 0;
    for (size_t i = 0; i < table.size(); i++)
    {
        set_count += (table[i] != nullptr) ? 1 : 0;
    }
    std::cout << "Opcode table occupancy: " << set_count << " of " << table.size() << std::endl;
}
#else

void make_opcode_table(std::vector<inst_func_ptr_t>& table)
{
    table.resize(0xffff + 1);
#include "generated.cpp"
}

#endif