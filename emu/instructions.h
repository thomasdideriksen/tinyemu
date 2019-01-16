#pragma once

#include "machinestate.h"

//
// MOVE
//

template <typename T, uint16_t src, uint16_t dst>
void move(machine_state& state)
{
    T* src_ptr = state.get_pointer<T>(src);
    T* dst_ptr = state.get_pointer<T>(swap_effective_address<dst>());

    T result = state.read(src_ptr);

    state.set_status_bit<bit::negative>(is_negative(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write(dst_ptr, result);
}

//
// MOVE from SR
//

template <uint16_t dst>
void move_from_sr(machine_state& state)
{
    auto src_ptr = state.get_pointer<uint16_t>(reg::status_register);
    auto dst_ptr = state.get_pointer<uint16_t>(dst);
    auto result = state.read(src_ptr);
    state.write(dst_ptr, result);
}

//
// MOVE to CCR
// 

template <uint16_t src>
void move_to_ccr(machine_state& state)
{
    auto src_ptr = state.get_pointer<uint8_t>(src);
    auto dst_ptr = state.get_pointer<uint8_t>(reg::status_register);
    auto result = state.read(src_ptr);
    state.write(dst_ptr, result);
}

//
// MOVE to SR
//

template <uint16_t src>
void move_to_sr(machine_state& state)
{
    CHECK_SUPERVISOR(state);
    auto src_ptr = state.get_pointer<uint16_t>(src);
    auto dst_ptr = state.get_pointer<uint16_t>(reg::status_register);
    auto result = state.read(src_ptr);
    state.write(dst_ptr, result);
}

//
// MOVE USP
//
template <uint16_t dir, uint16_t ea>
void move_usp(machine_state& state)
{
    CHECK_SUPERVISOR(state);
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
//

template <uint16_t dst, uint16_t data>
void moveq(machine_state& state)
{   
    auto dst_ptr = state.get_pointer<uint32_t>(make_effective_address<0, dst>());
    auto result = sign_extend(data);

    state.set_status_bit<bit::negative>(is_negative(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write(dst_ptr, result);
}

//
// MOVEA
//

template <typename T, uint16_t dst, uint16_t src>
void movea(machine_state& state)
{
    auto src_ptr = state.get_pointer<T>(src);
    auto dst_ptr = state.get_pointer<uint32_t>(make_effective_address<1, dst>());
    auto val = state.read<T>(src_ptr);
    state.write<uint32_t>(dst_ptr, sign_extend(val));
}

//
// MOVEM
//

template <uint16_t dir, typename T, uint16_t src>
void movem(machine_state& state)
{
    auto register_select = state.next<uint16_t>(); // Note: Get this first so we don't interfere with addressing modes, etc.
    auto mem_mode = (src >> 3) & 0x7;

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
            T* mem = state.get_pointer<T>(src);

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
//

template <uint16_t src, uint16_t dir, typename T, uint16_t dst>
void movep(machine_state& state)
{
    auto reg_ptr = state.get_pointer<uint8_t>(make_effective_address<0, src>()) + sizeof(T) - 1; // Note: Endian
    auto mem_ptr = state.get_pointer<uint8_t>(make_effective_address<2, dst>()) + state.next<int16_t>();

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
//

template <typename T, uint16_t ea>
void clr(machine_state& state)
{
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

template <uint16_t reg, uint16_t mode, typename T, uint16_t ea, typename O>
INLINE void arithmetic_helper(machine_state& state)
{
    T *src, *dst;

    switch (mode)
    {
    case 0: // Write to D register
        src = state.get_pointer<T>(ea);
        dst = state.get_pointer<T>(make_effective_address<0, reg>());
        break;
    case 1: // Write to effective address
        src = state.get_pointer<T>(make_effective_address<0, reg>());
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
//

template <uint16_t reg, uint16_t mode, typename T, uint16_t ea>
void add(machine_state& state)
{
    arithmetic_helper<reg, mode, T, ea, operation_add>(state);
}

//
// SUB
//

template <uint16_t reg, uint16_t mode, typename T, uint16_t ea>
void sub(machine_state& state)
{
    arithmetic_helper<reg, mode, T, ea, operation_sub>(state);
}

//
// Helper: ADDI, SUBI
//

template <typename T, uint16_t ea, typename O>
INLINE void arithmetic_imm_helper(machine_state& state)
{
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
//

template <typename T, uint16_t ea>
void addi(machine_state& state)
{
    arithmetic_imm_helper<T, ea, operation_add>(state);
}

//
// SUBI
//

template <typename T, uint16_t ea>
void subi(machine_state& state)
{
    arithmetic_imm_helper<T, ea, operation_sub>(state);
}

//
// Helper: ADDQ, SUBQ
//
template <uint16_t data, typename T, uint16_t dst, typename O>
INLINE void arithmetic_quick_helper(machine_state& state)
{
    auto ptr = state.get_pointer<T>(dst);
    auto val = state.read(ptr);

    if (is_address_register(uint32_t(dst)))
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
//
template <uint16_t data, typename T, uint16_t dst>
void addq(machine_state& state)
{
    arithmetic_quick_helper<data, T, dst, operation_add>(state);
}

//
// SUBQ
//
template <uint16_t data, typename T, uint16_t dst>
void subq(machine_state& state)
{
    arithmetic_quick_helper<data, T, dst, operation_sub>(state);
}

//
// Helper: ADDA, SUBA
//
template <uint16_t dst, typename T, uint16_t src_ea, typename O>
INLINE void arithmetic_address_helper(machine_state& state)
{
    auto dst_ptr = state.get_pointer<uint32_t>(make_effective_address<1, dst>());
    auto src_ptr = state.get_pointer<T>(src_ea);

    auto dst_val = state.read(dst_ptr);
    auto src_val = state.read(src_ptr);

    auto src_val_sign_extended = sign_extend(src_val);
    auto result = O::template execute<uint32_t>(dst_val, src_val_sign_extended);

    state.write(dst_ptr, result);
}

//
// ADDA
//

template <uint16_t dst, typename T, uint16_t src_ea>
void adda(machine_state& state)
{
    arithmetic_address_helper<dst, T, src_ea, operation_add>(state);
}

//
// SUBA
//

template <uint16_t dst, typename T, uint16_t src_ea>
void suba(machine_state& state)
{
    arithmetic_address_helper<dst, T, src_ea, operation_sub>(state);
}

//
// Helper: ADDX, SUBX
//

template <uint16_t dst, typename T, uint16_t mode, uint16_t src, typename O>
INLINE void arithmetic_extended_helper(machine_state& state)
{
    T *src_ptr, *dst_ptr;

    switch (mode)
    {
    case 0: // Data register direct (0)
        src_ptr = state.get_pointer<T>(make_effective_address<0, src>());
        dst_ptr = state.get_pointer<T>(make_effective_address<0, dst>());
        break;
    case 1: // Address register indirect with predecrement (4)
        src_ptr = state.get_pointer<T>(make_effective_address<4, src>());
        dst_ptr = state.get_pointer<T>(make_effective_address<4, dst>());
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
//

template <uint16_t dst, typename T, uint16_t mode, uint16_t src>
void addx(machine_state& state)
{
    arithmetic_extended_helper<dst, T, mode, src, operation_add>(state);
}

//
// SUBX
//

template <uint16_t dst, typename T, uint16_t mode, uint16_t src>
void subx(machine_state& state)
{
    arithmetic_extended_helper<dst, T, mode, src, operation_sub>(state);
}

//
// ORI to CCR
//
void ori_to_ccr(machine_state& state)
{
    auto imm = state.next<uint16_t>();
    auto ptr = state.get_pointer<uint8_t>(reg::status_register);
    auto ccr = state.read(ptr);
    uint8_t result = ccr | uint8_t(imm);
    state.write(ptr, result);
}

//
// ORI to SR
//
void ori_to_sr(machine_state& state)
{
    CHECK_SUPERVISOR(state);
    auto imm = state.next<uint16_t>();
    auto ptr = state.get_pointer<uint16_t>(reg::status_register);
    auto sr = state.read(ptr);
    uint16_t result = sr | imm;
    state.write(ptr, result);
}

//
// ORI
//
template <typename T, uint16_t dst_ea>
void ori(machine_state& state)
{
    auto ptr = state.get_pointer<T>(dst_ea);
    auto val = state.read(ptr);

    typedef traits<T>::extension_word_type_t extension_t;
    auto imm = state.next<extension_t>();
    T result = val | T(imm);

    state.set_status_bit<bit::negative>(most_significant_bit(result));
    state.set_status_bit<bit::zero>(result == 0);
    state.set_status_bit<bit::overflow>(false);
    state.set_status_bit<bit::carry>(false);

    state.write(ptr, result);
}

#if false
void inst_move(machine_state& state, uint16_t opcode);
void inst_moveq(machine_state& state, uint16_t opcode);
void inst_movea(machine_state& state, uint16_t opcode);
void inst_movem(machine_state& state, uint16_t opcode);
void inst_movep(machine_state& state, uint16_t opcode);

//
// Bitwise operators
//
void inst_and(machine_state& state, uint16_t opcode);
void inst_eor(machine_state& state, uint16_t opcode);
void inst_or(machine_state& state, uint16_t opcode);
void inst_ori(machine_state& state, uint16_t opcode);
void inst_andi(machine_state& state, uint16_t opcode);
void inst_eori(machine_state& state, uint16_t opcode);
void inst_not(machine_state& state, uint16_t opcode);
void inst_btst(machine_state& state, uint16_t opcode);
void inst_bset(machine_state& state, uint16_t opcode);
void inst_bclr(machine_state& state, uint16_t opcode);
void inst_bchg(machine_state& state, uint16_t opcode);
void inst_lsl_mem(machine_state& state, uint16_t opcode);
void inst_lsl_reg(machine_state& state, uint16_t opcode);
void inst_lsr_mem(machine_state& state, uint16_t opcode);
void inst_lsr_reg(machine_state& state, uint16_t opcode);
void inst_rol_mem(machine_state& state, uint16_t opcode);
void inst_ror_mem(machine_state& state, uint16_t opcode);
void inst_rol_reg(machine_state& state, uint16_t opcode);
void inst_ror_reg(machine_state& state, uint16_t opcode);
void inst_roxl_mem(machine_state& state, uint16_t opcode);
void inst_roxr_mem(machine_state& state, uint16_t opcode);
void inst_roxl_reg(machine_state& state, uint16_t opcode);
void inst_roxr_reg(machine_state& state, uint16_t opcode);
void inst_asl_mem(machine_state& state, uint16_t opcode);
void inst_asr_mem(machine_state& state, uint16_t opcode);
void inst_asl_reg(machine_state& state, uint16_t opcode);
void inst_asr_reg(machine_state& state, uint16_t opcode);

//
// Arithmetic operators
//
void inst_add(machine_state& state, uint16_t opcode);
void inst_subi(machine_state& state, uint16_t opcode);
void inst_addi(machine_state& state, uint16_t opcode);
void inst_addq(machine_state& state, uint16_t opcode);
void inst_subq(machine_state& state, uint16_t opcode);
void inst_neg(machine_state& state, uint16_t opcode);
void inst_negx(machine_state& state, uint16_t opcode);
void inst_divu(machine_state& state, uint16_t opcode);
void inst_divs(machine_state& state, uint16_t opcode);

//
// Branch/jump
//
void inst_jmp(machine_state& state, uint16_t opcode);
void inst_jsr(machine_state& state, uint16_t opcode);
void inst_rts(machine_state& state, uint16_t opcode);
void inst_rtr(machine_state& state, uint16_t opcode);
void inst_link(machine_state& state, uint16_t opcode);
void inst_unlk(machine_state& state, uint16_t opcode);
void inst_bra(machine_state& state, uint16_t opcode);
void inst_bsr(machine_state& state, uint16_t opcode);

//
// Exceptions
//
void inst_trap(machine_state& state, uint16_t opcode);
void inst_trapv(machine_state& state, uint16_t opcode);
void inst_rte(machine_state& state, uint16_t opcode);
void inst_illegal(machine_state& state, uint16_t opcode);


//
// Miscellaneous 
//
void inst_clr(machine_state& state, uint16_t opcode);
void inst_cmpi(machine_state& state, uint16_t opcode);
void inst_lea(machine_state& state, uint16_t opcode);
void inst_pea(machine_state& state, uint16_t opcode);
void inst_chk(machine_state& state, uint16_t opcode);
void inst_scc(machine_state& state, uint16_t opcode);
void inst_bcc(machine_state& state, uint16_t opcode);
void inst_dbcc(machine_state& state, uint16_t opcode);
void inst_ext(machine_state& state, uint16_t opcode);
void inst_swap(machine_state& state, uint16_t opcode);
void inst_tas(machine_state& state, uint16_t opcode);
void inst_tst(machine_state& state, uint16_t opcode);
void inst_reset(machine_state& state, uint16_t opcode);
void inst_nop(machine_state& state, uint16_t opcode);
void inst_exg(machine_state& state, uint16_t opcode);

#endif