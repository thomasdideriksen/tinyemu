#include <iostream>
#include <vector>
#include <stack>
#include <bitset>
#include <intrin.h>

#include "common.h"
#include "machinestate.h"

template <typename T>
inline void inst_move_helper(machine_state& state, uint16_t opcode)
{
    const auto src_mode = extract_bits<10, 3>(opcode);
    const auto src_reg = extract_bits<13, 3>(opcode);

    const auto dst_mode = extract_bits<7, 3>(opcode);
    const auto dst_reg = extract_bits<4, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    T* dst_ptr = state.get_pointer<T>(dst_mode, dst_reg);

    IF_FALSE_THROW(dst_ptr != nullptr, "Couldn't evaluate destination pointer (mode: " << dst_mode << ", register: " << dst_reg << ")");

    T imm = 0;
    if (!src_ptr)
    {
        typedef traits<T>::extension_word_type_t extension_t;
        imm = (T)state.next<extension_t>();
        src_ptr = &imm;
    }

    typedef traits<T>::signed_type_t signed_t;

    state.set_ccr_bit<ccr_bit::negative>(*((signed_t*)src_ptr) < 0);
    state.set_ccr_bit<ccr_bit::zero>(*src_ptr == 0);
    state.set_ccr_bit<ccr_bit::overflow>(false);
    state.set_ccr_bit<ccr_bit::carry>(false);
    
    *dst_ptr = *src_ptr;
}

void inst_move(machine_state& state, uint16_t opcode)
{
    const auto size = extract_bits<2, 2>(opcode);
    switch (size)
    {
    case 1: inst_move_helper<uint8_t>(state, opcode); break;   // Byte
    case 2: inst_move_helper<uint32_t>(state, opcode); break;  // Long
    case 3: inst_move_helper<uint16_t>(state, opcode); break;  // Word
    default: 
        THROW("Invalid move size: " << size);
    }
}

void inst_moveq(machine_state& state, uint16_t opcode)
{
    const auto dst_reg = extract_bits<4, 3>(opcode);
    const uint8_t data = extract_bits<8, 8>(opcode);
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(0, dst_reg);

    bool is_negative;
    auto result = sign_extend(data, &is_negative);

    state.set_ccr_bit<ccr_bit::negative>(is_negative);
    state.set_ccr_bit<ccr_bit::zero>(result == 0);
    state.set_ccr_bit<ccr_bit::overflow>(false);
    state.set_ccr_bit<ccr_bit::carry>(false);

    *dst_ptr = result;
}

void inst_rte(machine_state& state, uint16_t opcode)
{
    THROW("Unimplemented instruction");
}

void int_clr(machine_state& state, uint16_t opcode)
{
    THROW("Unimplemented instruction");
}

template <typename T>
inline void inst_movea_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(1 /* Always "address register direct" mode */, dst_reg);

    T imm = 0;
    if (!src_ptr)
    {
        imm = state.next<T>();
        src_ptr = &imm;
    }

    *dst_ptr = sign_extend(*src_ptr);
}

void inst_movea(machine_state& state, uint16_t opcode)
{
    const auto size = extract_bits<2, 2>(opcode);
    switch (size)
    {
    case 2: inst_movea_helper<uint32_t>(state, opcode); break; // Long
    case 3: inst_movea_helper<uint16_t>(state, opcode); break; // Word
    default:
        THROW("Invalid movea size");
    }
}

void inst_movep(machine_state& state, uint16_t opcode)
{
    THROW("Unimplemented instruction");
}

void inst_negx(machine_state& state, uint16_t opcode)
{
    THROW("Unimplemented instruction");
}

template <typename T>
inline void inst_add_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto direction = extract_bits<7, 1>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    T* dst_ptr = state.get_pointer<T>(0 /* Always "data register direct" mode */, dst_reg);

    T imm = 0;
    if (!src_ptr)
    {
        typedef traits<T>::extension_word_type_t extension_t;
        imm = (T)state.next<extension_t>();
        src_ptr = &imm;
    }

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t result_high_precision = 
        high_precision_t(*src_ptr) + 
        high_precision_t(*dst_ptr);

    T result = T(result_high_precision & high_precision_t(traits<T>::max));

    switch (direction)
    {
    case 0: *dst_ptr = T(result); break; // Write to 'dst'
    case 1: *src_ptr = T(result); break; // Write to 'src'
    default:
        THROW("Invalid direction");
    }

    bool carry = ((result_high_precision >> traits<T>::bits) & 0x1) != 0;
    bool negative = is_negative(result);

    state.set_ccr_bit<ccr_bit::extend>(carry);
    state.set_ccr_bit<ccr_bit::negative>(negative);
    state.set_ccr_bit<ccr_bit::zero>(result == 0);
    //state.set_ccr_bit<ccr_bit::overflow>(); <--- TODO
    state.set_ccr_bit<ccr_bit::carry>(carry);
}

void inst_add(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    switch (size)
    {
    case 0: inst_add_helper<uint8_t>(state, opcode); break;  // Byte
    case 1: inst_add_helper<uint16_t>(state, opcode); break; // Word
    case 2: inst_add_helper<uint32_t>(state, opcode); break; // Long
    default:
        THROW("Invalid add size");
    }
}

