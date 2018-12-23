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

    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    state.write(dst_ptr, result);
}

//
// MOVEQ
//

template <uint16_t dst, uint16_t data>
void moveq(machine_state& state)
{   
    auto dst_ptr = state.get_pointer<uint32_t>(make_effective_address<0, dst>());
    auto result = sign_extend(data);

    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

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