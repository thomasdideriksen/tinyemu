#pragma once

#include "machinestate.h"

void inst_move(machine_state& state, uint16_t opcode);
void inst_moveq(machine_state& state, uint16_t opcode);
void inst_rte(machine_state& state, uint16_t opcode);
void inst_clr(machine_state& state, uint16_t opcode);
void inst_movea(machine_state& state, uint16_t opcode);
void inst_movep(machine_state& state, uint16_t opcode);
void inst_negx(machine_state& state, uint16_t opcode);
void inst_add(machine_state& state, uint16_t opcode);
void inst_and(machine_state& state, uint16_t opcode);