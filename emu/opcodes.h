#pragma once
#include <vector>
#include <cstdint>
#include "machinestate.h"

void make_opcode_table(std::vector<inst_func_ptr_t>& table);
