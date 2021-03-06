#pragma once

#include "machinestate.h"

//
// MOVE
// Move data
//

template <typename T>
void move(machine_state& state, uint16_t opcode)
{
    auto src = extract_bits<10, 6>(opcode);
    auto dst = extract_bits<4, 6>(opcode);
    
    dst = (dst >> 3) | ((dst & 0x3) << 3); // Note: Swap upper and lower 3 bits

    T* src_ptr = state.get_pointer<T>(src);
    T* dst_ptr = state.get_pointer<T>(dst);

    T result = state.read(src_ptr);

    state.set_status_bit<bit::negative>(is_negative(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write(dst_ptr, result);
}

//
// MOVE from SR
// Move data from status register
//

void move_from_sr(machine_state& state, uint16_t opcode)
{
    auto dst = extract_bits<10, 6>(opcode);
    auto src_ptr = state.get_pointer<uint16_t>(reg::status_register);
    auto dst_ptr = state.get_pointer<uint16_t>(dst);
    auto result = state.read(src_ptr);
    state.write(dst_ptr, result);
}

//
// MOVE to CCR
// Move data to condition code register
// 

void move_to_ccr(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    auto src_ptr = state.get_pointer<uint8_t>(ea);
    auto dst_ptr = state.get_pointer<uint8_t>(reg::status_register);
    auto result = state.read(src_ptr);
    state.write(dst_ptr, result);
}

//
// MOVE to SR
// Move data to status register
//

template <uint16_t src>
void move_to_sr(machine_state& state, uint16_t opcode)
{
    CHECK_SUPERVISOR(state);
    auto src_ptr = state.get_pointer<uint16_t>(src);
    auto dst_ptr = state.get_pointer<uint16_t>(reg::status_register);
    auto result = state.read(src_ptr);
    state.write(dst_ptr, result);
}

//
// MOVE USP
// Read or write user stack pointer
//

template <uint16_t dir>
void move_usp(machine_state& state, uint16_t opcode)
{
    CHECK_SUPERVISOR(state);
    auto ea = extract_bits<13, 3>(opcode);
    auto reg_ptr = state.get_pointer<uint32_t>(reg::user_stack_pointer);
    auto mem_ptr = state.get_pointer<uint32_t>(ea);
    switch (dir)
    {
    case 0: // Register to memory
    {
        auto result = state.read(reg_ptr);
        state.write(mem_ptr, result);
    }
    break;

    case 1: // Memory to register
    {
        auto result = state.read(mem_ptr);
        state.write(reg_ptr, result);
    }
    break;

    default:
        THROW("Invalid direction");
    }
}

//
// MOVEQ
// Move quick (move literal)
//

void moveq(machine_state& state, uint16_t opcode)
{   
    auto dst = extract_bits<4, 3>(opcode);
    auto data = extract_bits<8, 8>(opcode);

    auto dst_ptr = state.get_pointer<uint32_t>(make_effective_address(0, dst));
    auto result = sign_extend(data);

    state.set_status_bit<bit::negative>(is_negative(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write(dst_ptr, result);
}

//
// MOVEA
// Move address
//

template <typename T>
void movea(machine_state& state, uint16_t opcode)
{
    auto src_ea = extract_bits<10, 6>(opcode);
    auto dst_reg = extract_bits<4, 3>(opcode);

    auto src_ptr = state.get_pointer<T>(src_ea);
    auto dst_ptr = state.get_pointer<uint32_t>(make_effective_address(1, dst_reg));
    auto val = state.read<T>(src_ptr);
    state.write<uint32_t>(dst_ptr, sign_extend(val));
}

//
// MOVEM
// Move multiple registers
//

template <uint16_t dir, typename T>
void movem(machine_state& state, uint16_t opcode)
{
    auto src_ea = extract_bits<10, 6>(opcode);

    auto register_select = state.next<uint16_t>(); // Note: Get this first so we don't interfere with addressing modes, etc.
    auto mem_mode = (src_ea >> 3) & 0x7;

    int32_t delta = 1;
    int32_t begin = 0;

    if (dir == 0 /* Register to memory */ && mem_mode == 4 /* Address register indirect with predecrement */)
    {
        // Note: With pre-decrement addressing mode the registers are stored in inverse order (A7-A0, D7-D0)
        delta = -1;
        begin = 15;
    }

    for (int32_t counter = 0, i = begin; counter < 16; counter++, i += delta)
    {
        const uint32_t mask = (1 << i);

        if ((mask & register_select) != 0)
        {
            uint32_t reg_mode = (i >> 3) & 0x1;
            uint32_t reg_reg = i & 0x7;

            T* reg = state.get_pointer<T>(make_effective_address(reg_mode, reg_reg));
            T* mem = state.get_pointer<T>(src_ea);

            switch (dir)
            {
            case 0: // Register to memory 
                state.write(mem, state.read(reg));
                break;
            case 1: // Memory to register
                state.write((uint32_t*)reg, sign_extend(state.read(mem)));
                break;
            default:
                THROW("Invalid direction");
            }
        }
    }
}

//
// MOVEP
// Move peripheral data
//

template <uint16_t dir, typename T>
void movep(machine_state& state, uint16_t opcode)
{
    auto src_reg = extract_bits<4, 3>(opcode);
    auto dst_reg = extract_bits<13, 3>(opcode);

    auto reg_ptr = state.get_pointer<uint8_t>(make_effective_address(0, src_reg)) + sizeof(T) - 1; // Note: Endian
    auto mem_ptr = state.get_pointer<uint8_t>(make_effective_address(2, dst_reg)) + state.next<int16_t>();

    for (uint32_t i = 0; i < sizeof(T); i++)
    {
        switch (dir)
        {
        case 0: state.write<uint8_t>(reg_ptr, state.read<uint8_t>(mem_ptr)); break; // Memory to register
        case 1: state.write<uint8_t>(mem_ptr, state.read<uint8_t>(reg_ptr)); break; // Register to memory
        default:
            THROW("Invalid direction");
        }
        mem_ptr += 2;
        reg_ptr -= 1;  // Note: Endian
    }
}

//
// CLR
// Clear operand
//

template <typename T>
void clr(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    T* ptr = state.get_pointer<T>(ea);

    state.set_status_bit<bit::negative>(false);
    state.set_status_bit<bit::zero>(true);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write(ptr, T(0x0));
}

struct operation_sub { template <typename T> static T execute(T a, T b) { return a - b; } };
struct operation_add { template <typename T> static T execute(T a, T b) { return a + b; } };

//
// Helper: ADD, SUB
//

template <uint16_t mode, typename T, typename O>
INLINE void arithmetic_helper(machine_state& state, uint16_t opcode)
{
    auto reg = extract_bits<4, 3>(opcode);
    auto ea = extract_bits<10, 6>(opcode);

    T *src, *dst;

    switch (mode)
    {
    case 0: // Write to D register
        src = state.get_pointer<T>(ea);
        dst = state.get_pointer<T>(make_effective_address(0, reg));
        break;
    case 1: // Write to effective address
        src = state.get_pointer<T>(make_effective_address(0, reg));
        dst = state.get_pointer<T>(ea);
        break;
    default:
        THROW("Invalid mode");
    }

    auto src_val = state.read<T>(src);
    auto dst_val = state.read<T>(dst);

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t result_high_precision = O::template execute<high_precision_t>(
        high_precision_t(src_val), 
        high_precision_t(dst_val));

    T result = T(result_high_precision);

    bool carry = has_carry(result_high_precision);
    bool negative = is_negative(result);
    bool zero = (result == 0);
    bool overflow = has_overflow(src_val, dst_val, result);

    state.set_status_bit<bit::extend>(carry);
    state.set_status_bit<bit::negative>(negative);
    state.set_status_bit<bit::zero>(zero);
    state.set_status_bit<bit::overflow>(overflow);
    state.set_status_bit<bit::carry>(carry);

    state.write<T>(dst, result);
}

//
// ADD
// Arithmetic add
//

template <uint16_t mode, typename T>
void add(machine_state& state, uint16_t opcode)
{
    arithmetic_helper<mode, T, operation_add>(state, opcode);
}

//
// SUB
// Arithmetic subtract
//

template <uint16_t mode, typename T>
void sub(machine_state& state, uint16_t opcode)
{
    arithmetic_helper<mode, T, operation_sub>(state, opcode);
}

//
// Helper: ADDI, SUBI
//

template <typename T, typename O>
INLINE void arithmetic_imm_helper(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);

    T* dst_ptr = state.get_pointer<T>(ea);
    T dst = state.read(dst_ptr);

    typedef traits<T>::extension_word_type_t extension_t;
    auto imm = state.next<extension_t>();

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t result_high_precision = O::template execute<high_precision_t>(
            high_precision_t(dst),
            high_precision_t(imm));

    T result = T(result_high_precision);

    bool carry = has_carry(result_high_precision);
    bool negative = is_negative(result);
    bool zero = (result == 0);
    bool overflow = has_overflow<T>(dst, T(imm), result);

    state.set_status_bit<bit::extend>(carry);
    state.set_status_bit<bit::negative>(negative);
    state.set_status_bit<bit::zero>(zero);
    state.set_status_bit<bit::overflow>(overflow);
    state.set_status_bit<bit::carry>(carry);

    state.write(dst_ptr, result);
}

//
// ADDI
// Arithmetic add immediate
//

template <typename T>
void addi(machine_state& state, uint16_t opcode)
{
    arithmetic_imm_helper<T, operation_add>(state, opcode);
}

//
// SUBI
// Arithmetic subtract immediate
//

template <typename T>
void subi(machine_state& state, uint16_t opcode)
{
    arithmetic_imm_helper<T, operation_sub>(state, opcode);
}

//
// Helper: ADDQ, SUBQ
//
template <typename T, typename O>
INLINE void arithmetic_quick_helper(machine_state& state, uint16_t opcode)
{
    auto data = extract_bits<4, 3>(opcode);
    auto ea = extract_bits<10, 6>(opcode);

    auto ptr = state.get_pointer<T>(ea);
    auto val = state.read(ptr);

    if (is_address_register(uint32_t(ea)))
    {
        uint32_t result = O::template execute<uint32_t>(
            uint32_t(val),
            uint32_t(data));

        state.write<uint32_t>((uint32_t*)ptr, result);
    }
    else
    {
        typedef traits<T>::higher_precision_type_t high_precision_t;

        high_precision_t result_high_precision = O::template execute<high_precision_t>(
            high_precision_t(val),
            high_precision_t(data));

        T result = T(result_high_precision);

        bool carry = has_carry(result_high_precision);
        bool negative = is_negative(result);
        bool zero = (result == 0);
        bool overflow = has_overflow<T>(val, T(data), result);

        state.set_status_bit<bit::extend>(carry);
        state.set_status_bit<bit::negative>(negative);
        state.set_status_bit<bit::zero>(zero);
        state.set_status_bit<bit::overflow>(overflow);
        state.set_status_bit<bit::carry>(carry);

        state.write(ptr, result);
    }
}

//
// ADDQ
// Arithmetic add quick (literal)
//

template <typename T>
void addq(machine_state& state, uint16_t opcode)
{
    arithmetic_quick_helper<T, operation_add>(state, opcode);
}

//
// SUBQ
// Arithmetic subtract quick (literal)
//

template <typename T>
void subq(machine_state& state, uint16_t opcode)
{
    arithmetic_quick_helper<T, operation_sub>(state, opcode);
}

//
// Helper: ADDA, SUBA
//

template <typename T, typename O>
INLINE void arithmetic_address_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto src_ea = extract_bits<10, 6>(opcode);

    auto dst_ptr = state.get_pointer<uint32_t>(make_effective_address(1, dst_reg));
    auto src_ptr = state.get_pointer<T>(src_ea);

    auto dst_val = state.read(dst_ptr);
    auto src_val = state.read(src_ptr);

    auto src_val_sign_extended = sign_extend(src_val);
    auto result = O::template execute<uint32_t>(dst_val, src_val_sign_extended);

    state.write(dst_ptr, result);
}

//
// ADDA
// Arithmetic add address
//

template <typename T>
void adda(machine_state& state, uint16_t opcode)
{
    arithmetic_address_helper<T, operation_add>(state, opcode);
}

//
// SUBA
// Arithmetic subtract address
//

template <typename T>
void suba(machine_state& state, uint16_t opcode)
{
    arithmetic_address_helper<T, operation_sub>(state, opcode);
}

//
// Helper: ADDX, SUBX
//

template <typename T, uint16_t mode, typename O>
INLINE void arithmetic_extended_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T *src_ptr, *dst_ptr;

    switch (mode)
    {
    case 0: // Data register direct (0)
        src_ptr = state.get_pointer<T>(make_effective_address(0, src_reg));
        dst_ptr = state.get_pointer<T>(make_effective_address(0, dst_reg));
        break;
    case 1: // Address register indirect with predecrement (4)
        src_ptr = state.get_pointer<T>(make_effective_address(4, src_reg));
        dst_ptr = state.get_pointer<T>(make_effective_address(4, dst_reg));
        break;
    default:
        THROW("Invalid mode");
    }

    auto src_val = state.read(src_ptr);
    auto dst_val = state.read(dst_ptr);
    auto extend = state.get_status_bit<bit::extend>() ? 1 : 0;

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t tmp = 
        O::template execute<high_precision_t>(high_precision_t(src_val), high_precision_t(dst_val));
    
    high_precision_t result_high_precision =
        O::template execute<high_precision_t>(high_precision_t(tmp), high_precision_t(extend));

    T result = T(result_high_precision);

    bool carry = has_carry(result_high_precision);
    bool negative = is_negative(result);
    bool zero = (result == 0);
    bool overflow =
        has_overflow<T>(src_val, dst_val, T(tmp)) ||
        has_overflow<T>(T(tmp), extend, result);

    state.set_status_bit<bit::extend>(carry);
    state.set_status_bit<bit::negative>(negative);
    state.set_status_bit<bit::zero>(zero);
    state.set_status_bit<bit::overflow>(overflow);
    state.set_status_bit<bit::carry>(carry);

    state.write(dst_ptr, result);
}

//
// ADDX
// Arithmetic add extended
//

template <typename T, uint16_t mode>
void addx(machine_state& state, uint16_t opcode)
{
    arithmetic_extended_helper<T, mode, operation_add>(state, opcode);
}

//
// SUBX
// Arithmetic subtract extended
//

template <typename T, uint16_t mode>
void subx(machine_state& state, uint16_t opcode)
{
    arithmetic_extended_helper<T, mode, operation_sub>(state, opcode);
}

struct operation_or { template <typename T> static T execute(T a, T b) { return a | b; } };
struct operation_eor { template <typename T> static T execute(T a, T b) { return a ^ b; } };
struct operation_and { template <typename T> static T execute(T a, T b) { return a & b; } };

//
// Helper: ORI, EORI, ANDI to CCR
//

template <typename O>
INLINE void logical_immediate_to_ccr_helper(machine_state& state, uint16_t opcode)
{
    auto imm = state.next<uint16_t>();
    auto ptr = state.get_pointer<uint8_t>(reg::status_register);
    auto ccr = state.read(ptr);
    uint8_t result = O::template execute<uint8_t>(ccr, uint8_t(imm));
    state.write(ptr, result);
}

//
// ORI to CCR
// Logical or immediate to condition code register
//

void ori_to_ccr(machine_state& state, uint16_t opcode)
{
    logical_immediate_to_ccr_helper<operation_or>(state, opcode);
}

//
// ANDI to CCR
// Logical and immediate to condition code register
//

void andi_to_ccr(machine_state& state, uint16_t opcode)
{
    logical_immediate_to_ccr_helper<operation_and>(state, opcode);
}

//
// EORI to CCR
// Logical exclusive or immediate to condition code register
//

void eori_to_ccr(machine_state& state, uint16_t opcode)
{
    logical_immediate_to_ccr_helper<operation_eor>(state, opcode);
}

//
// Helper: ORI, EORI, ANDI to SR
//

template <typename O>
INLINE void logical_immediate_to_sr_helper(machine_state& state, uint16_t opcode)
{
    auto imm = state.next<uint16_t>();

    CHECK_SUPERVISOR(state);
    auto ptr = state.get_pointer<uint16_t>(reg::status_register);
    auto sr = state.read(ptr);
    uint16_t result = O::template execute<uint16_t>(sr, imm);
    state.write(ptr, result);
}

//
// ORI to SR
// Logical or immediate to status register
//

void ori_to_sr(machine_state& state, uint16_t opcode)
{
    logical_immediate_to_sr_helper<operation_or>(state, opcode);
}

//
// ANDI to SR
// Logical and immediate to status register
//

void andi_to_sr(machine_state& state, uint16_t opcode)
{
    logical_immediate_to_sr_helper<operation_and>(state, opcode);
}

//
// EORI to SR
// Logical exclusive or immeditate to status register
//

void eori_to_sr(machine_state& state, uint16_t opcode)
{
    logical_immediate_to_sr_helper<operation_eor>(state, opcode);
}

//
// Helper: ORI, EORI, ANDI
//
template <typename T, typename O>
INLINE void logical_immediate_helper(machine_state& state, uint16_t opcode)
{
    auto dst_ea = extract_bits<10, 6>(opcode);

    auto ptr = state.get_pointer<T>(dst_ea);
    auto val = state.read(ptr);

    typedef traits<T>::extension_word_type_t extension_t;
    auto imm = state.next<extension_t>();
    T result = O::template execute<T>(val, T(imm));

    state.set_status_bit<bit::negative>(most_significant_bit(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write(ptr, result);
}

//
// ORI
// Logical or immediate
//

template <typename T>
void ori(machine_state& state, uint16_t opcode)
{
    logical_immediate_helper<T,  operation_or>(state, opcode);
}

//
// ANDI
// Logical and immediate
//

template <typename T>
void andi(machine_state& state, uint16_t opcode)
{
    logical_immediate_helper<T, operation_and>(state, opcode);
}

//
// EORI
// Logical exclusive or immeditate
//

template <typename T>
void eori(machine_state& state, uint16_t opcode)
{
    logical_immediate_helper<T, operation_eor>(state, opcode);
}

//
// Helper: OR, EOR, AND
//
template <uint16_t dir, typename T, typename O>
INLINE void logical_helper(machine_state& state, uint16_t opcode)
{
    auto ptr_reg = state.get_pointer<T>(make_effective_address(0, extract_bits<4, 3>(opcode)));
    auto ptr_ea = state.get_pointer<T>(extract_bits<10, 6>(opcode));

    auto val_reg = state.read(ptr_reg);
    auto val_ea = state.read(ptr_ea);

    T result = O::template execute<T>(val_reg, val_ea);

    state.set_status_bit<bit::negative>(most_significant_bit(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    switch (dir)
    {
    case 0:
        state.write(ptr_reg, result);
        break;
    case 1:
        state.write(ptr_ea, result);
        break;
    default:
        THROW("Invalid direction");
    }
}

//
// OR
// Logical or
//

template <uint16_t dir, typename T>
void _or(machine_state& state, uint16_t opcode)
{
    logical_helper<dir, T, operation_or>(state, opcode);
}

//
// AND
// Logical and
//

template <uint16_t dir, typename T>
void _and(machine_state& state, uint16_t opcode)
{
    logical_helper<dir, T, operation_and>(state, opcode);
}

//
// EOR
// Logical exclusive or
//

template <typename T>
void eor(machine_state& state, uint16_t opcode)
{
    logical_helper<1, T, operation_eor>(state, opcode);
}

//
// Helper: NEGX, NEG
//

template <typename T, bool use_extend>
INLINE void negate_helper(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    auto ptr = state.get_pointer<T>(ea);
    auto val = state.read<T>(ptr);

    T extend = (use_extend && state.get_status_bit<bit::extend>()) ? T(1) : T(0);

    val = negate(val);
    extend = negate(extend);

    typedef traits<T>::higher_precision_type_t high_precition_t;

    high_precition_t result_high_precision = 
        high_precition_t(val) + 
        high_precition_t(extend);

    T result = (T)(result_high_precision);

    bool non_zero = result != 0;
    bool negative = is_negative(result);
    bool overflow = has_overflow(val, extend, result);

    state.set_status_bit<bit::extend>(non_zero);
    state.set_status_bit<bit::negative>(negative);
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(overflow);
    state.set_status_bit<bit::carry>(non_zero);

    state.write<T>(ptr, result);
}

//
// NEG
// Negate
//

template <typename T>
void neg(machine_state& state, uint16_t opcode)
{
    negate_helper<T, false>(state, opcode);
}

//
// NEGX
// Negate with extend
//

template <typename T>
void negx(machine_state& state, uint16_t opcode)
{
    negate_helper<T, true>(state, opcode);
}


//
// Helper: DIVU, DIVS
//

template <typename TDenom, typename TNum, const TNum min_val, const TNum max_val>
INLINE void divide_helper(machine_state& state, uint16_t opcode)
{
    // Denominator (16 bits)
    auto denom_ea = extract_bits<10, 6>(opcode);
    auto denom_ptr = state.get_pointer<uint16_t>(denom_ea);
    auto denom_val = state.read<TDenom>((TDenom*)denom_ptr);

    // Always clear carry flag
    state.set_status_bit<bit::carry>(false);

    if (denom_val == 0)
    {
        state.exception(5 /* Divide by zero */);
    }
    else
    {
        // Numerator (32 bits)
        auto num_reg = extract_bits<4, 3>(opcode);
        auto num_ptr = state.get_pointer<uint32_t>(make_effective_address(0, num_reg));
        auto num_val = state.read<TNum>((TNum*)num_ptr);

        // Divide
        auto quotient = num_val / TNum(denom_val);

        // Does the result overflow
        bool overflow = (quotient > max_val) || (quotient < min_val);
        state.set_status_bit<bit::overflow>(overflow);

        if (!overflow)
        {
            // Generate result (quotient + remainder)
            uint32_t quotient_unsigned = uint32_t(quotient);
            uint32_t remainder = uint32_t(num_val % TNum(denom_val));
            uint32_t result = ((remainder << 16) & 0xffff0000) | (quotient_unsigned & 0xffff);

            // Update status bits
            state.set_status_bit<bit::negative>(is_negative(quotient_unsigned));
            state.set_status_bit<bit::zero>(quotient == 0);

            // Write result
            state.write<uint32_t>((uint32_t*)num_ptr, result);
        }
    }
}

//
// DIVU
// Unsigned divide
//

void divu(machine_state& state, uint16_t opcode)
{
    divide_helper<uint16_t, uint32_t, 0, 65535>(state, opcode);
}

//
// DIVS
// Signed divide
//

void divs(machine_state& state, uint16_t opcode)
{
    divide_helper<int16_t, int32_t, -32768, 32767>(state, opcode);
}

//
// JMP
// Unconditional jump
//

void jmp(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    
    uint32_t* jump_to_ptr = state.get_pointer<uint32_t>(ea);
    uint32_t jump_to = state.read(jump_to_ptr);

    state.set_program_counter(jump_to);
}

//
// JSR
// Jump to subroutine
//

void jsr(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);

    uint32_t* jump_to_ptr = state.get_pointer<uint32_t>(ea);
    uint32_t jump_to = state.read(jump_to_ptr);

    state.push_program_counter();
    state.set_program_counter(jump_to);
}

//
// RTS
// Return from subroutine
//

void rts(machine_state& state, uint16_t opcode)
{
    state.pop_program_counter();
}

//
// RTR
// Return and restore condition codes
//

void rtr(machine_state& state, uint16_t opcode)
{
    // Pop the status register (SR) off the stack, but only set the condition code register (CCR)
    auto status_reg = state.pop<uint16_t>();
    state.set_condition_code_register(uint8_t(status_reg & 0xff));

    state.pop_program_counter();
}

//
// RTE
// Return from exception
//

void rte(machine_state& state, uint16_t opcode)
{
    CHECK_SUPERVISOR(state);
    state.pop_status_register();
    state.pop_program_counter();
}

//
// LINK
// Link and allocate
//

void link(machine_state& state, uint16_t opcode)
{
    // The contents of the specified address register is pushed onto the stack
    auto reg = extract_bits<13, 3>(opcode);
    auto ptr = state.get_pointer<uint32_t>(make_effective_address(1, reg));
    auto val = state.read<uint32_t>(ptr);
    state.push<uint32_t>(val);

    // Then, the address register is loaded with the updated stack pointer
    auto stack_ptr_ptr = state.get_pointer<uint32_t>(make_effective_address(1, 7));
    auto stack_ptr = state.read<uint32_t>(stack_ptr_ptr);
    state.write<uint32_t>(ptr, stack_ptr);

    //  Finally, the 16-bit sign-extended displacement is added to the stack pointer
    auto displacement = state.next<uint16_t>();
    auto displacement_sign_extended = sign_extend(displacement);
    auto new_stack_ptr = stack_ptr + displacement_sign_extended;
    state.write<uint32_t>(stack_ptr_ptr, new_stack_ptr);
}

//
// UNLK
// Unlink
//

void unlk(machine_state& state, uint16_t opcode)
{
    // The stack pointer is loaded from the specified address register...
    auto reg = extract_bits<13, 3>(opcode);
    auto ptr = state.get_pointer<uint32_t>(make_effective_address(1, reg));
    auto new_stack_ptr = state.read<uint32_t>(ptr);

    // ... and the old contents of the stack pointer is lost
    auto stack_ptr_ptr = state.get_pointer<uint32_t>(make_effective_address(1, 7));
    state.write<uint32_t>(stack_ptr_ptr, new_stack_ptr);

    // The address register is then loaded with the longword pulled off the stack.
    auto val = state.pop<uint32_t>();
    state.write<uint32_t>(ptr, val);
}

//
// BRA
// Branch always
//

void bra(machine_state& state, uint16_t opcode)
{
    int32_t displacement = int8_t(extract_bits<8, 8>(opcode));
    if (displacement == 0)
    {
        displacement = int32_t(state.next<int16_t>());
    }
    state.offset_program_counter(displacement);
}

//
// BSR
// Branch to subroutine
//

void bsr(machine_state& state, uint16_t opcode)
{
    int32_t displacement = int8_t(extract_bits<8, 8>(opcode));
    if (displacement == 0)
    {
        displacement = int32_t(state.next<int16_t>());
    }
    state.push_program_counter();
    state.offset_program_counter(displacement);
}

//
// TRAP
// Trap (software interrupt)
//

void trap(machine_state& state, uint16_t opcode)
{
    auto vector = extract_bits<12, 4>(opcode);
    state.exception(32 + vector); // Note: Software traps are using entry 32 - 47 in the vector table
}

//
// TRAPV
// Trap on overflow
//

void trapv(machine_state& state, uint16_t opcode)
{
    if (state.get_status_bit<bit::overflow>())
    {
        state.exception(7 /* TRAPV */);
    }
}

//
// ILLEGAL
// Illegal instruction
//

void illegal(machine_state& state, uint16_t opcode)
{
    state.exception(4 /* Illegal instruction */);
}

struct operation_btst { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val; } };
struct operation_bchg { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val ^ (1 << bit_index); } };
struct operation_bclr { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val & (~(1 << bit_index)); } };
struct operation_bset { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val | (1 << bit_index); } };

