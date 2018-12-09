#pragma once

#include "machinestate.h"

void inst_move(machine_state& state, uint16_t opcode);
void inst_moveq(machine_state& state, uint16_t opcode);
void inst_clr(machine_state& state, uint16_t opcode);
void inst_movea(machine_state& state, uint16_t opcode);
void inst_add(machine_state& state, uint16_t opcode);
void inst_and(machine_state& state, uint16_t opcode);
void inst_eor(machine_state& state, uint16_t opcode);
void inst_or(machine_state& state, uint16_t opcode);
void inst_ori(machine_state& state, uint16_t opcode);
void inst_andi(machine_state& state, uint16_t opcode);
void inst_eori(machine_state& state, uint16_t opcode);
void inst_subi(machine_state& state, uint16_t opcode);
void inst_addi(machine_state& state, uint16_t opcode);
void inst_cmpi(machine_state& state, uint16_t opcode);
void inst_btst(machine_state& state, uint16_t opcode);
void inst_bset(machine_state& state, uint16_t opcode);
void inst_bclr(machine_state& state, uint16_t opcode);
void inst_bchg(machine_state& state, uint16_t opcode);
void inst_jmp(machine_state& state, uint16_t opcode);
void inst_addq(machine_state& state, uint16_t opcode);

