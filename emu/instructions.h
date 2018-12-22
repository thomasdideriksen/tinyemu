#pragma once

#include "machinestate.h"

//
// Move
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

template <uint16_t dst, uint16_t data>
void moveq(machine_state& state)
{

}

template <typename T, uint16_t dst, uint16_t src>
void movea(machine_state& state)
{

}

template <uint16_t dir, typename T, uint16_t src>
void movem(machine_state& state)
{

}

template <uint16_t src, uint16_t dir, typename T, uint16_t dst>
void movep(machine_state& state)
{

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