//
// Helper: BTST, BCHG, BCLR, BSET
//

template <uint16_t mode, typename O>
INLINE void bitop_helper(machine_state& state, uint16_t opcode)
{
    uint32_t bit_index = 0;
    switch (mode)
    {
    case 0: // Immediate
    {
        bit_index = uint32_t(state.next<uint16_t>());
    }
    break;

    case 1: // Data register
    {
        auto src_reg = extract_bits<4, 3>(opcode);
        uint32_t* ptr = state.get_pointer<uint32_t>(make_effective_address(0, src_reg));
        bit_index = state.read(ptr);
    }
    break;

    default:
        THROW("Invalid mode");
    }

    auto dst_ea = extract_bits<10, 6>(opcode);
    auto dst_mode = (dst_ea >> 3) & 0x7;

    static uint32_t modulo[8] = { 0x1f, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7 }; // Modulo 32 for data registers, otherwise module 8 (for memory locations)
    bit_index &= modulo[dst_mode];

    uint32_t* dst_ptr = state.get_pointer<uint32_t>(dst_ea);
    uint32_t dst = state.read(dst_ptr);

    auto result = O::template execute(dst, bit_index);

    const uint32_t mask = (1 << bit_index);
    state.set_status_bit<bit::zero>((result & mask) == 0);

    state.write(dst_ptr, result);
}

