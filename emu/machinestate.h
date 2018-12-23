#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include "common.h"

enum class bit
{
    carry = 0,
    overflow = 1,
    zero = 2,
    negative = 3,
    extend = 4,
    supervisor = 13,
    trace = 15,
    // Note: Interrupt mask is bit 8, 9 and 10
};

enum class reg
{
    status_register,
    user_stack_pointer,
    supervisor_stack_pointer,
};

class machine_state;
typedef void(*inst_func_ptr_t)(machine_state&);

#define CHECK_SUPERVISOR(state) if (!state.get_status_bit<bit::supervisor>()) { state.exception(8 /* Privilege violation */); return; }

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
    uint32_t m_storage[4];
    uint32_t m_storage_index;

    INLINE uint32_t* get_address_register_pointer(uint32_t reg)
    {
        // Note: Adress register 7 is special: It refers to the user *or* supervisor stack pointer, depending on the current CPU mode
        if (reg == 7)
        {
            return (uint32_t*)(get_status_bit<bit::supervisor>() ? &m_registers.SSP : &m_registers.USP);
        }
        else
        {
            return (uint32_t*)&m_registers.A[reg];
        }
    }

    template <typename T>
    INLINE T* get_next_storage_pointer()
    {
        T* ptr = (T*)&m_storage[m_storage_index % countof(m_storage)];
        m_storage_index++;
        return ptr;
    }

    template <typename T>
    INLINE bool is_memory(T* ptr)
    {
        return ((uint8_t*)ptr >= m_memory && (uint8_t*)ptr < (m_memory + m_memory_size));
    }
 
public:
    machine_state();
    virtual ~machine_state();
    void load_program(size_t memory_offset, void* program, size_t program_size, uint32_t init_pc);
    void tick();
    void set_program_counter(uint32_t value);
    void offset_program_counter(int32_t offset);
    void push_program_counter();
    void pop_program_counter();
    void push_status_register();
    void pop_status_register();
    void exception(uint32_t vector);
    void reset();
    void set_condition_code_register(uint8_t ccr);

    template <typename T>
    INLINE void push(T value)
    {
        uint32_t* stack_ptr = get_pointer<uint32_t>(make_effective_address<1, 7>());
        uint32_t stack_value = read(stack_ptr);
        int64_t new_stack_value = int64_t(stack_value) - int64_t(sizeof(T));
        IF_FALSE_THROW(new_stack_value >= 0, "Stack overflow");
        write<uint32_t>(stack_ptr, uint32_t(new_stack_value));
        write<T>((T*)&m_memory[new_stack_value], value);
    }

    template <typename T>
    INLINE T pop()
    {
        uint32_t* stack_ptr = get_pointer<uint32_t>(make_effective_address<1, 7>());
        uint32_t stack_value = read(stack_ptr);
        T* mem_ptr = (T*)&m_memory[stack_value];
        T result = read(mem_ptr);
        uint32_t new_stack_value = stack_value + sizeof(T);
        write<uint32_t>(stack_ptr, new_stack_value);
        return result;
    }

    template <typename T>
    INLINE uint32_t pointer_to_memory_offset(T* ptr)
    {
        IF_FALSE_THROW(is_memory(ptr), "Invalid memory pointer");
        return uint32_t(size_t(ptr) - size_t(m_memory));
    }

    template <typename T>
    INLINE void write(T* dst, T value)
    {
        if (is_memory(dst))
        {
            *dst = swap<T>(value);
        }
        else
        {
            *dst = value;
        }
    }

    template <typename T>
    INLINE T read(T* src)
    {
        if (is_memory(src))
        {
            return swap(*src);
        }
        else
        {
            return *src;
        }
    }

    template <typename T>
    INLINE T next()
    {
        T* ptr = (T*)&m_memory[m_registers.PC];
        m_registers.PC += sizeof(T);
        return swap<T>(*ptr);
    }

    template <const bit bit>
    INLINE bool get_status_bit()
    {
        return ((m_registers.SR >> uint32_t(bit)) & 0x1) != 0;
    }

    template <const bit bit>
    INLINE void set_status_bit(bool value)
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
    INLINE T* get_pointer(reg reg)
    {
        switch (reg)
        {
        case reg::status_register: return (T*)&m_registers.SR;
        case reg::supervisor_stack_pointer: return (T*)&m_registers.SSP;
        case reg::user_stack_pointer: return (T*)&m_registers.USP;
        default:
            THROW("Invalid register");
        }
    }

    template <typename T>
    INLINE T* get_pointer(uint32_t effective_address)
    {
        auto reg = effective_address & 0x7;
        auto mode = (effective_address >> 3) & 0x7;

        switch (mode)
        {
        case 0: // Data register direct
        {
            return (T*)&m_registers.D[reg];
        }

        case 1: // Address register direct
        {
            return (T*)get_address_register_pointer(reg);
        }
            
        case 2: // Address register indirect
        {
            auto ptr = get_address_register_pointer(reg);
            auto value = *ptr;
            return (T*)&m_memory[value];
        }

        case 3: // Address register indirect with postincrement
        {
            auto ptr = get_address_register_pointer(reg);
            auto value = *ptr;
            (*ptr) += sizeof(T);
            return (T*)&m_memory[value];
        }

        case 4: // Address register indirect with predecrement
        {
            auto ptr = get_address_register_pointer(reg);
            (*ptr) -= sizeof(T);
            auto value = *ptr;
            return (T*)&m_memory[value];
        }
         
        case 5: // Address register indirect with displacement
        {
            auto ptr = get_address_register_pointer(reg);
            auto value = *ptr;
            auto displacement = next<uint16_t>();
            return (T*)&m_memory[value + displacement];
        }

        case 6: // Address register indirect with index
            THROW("Unimplemented addressing mode: " << mode);

        case 7:
            switch (reg)
            {
            case 0: // Absolute short
            {
                uint16_t* ptr = get_next_storage_pointer<uint16_t>();
                *ptr = next<uint16_t>();
                return (T*)ptr;
            }
                
            case 1: // Absolute long
            {
                uint32_t* ptr = get_next_storage_pointer<uint32_t>();
                *ptr = next<uint32_t>();
                return (T*)ptr;
            }

            case 2: // Program counter with displacement
                THROW("Unimplemented addressing mode: " << mode);

            case 3: // Program counter with index
                THROW("Unimplemented addressing mode: " << mode);

            case 4: // Immediate or status register
            {
                typedef traits<T>::extension_word_type_t extension_t;
                extension_t* ptr = get_next_storage_pointer<extension_t>();
                *ptr = next<extension_t>();
                return (T*)ptr;
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