template <typename T>
inline void inst_and_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto direction = extract_bits<7, 1>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    T* dst_ptr = state.get_pointer<T>(0 /* Always "data register direct" mode */, dst_reg);

    T imm = 0;
    if (!src_ptr)
    {
        typedef traits<T>::extension_word_type_t extension_t;
        imm = (T)state.next<extension_t>();
        src_ptr = &imm;
    }

    T result = (*src_ptr) & (*dst_ptr);

    switch (direction)
    {
    case 0: *dst_ptr = T(result); break; // Write to 'dst'
    case 1: *src_ptr = T(result); break; // Write to 'src'
    default:
        THROW("Invalid direction");
    }

    bool carry = ((result_high_precision >> traits<T>::bits) & 0x1) != 0;
    bool negative = is_negative(result);

    state.set_ccr_bit<ccr_bit::extend>(carry);
    state.set_ccr_bit<ccr_bit::negative>(negative);
    state.set_ccr_bit<ccr_bit::zero>(result == 0);
    //state.set_ccr_bit<ccr_bit::overflow>(); <--- TODO
    state.set_ccr_bit<ccr_bit::carry>(carry);
}

void inst_and(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    switch (size)
    {
    case 0: inst_and_helper<uint8_t>(state, opcode); break;  // Byte
    case 1: inst_and_helper<uint16_t>(state, opcode); break; // Word
    case 2: inst_and_helper<uint32_t>(state, opcode); break; // Long
    default:
        THROW("Invalid and size");
    }
}


#define ALL {}

std::vector<opcode_desc_t> g_opcode_descs = {
    {"MOVE", inst_move, {
        {2, "Fixed", {0}},
        {2, "Size", {1 /* Byte */, 2 /* Long */, 3 /* Word */}},
        {3, "Destination Register", ALL},
        {3, "Destination Addressing Mode", {0, 2, 3, 4, 5, 6, 7}},
        {3, "Source Register", ALL},
        {3, "Source Addressing Mode", ALL}}},

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
        {3, "Source Addressing Mode", ALL},
        {3, "Source Register", ALL}}},

    {"MOVEP", inst_movep, {
        {4, "Fixed", {0}},
        {3, "Destination Register (always a D register)", ALL},
        {1, "Fixed", {1}},
        {1, "Direction", {0 /* Memory to register */, 1 /* Register to memory */}},
        {1, "Size", {0 /* Word */, 1 /* Long */}},
        {3, "Fixed", {1}},
        {3, "Source Register (always an A register)", ALL}}},

    {"NEGX", inst_negx, {
        {8, "Fixed", {0x40}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Addressing Mode", ALL},
        {3, "Register", ALL}}},

    {"CLR", int_clr, {
        {8, "Prefix", {0x42}},
        {2, "Size", {0 /* Byte */, 1 /* Word */ , 2 /* Long */}},
        {3, "Addressing Mode", ALL},
        {3, "Register", ALL}}},

    {"RTE", inst_rte, {
        {16, "Fixed", {0x4e73}}}},

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

};



/*
http://www.easy68k.com/
https://www.nxp.com/files-static/archives/doc/ref_manual/M68000PRM.pdf
http://www.easy68k.com/paulrsm/doc/dpbm68k1.htm
https://wiki.neogeodev.org/index.php?title=68k
https://wiki.neogeodev.org/index.php?title=68k_instructions_timings
http://68k.hax.com/NEGX <--- 

https://www.ticalc.org/pub/text/68k/68k2x86.txt
https://www.ticalc.org/pub/text/68k/68kguide.txt
http://goldencrystal.free.fr/M68kOpcodes-v2.3.pdf <---
http://info.sonicretro.org/SCHG:68000_ASM-to-Hex_Code_Reference
http://mrjester.hapisan.com/04_MC68/
https://darkdust.net/writings/megadrive/crosscompiler

http://ocw.utm.my/pluginfile.php/1340/mod_resource/content/0/04-68k-Addressing.Modes.ppt.pdf <-- Adressing examples

http://www.cse.dmu.ac.uk/~sexton/WWWPages/exceptions.html <--- TRAP, reset vectors, exception prpocessing

8, 32-bit data registers and 8, 32-bit address registers
7 interrupt levels
56 instructions
14 addressing modes

Instruction format: 1word- 11words (2 bytes - 22 bytes)
 
 --  The first word of the instruction, called the [simple/single(?) effective address operation word], specifies:
        1) the length of the instruction
        2) the effective addressing mode
        3) and the operation to be performed

-- The remaining words, called [brief extension word] and [full extension word], further specify the instruction and operands



Adressing modes:
    0: Register direct 
    1: Absolute data
    2: PC relative 
    3: Register indirect
    4: Immediate data
    5: Implied

--> Note: The "Effective Address" is a combination of REGISTER (3 bits) and MODE (3 bits)

--> Note: TRAP is a software interrupt

*/



int main(void)
{
    try
    {
        machine_state machine;

        FILE* fp = nullptr;
        ::fopen_s(&fp, "C:\\Users\\dideriks\\Desktop\\EASy68K\\EASy68K\\test.bin", "rb");
        if (fp)
        {
            ::fseek(fp, 0, SEEK_END);
            size_t size = ::ftell(fp);
            ::fseek(fp, 0, SEEK_SET);
            void* buffer = ::malloc(size);
            ::fread(buffer, size, 1, fp);
            ::fclose(fp);
            machine.load_program(0, buffer, size, 0);
            ::free(buffer);

            while (true)
            {
                machine.tick();
            }
        }
    }
    catch (std::exception& ex)
    {
        std::cout << "Exception: " << ex.what() << std::endl;
    }

    return 0;
}