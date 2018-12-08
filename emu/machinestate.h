#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "common.h"

enum class bit
{
    carry = 0,
    overflow = 1,
    zero = 2,
    negative = 3,
    extend = 4,
    mode = 13,
    trace = 15,
    // Note: Interrupt mask is bit 8, 9 and 10
};

class machine_state;
typedef void(*inst_func_ptr_t)(machine_state&, uint16_t);

class machine_state
{
private:
    struct registers_t
    {
        uint32_t D[8];      // Data registers (D0 - D7)
        uint32_t A[7];      // Address registers (A0 - A6)

        uint32_t USP;       // User stack pointer (shadowed by A7 in user mode)
        uint32_t SSP;       // Supervisor stack pointer (shadowed by A7 in supervisor mode)

        uint32_t PC;        // Program counter, PC (24-bit, max 16MB of memory)
        uint16_t SR;        // Status register, SR (15-8=system byte, 7-0=user byte aka CCR (4: e[X]tend , 3: [N]egative, 2: [Z]ero, 1: o[V]erflow, 0: [C]arry) - bit 5, 6, 7 are ignored)
    };

    registers_t m_registers;
    uint8_t* m_memory;
    size_t m_memory_size;
    std::vector<inst_func_ptr_t> m_opcode_table;
    uint32_t m_imm_storage;

    template <typename T>
    inline T* get_address_register_pointer(uint32_t reg)
    {
        // Note: Adress register 7 is special: It refers to the user *or* supervisor stack pointer, depending on the current CPU mode
        if (reg == 7)
        {
            return (T*)(get_status_register<bit::mode>() ? &m_registers.SSP : &m_registers.USP);
        }
        else
        {
            return (T*)&m_registers.A[reg];
        }
    }
 
public:
    machine_state();
    virtual ~machine_state();
    void load_program(size_t memory_offset, void* program, size_t program_size, uint32_t init_pc);
    void tick();
    
    template <typename T>
    inline void write(T* dst, T value)
    {
        *dst = value;
        if ((uint8_t*)dst >= m_memory && (uint8_t*)dst < (m_memory + m_memory_size))
        {
            // TODO: Callback for memory mapped stuff
        }
    }

    template <typename T>
    T next()
    {
        T* ptr = (T*)&m_memory[m_registers.PC];
        m_registers.PC += sizeof(T);
        switch (sizeof(T))
        {
        case 1: return *ptr;
        case 2: return T(::_byteswap_ushort(*ptr));
        case 4: return T(::_byteswap_ulong(*ptr));
        default: 
            THROW("Invalid type size");
        }
    }

    template <const bit bit>
    inline bool get_status_register()
    {
        return ((m_registers.SR >> uint32_t(bit)) & 0x1) != 0;
    }

    template <const bit bit>
    inline void set_status_register(bool value)
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

    //template <typename T, const bool use_imm = true>
    //inline T* get_pointer(uint32_t effective_address)
    //{
    //    auto reg = (effective_address & 0x7);
    //    auto mode = (effective_address >> 3) & 0x7;
    //    return get_pointer<T, use_imm >(mode, reg);
    //}

    template <typename T, const bool use_imm = true>
    inline T* get_pointer(uint32_t mode, uint32_t reg)
    {
        switch (mode)
        {
        case 0: // Data register direct
            return (T*)&m_registers.D[reg];

        case 1: // Address register direct
            return get_address_register_pointer<T>(reg);
            
        case 2: // Address register indirect
            return (T*)&m_memory[*(get_address_register_pointer<T>(reg))];

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
                if (use_imm)
                {
                    typedef traits<T>::extension_word_type_t extension_t;
                    m_imm_storage = next<extension_t>();
                    return (T*)&m_imm_storage;
                }
                else
                {
                    return (T*)&m_registers.SR;
                }

            default:
                THROW("Invalid register value for adressing mode 7: " << reg);
            }
            break;

        default:
            THROW("Invalid addressing mode: " << mode);
        }
    }
};