//
// BTST
// Test bit
//

template <uint16_t mode>
void btst(machine_state& state, uint16_t opcode)
{
    bitop_helper<mode, operation_btst>(state, opcode);
}

//
// BSET
// Test and set bit
//

template <uint16_t mode>
void bset(machine_state& state, uint16_t opcode)
{
    bitop_helper<mode, operation_bset>(state, opcode);
}

//
// BCLR
// Test and clear bit
//

template <uint16_t mode>
void bclr(machine_state& state, uint16_t opcode)
{
    bitop_helper<mode, operation_bclr>(state, opcode);
}

//
// BCHG
// Test and change (flip) bit
//

template <uint16_t mode>
void bchg(machine_state& state, uint16_t opcode)
{
    bitop_helper<mode, operation_bchg>(state, opcode);
}

//
// NOT
// Logical not
//

template <typename T>
void _not(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);

    auto ptr = state.get_pointer<T>(ea);
    T value = state.read(ptr);
    T result = ~value;

    state.set_status_bit<bit::negative>(is_negative(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write<T>(ptr, result);
}


//
// LEA
// Load effective address
//

void lea(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto src_ea = extract_bits<10, 6>(opcode);

    uint32_t* src_ptr = state.get_pointer<uint32_t>(src_ea);
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(make_effective_address(1 /* Address register direct */, dst_reg));

    uint32_t offset = state.pointer_to_memory_offset(src_ptr);
    state.write(dst_ptr, offset);
}

//
// PEA
// Push effective address onto stack
//

void pea(machine_state& state, uint16_t opcode)
{
    auto src_ea = extract_bits<10, 6>(opcode);

    uint32_t* src_ptr = state.get_pointer<uint32_t>(src_ea);

    uint32_t offset = state.pointer_to_memory_offset(src_ptr);
    state.push(offset);
}

//
// CHK
// Trap if value is out of specified bounds
//

void chk(machine_state& state, uint16_t opcode)
{
    auto value_reg = extract_bits<4, 3>(opcode);
    int16_t* value_ptr = (int16_t*)state.get_pointer<uint16_t>(make_effective_address(0 /* Data register direct */, value_reg));
    int16_t value = state.read(value_ptr);

    auto src_ea = extract_bits<10, 6>(opcode);
    auto src_ptr = state.get_pointer<uint16_t>(src_ea);
    auto upper_bound = state.read(src_ptr);

    if (value < 0 || value > upper_bound)
    {
        state.exception(6 /* CHK out of bounds */);
    }
}

//
// Helper: CMPI, CMPM, CMPA, CMP
//

template <typename T>
INLINE void cmp_helper(machine_state& state, T a, T b)
{
    T result = a - b;

    state.set_status_bit<bit::negative>(is_negative<T>(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(has_overflow(a, b, result));
    state.set_status_bit<bit::carry>(has_borrow<T>(a, b));
}

//
// CMPI
// Compare immediate
//

template <typename T>
void cmpi(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    T* ptr = state.get_pointer<T>(ea);
    T value = state.read(ptr);

    typedef traits<T>::extension_word_type_t extension_t;
    extension_t imm = state.next<extension_t>();

    cmp_helper<T>(state, value, T(imm));
}

//
// CMPM
// Compare memory with memory
//

template <typename T>
void cmpm(machine_state& state, uint16_t opcode)
{
    auto src_reg = extract_bits<13, 3>(opcode);
    auto dst_reg = extract_bits<4, 3>(opcode);
 
    // Note: Address register indirect with postincrement
    auto src_ptr = state.get_pointer<T>(make_effective_address(3, src_reg));
    auto dst_ptr = state.get_pointer<T>(make_effective_address(3, dst_reg));
 
    T src_val = state.read<T>(src_ptr);
    T dst_val = state.read<T>(dst_ptr);

    cmp_helper<T>(state, dst_val, src_val);
}

//
// CMPA
// Compare address
//

template <typename T>
void cmpa(machine_state& state, uint16_t opcode)
{
    auto src_ea = extract_bits<10, 6>(opcode);
    auto dst_reg = extract_bits<4, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_ea);
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(make_effective_address(1, dst_reg));

    uint32_t src_val = sign_extend<T>(state.read<T>(src_ptr));
    uint32_t dst_val = state.read<uint32_t>(dst_ptr);

    cmp_helper<uint32_t>(state, dst_val, src_val);
}

//
// CMP
// Compare
//

template <typename T>
void cmp(machine_state& state, uint16_t opcode)
{
    auto src_ea = extract_bits<10, 6>(opcode);
    auto dst_reg = extract_bits<4, 3>(opcode);

    auto src_ptr = state.get_pointer<T>(src_ea);
    auto dst_ptr = state.get_pointer<T>(make_effective_address(0, dst_reg));

    auto src_val = state.read(src_ptr);
    auto dst_val = state.read(dst_ptr);

    cmp_helper<T>(state, dst_val, src_val);
}

//
// EXT
// Sign extend a data register
//

template <typename T>
void ext(machine_state& state, uint16_t opcode)
{
    auto reg = extract_bits<13, 3>(opcode);

    typedef traits<T>::lower_precision_type_t low_precision_t;
    low_precision_t* ptr = state.get_pointer<low_precision_t>(make_effective_address(0, reg));
    low_precision_t value = state.read<low_precision_t>(ptr);

    T result = T(sign_extend<low_precision_t>(value));

    state.set_status_bit<bit::negative>(is_negative<T>(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write<T>((T*)ptr, result);
}

//
// SWAP
// Swap register halves
//

void swap(machine_state& state, uint16_t opcode)
{
    auto reg = extract_bits<13, 3>(opcode);

    uint32_t* ptr = state.get_pointer<uint32_t>(make_effective_address(0, reg));
    uint32_t value = state.read<uint32_t>(ptr);
    uint32_t result = (value >> 16) | (value << 16);

    state.set_status_bit<bit::negative>(most_significant_bit(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write<uint32_t>(ptr, result);
}

//
// TAS
// Test and set operand
//

void tas(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    
    auto ptr = state.get_pointer<uint8_t>(ea);
    auto val = state.read<uint8_t>(ptr);

    state.set_status_bit<bit::negative>(most_significant_bit(val));
    state.set_status_bit<bit::zero>(val == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    val |= (1 << 7);

    state.write<uint8_t>(ptr, val);
}

//
// TST
// Test and operand
//

template <typename T>
void tst(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    
    auto ptr = state.get_pointer<T>(ea);
    auto val = state.read<T>(ptr);

    state.set_status_bit<bit::negative>(is_negative(val));
    state.set_status_bit<bit::zero>(val == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);
}

//
// RESET
// Reset external devices
//

void reset(machine_state& state, uint16_t opcode)
{
    state.reset();
}

//
// NOP
// No operation
//

void nop(machine_state& state, uint16_t opcode)
{
}

//
// EXG
// Exchange registers
//

template <uint16_t operation>
void exg(machine_state& state, uint16_t opcode)
{
    uint32_t mode1, mode2;

    switch (operation)
    {
    case 0x8: // D/D
        mode1 = 0;
        mode2 = 0;
        break;

    case 0x9: // A/A
        mode1 = 1;
        mode2 = 1;
        break;

    case 0x11: // D/A
        mode1 = 0;
        mode2 = 1;
        break;

    default:
        THROW("Invalid operation");
    }

    auto reg1 = extract_bits<4, 3>(opcode);
    auto reg2 = extract_bits<13, 3>(opcode);

    auto ptr1 = state.get_pointer<uint32_t>(make_effective_address(mode1, reg1));
    auto ptr2 = state.get_pointer<uint32_t>(make_effective_address(mode2, reg2));

    auto val1 = state.read<uint32_t>(ptr1);
    auto val2 = state.read<uint32_t>(ptr2);

    state.write<uint32_t>(ptr1, val2);
    state.write<uint32_t>(ptr2, val1);
}

//
// STOP
// Load status register and stop
//

void stop(machine_state& state, uint16_t opcode)
{
    auto imm = state.next<uint16_t>();

    CHECK_SUPERVISOR(state);

    if (state.get_status_bit<bit::trace>())
    {
        state.exception(9 /* Trace exception */);
        return;
    }

    bool mode_change_attempt = (imm & (1 << uint32_t(bit::supervisor))) == 0;

    if (mode_change_attempt)
    {
        state.exception(8 /* Privilege violation */);
        return;
    }

    auto ptr = state.get_pointer<uint16_t>(reg::status_register);
    state.write<uint16_t>(ptr, imm);

    state.stop();
}

//
// Helper: Scc, Bcc
//

template <uint16_t condition>
INLINE bool evaluate_condition(machine_state& state)
{
    bool c = state.get_status_bit<bit::carry>();
    bool z = state.get_status_bit<bit::zero>();
    bool n = state.get_status_bit<bit::negative>();
    bool v = state.get_status_bit<bit::overflow>();

    switch (condition)
    {
    case 0x0: return true;                                // True (T)
    case 0x1: return false;                               // False (F)
    case 0x2: return !c && !z;                            // High (HI)
    case 0x3: return c || z;                              // Low or same (LS)
    case 0x4: return !c;                                  // Carry clear (CC)
    case 0x5: return c;                                   // Carry set (CS)
    case 0x6: return !z;                                  // Not equal (NE)
    case 0x7: return z;                                   // Equal (EQ)
    case 0x8: return !v;                                  // Overflow clear (VC)
    case 0x9: return v;                                   // Overflow set (VS)
    case 0xa: return !n;                                  // Plus (PL)
    case 0xb: return n;                                   // Minus (MI)
    case 0xc: return (n && v) || (!n && !v);              // Greater or equal (GE)
    case 0xd: return (n && !v) || (!n && v);              // Less than (LT)
    case 0xe: return (n && v && !z) || (!n && !v && !z);  // Greater than (GT)
    case 0xf: return z || (n && !v) || (!n && v);         // Less or equal (LE)
    default:
        THROW("Invalid condition: " << condition);
    }
}

//
// Scc
// Set on condition
//

template <uint16_t condition>
void scc(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    auto ptr = state.get_pointer<uint8_t>(ea);

    bool result = evaluate_condition<condition>(state);
    state.write<uint8_t>(ptr, uint8_t(result ? 0xff : 0x00));
}

//
// Bcc
// Branch on condition
//

template <uint16_t condition>
void bcc(machine_state& state, uint16_t opcode)
{
    bool result = evaluate_condition<condition>(state);

    if (result)
    {
        int32_t displacement = int8_t(extract_bits<8, 8>(opcode));
        if (displacement == 0)
        {
            displacement = int32_t(state.next<int16_t>());
        }
        state.offset_program_counter(displacement);
    }
}

//
// DBcc
// Test condition, decrement and branch
//

template <uint16_t condition>
void dbcc(machine_state& state, uint16_t opcode)
{
    bool result = evaluate_condition<condition>(state);
    auto displacement = int32_t(state.next<int16_t>());
    
    if (result)
    {
        auto reg = extract_bits<13, 3>(opcode);
        uint16_t* ptr = state.get_pointer<uint16_t>(make_effective_address(0, reg));
        int16_t val = state.read<int16_t>((int16_t*)ptr) - int16_t(1);
        state.write(ptr, uint16_t(val));

        if (val != -1)
        {
            state.offset_program_counter(displacement);
        }
    }
}

//
// Helper: MULU, MULS
//
template <typename TOperand, typename TResult>
INLINE void mul_helper(machine_state& state, uint16_t opcode)
{
    auto src_ea = extract_bits<10, 6>(opcode);
    auto dst_reg = extract_bits<4, 3>(opcode);

    auto src_ptr = state.get_pointer<uint16_t>(src_ea);
    auto dst_ptr = state.get_pointer<uint16_t>(make_effective_address(0, dst_reg));

    TOperand src_val = TOperand(state.read(src_ptr));
    TOperand dst_val = TOperand(state.read(dst_ptr));

    TResult result = TResult(src_val) * TResult(dst_val);

    state.set_status_bit<bit::negative>(is_negative(uint32_t(result)));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write<uint32_t>((uint32_t*)dst_ptr, result);
}

//
// MULU
// Unsigned multiply
//

void mulu(machine_state& state, uint16_t opcode)
{
    mul_helper<uint16_t, uint32_t>(state, opcode);
}

//
// MULS
// Signed multiply
//

void muls(machine_state& state, uint16_t opcode)
{
    mul_helper<int16_t, int32_t>(state, opcode);
}

//
// Helpers: Shift/rotate operations
//

struct operation_rotate_left
{
    template <typename T>
    static T execute(T value, uint32_t shift, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_left(value, shift);
        return rotate_left<T>(value, shift);
    }
};

struct operation_rotate_right
{
    template <typename T>
    static T execute(T value, uint32_t shift, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_right(value, shift);
        return rotate_right<T>(value, shift);
    }
};

struct operation_shift_left
{
    template <typename T>
    static T execute(T value, uint32_t shift, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_left(value, shift);
        return value << shift;
    }
};

struct operation_shift_right
{
    template <typename T>
    static T execute(T value, uint32_t shift, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_right(value, shift);
        return value >> shift;
    }
};

//
// ASx (memory)
// Arithmetic shift
//

template <uint16_t direction>
void asx_mem(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    auto ptr = state.get_pointer<uint16_t>(ea);
    auto val = int16_t(state.read(ptr));
    auto msr_before = most_significant_bit(val);

    bool last_out;
    switch (direction)
    {
    case 0: val = operation_shift_right::execute<int16_t>(val, 1, last_out); break;   // Right
    case 1: val = operation_shift_left::execute<int16_t>(val, 1, last_out); break;    // left
    default:
        THROW("Invalid direction");
    }

    auto msr_after = most_significant_bit(val);

    state.set_status_bit<bit::extend>(last_out);
    state.set_status_bit<bit::negative>(msr_after);
    state.set_status_bit<bit::zero>(val == 0);
    state.set_status_bit<bit::overflow>(msr_before != msr_after);
    state.set_status_bit<bit::carry>(last_out);

    state.write<uint16_t>(ptr, uint16_t(val));
}

//
// LSx (memory)
// Logical shift
//

template <uint16_t direction>
void lsx_mem(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    auto ptr = state.get_pointer<uint16_t>(ea);
    auto val = state.read<uint16_t>(ptr);

    bool last_out;
    switch (direction)
    {
    case 0: val = operation_shift_right::execute<uint16_t>(val, 1, last_out); break;   // Right
    case 1: val = operation_shift_left::execute<uint16_t>(val, 1, last_out); break;    // left
    default:
        THROW("Invalid direction");
    }

    state.set_status_bit<bit::extend>(last_out);
    state.set_status_bit<bit::negative>(is_negative(val));
    state.set_status_bit<bit::zero>(val == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(last_out);

    state.write<uint16_t>(ptr, val);
}

//
// ROXx (memory)
// Rotate with extend
//

template <uint16_t direction>
void roxx_mem(machine_state& state, uint16_t opcode)
{
    THROW("ROXx not implemented");
}

//
// ROx (memory)
// Rotate
//

template <uint16_t direction>
void rox_mem(machine_state& state, uint16_t opcode)
{
    auto ea = extract_bits<10, 6>(opcode);
    auto ptr = state.get_pointer<uint16_t>(ea);
    auto val = state.read<uint16_t>(ptr);

    bool last_out;
    switch (direction)
    {
    case 0: val = operation_rotate_right::execute<uint16_t>(val, 1, last_out); break;   // Right
    case 1: val = operation_rotate_left::execute<uint16_t>(val, 1, last_out); break;    // left
    default:
        THROW("Invalid direction");
    }

    state.set_status_bit<bit::negative>(most_significant_bit(val));
    state.set_status_bit<bit::zero>(val == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(last_out);

    state.write<uint16_t>(ptr, val);
}

//
// ASx (register)
// Artihmetic shift
//

template <uint16_t direction, typename T, uint16_t mode>
void asx_reg(machine_state& state, uint16_t opcode)
{
    THROW("ASx not implemented");
}

//
// LSx (register)
// Logical shift
//

template <uint16_t direction, typename T, uint16_t mode>
void lsx_reg(machine_state& state, uint16_t opcode)
{
    THROW("LSx not implemented");
}

//
// ROXx (register)
// Rotate with extend
//

template <uint16_t direction, typename T, uint16_t mode>
void roxx_reg(machine_state& state, uint16_t opcode)
{
    THROW("ROXx not implemented");
}

//
// ROx (register)
// Rotate with extend
//

template <uint16_t direction, typename T, uint16_t mode>
void rox_reg(machine_state& state, uint16_t opcode)
{
    THROW("ROx not implemented");
}

//
// NBCD
// Negate decimal with sign extend
//

void nbcd(machine_state& state, uint16_t opcode)
{
    THROW("NBCD not implemented");
}

//
// SBCD
// Subtract decimal with extend
//

template <uint16_t mode>
void sbcd(machine_state& state, uint16_t opcode)
{
    THROW("SBCD not implemented");
}

//
// ABCD
// Add decimal with extend
//

template <uint16_t mode>
void abcd(machine_state& state, uint16_t opcode)
{
    THROW("ABCD not implemented");
}