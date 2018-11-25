#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "common.h"

enum class ccr_bit
{
    Carry = 0,
    Overflow = 1,
    Zero = 2,
    Negative = 3,
    Extend = 4,
};

class machine_state;
typedef void(*inst_func_ptr_t)(machine_state&, uint16_t);

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

class machine_state
{
private:
    struct registers_t
    {
        uint32_t D[8];      // Data registers (D0 - D7)
        uint32_t A[8];      // Address registers (A0 - A7)

        uint32_t USP;       // User stack pointer (shadowed by A7 in user mode)
        uint32_t SSP;       // Supervisor stack pointer (shadowed by A7 in supervisor mode)

        uint32_t PC;        // Program counter, PC (24-bit, max 16MB of memory)
        uint16_t SR;        // Status register, SR (15-8=system byte, 7-0=user byte aka CCR (4: e[X]tend , 3: [N]egative, 2: [Z]ero, 1: o[V]erflow, 0: [C]arry) - bit 5, 6, 7 are ignored)
    };

    registers_t m_registers;
    uint8_t* m_memory;
    std::vector<inst_func_ptr_t> m_opcode_lut;
    void populate_lut(const opcode_desc_t& desc);
    
public:
    machine_state();
    virtual ~machine_state();
    void load_program(size_t memory_offset, void* program, size_t program_size, uint32_t init_pc);
    void tick();

    template <typename T>
    T next()
    {
        T* ptr = (T*)&m_memory[m_registers.PC];
        m_registers.PC += sizeof(T);
        switch (sizeof(T))
        {
        case 1: return *ptr;
        case 2: return ::_byteswap_ushort(*ptr);
        case 4: return ::_byteswap_ulong(*ptr);
        default: 
            THROW("Invalid type size");
        }
    }

    template <const ccr_bit bit>
    inline bool get_ccr_bit()
    {
        return ((m_registers.SR >> uint32_t(bit)) & 0x1) != 0;
    }

    template <const ccr_bit bit>
    inline void set_ccr_bit(bool value)
    {
        uint16_t mask = 1 << uint32_t(bit);
        if (value)
        {
            m_registers.SR |= mask;
        }
        else
        {
            m_registers.SR &= (~mask);
        }
    }

    template <typename T>
    inline T* get_pointer(uint32_t mode, uint32_t reg)
    {
        switch (mode)
        {
        case 0: // Data register direct
            return (T*)&m_registers.D[reg];

        case 1: // Address register direct
            return (T*)&m_registers.A[reg];
            
        case 2: // Address register indirect
            return (T*)&m_memory[m_registers.A[reg]];

        case 3: // Address register indirect with postincrement
            THROW("Unimplemented addressing mode: " << mode);

        case 4: // Address register indirect with predecrement
            THROW("Unimplemented addressing mode: " << mode);

        case 5: // Address register indirect with displacement
            THROW("Unimplemented addressing mode: " << mode);

        case 6: // Address register indirect with index
            THROW("Unimplemented addressing mode: " << mode);

        case 7:
            switch (reg)
            {
            case 0: // Absolute short
                THROW("Unimplemented addressing mode: " << mode);

            case 1: // Absolute long
                THROW("Unimplemented addressing mode: " << mode);

            case 2: // Program counter with displacement
                THROW("Unimplemented addressing mode: " << mode);

            case 3: // Program counter with index
                THROW("Unimplemented addressing mode: " << mode);

            case 4: // Immediate or status register
                return nullptr;

            default:
                THROW("Invalid register value for adressing mode 7: " << reg);
            }
            break;

        default:
            THROW("Invalid addressing mode: " << mode);
        }
    }
};