#include "instructions.h"
#include "common.h"
#include "machinestate.h"

#define PROCESS_SIZE(size, helper)\
    switch (size) {\
    case 0: ##helper<uint8_t>(state, opcode); break;\
    case 1: ##helper<uint16_t>(state, opcode); break;\
    case 2: ##helper<uint32_t>(state, opcode); break;\
    default: THROW("Invalid size");}

#define PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, helper, second_template_param)\
    switch (size) {\
    case 0: ##helper<uint8_t, second_template_param>(state, opcode); break;\
    case 1: ##helper<uint16_t, second_template_param>(state, opcode); break;\
    case 2: ##helper<uint32_t, second_template_param>(state, opcode); break;\
    default: THROW("Invalid size");}

//
// MOVE
// General purpose move
//

template <typename T>
inline void inst_move_helper(machine_state& state, uint16_t opcode)
{
    const auto dst_reg = extract_bits<4, 3>(opcode);
    const auto dst_mode = extract_bits<7, 3>(opcode);
    const auto src_mode = extract_bits<10, 3>(opcode);
    const auto src_reg = extract_bits<13, 3>(opcode);
    
    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    T* dst_ptr = state.get_pointer<T>(dst_mode, dst_reg);

    T src = state.read(src_ptr);

    state.set_status_register<bit::negative>(is_negative(src));
    state.set_status_register<bit::zero>(src == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);
    
    state.write(dst_ptr, src);
}

void inst_move(machine_state& state, uint16_t opcode)
{
    const auto size = extract_bits<2, 2>(opcode);
    switch (size)
    {
    case 1: inst_move_helper<uint8_t>(state, opcode); break;   // Byte
    case 2: inst_move_helper<uint32_t>(state, opcode); break;  // Long
    case 3: inst_move_helper<uint16_t>(state, opcode); break;  // Word
    default:
        THROW("Invalid move size: " << size);
    }
}

//
// MOVEQ
// Move quick, moves a small literal/immediate value
//

void inst_moveq(machine_state& state, uint16_t opcode)
{
    const auto dst_reg = extract_bits<4, 3>(opcode);
    const uint8_t data = uint8_t(extract_bits<8, 8>(opcode));
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(0, dst_reg);

    auto result = sign_extend(data);

    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    state.write(dst_ptr, result);
}

//
// CLR
// Clears (zeroes) operand
//

template <typename T>
void inst_clr_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    T* ptr = state.get_pointer<T>(mode, reg);

    state.set_status_register<bit::negative>(false);
    state.set_status_register<bit::zero>(true);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    state.write(ptr, T(0x0));
}

void inst_clr(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_clr_helper);
}

//
// MOVEA
// Move address
//

template <typename T>
inline void inst_movea_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(1 /* Always "address register direct" mode */, dst_reg);

    T src = state.read(src_ptr);
    state.write<uint32_t>(dst_ptr, sign_extend(src));
}

void inst_movea(machine_state& state, uint16_t opcode)
{
    const auto size = extract_bits<2, 2>(opcode);
    switch (size)
    {
    case 2: inst_movea_helper<uint32_t>(state, opcode); break; // Long
    case 3: inst_movea_helper<uint16_t>(state, opcode); break; // Word
    default:
        THROW("Invalid movea size");
    }
}

//
// ADD
// Binary addition
//

template <typename T>
inline void inst_add_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto direction = extract_bits<7, 1>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    T* dst_ptr = state.get_pointer<T>(0 /* Always "data register direct" mode */, dst_reg);

    T src = state.read(src_ptr);
    T dst = state.read(dst_ptr);

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t result_high_precision =
        high_precision_t(src) +
        high_precision_t(dst);

    T result = T(result_high_precision & high_precision_t(traits<T>::max));

    bool negative = is_negative(result);
    bool overflow = has_overflow<T>(src, dst, result);
    bool carry = has_carry(result_high_precision);

    state.set_status_register<bit::extend>(carry);
    state.set_status_register<bit::negative>(negative);
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(overflow);
    state.set_status_register<bit::carry>(carry);

    switch (direction)
    {
    case 0: state.write(dst_ptr, result); break; // Write to 'dst'
    case 1: state.write(src_ptr, result); break; // Write to 'src'
    default:
        THROW("Invalid direction");
    }
}

void inst_add(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_add_helper);
}

//
// Helper: AND, EOR, OR
//

struct operation_or { template <typename T> static T execute(T a, T b) { return a | b; } };
struct operation_and { template <typename T> static T execute(T a, T b) { return a & b; } };
struct operation_eor { template <typename T> static T execute(T a, T b) { return a ^ b; } };

template <typename T, typename O>
inline void inst_logical_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto direction = extract_bits<7, 1>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    T* dst_ptr = state.get_pointer<T>(0 /* Always "data register direct" mode */, dst_reg);

    T src = state.read(src_ptr);
    T dst = state.read(dst_ptr);

    T result = O::template execute(src, dst);

    state.set_status_register<bit::negative>(most_significant_bit(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    switch (direction)
    {
    case 0: state.write(dst_ptr, result); break; // Write to 'dst'
    case 1: state.write(src_ptr, result); break; // Write to 'src'
    default:
        THROW("Invalid direction");
    }
}

//
// AND
// Logical AND
//

void inst_and(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_helper, operation_and);
}

//
// EOR
// Logical exclusive OR
//

void inst_eor(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_helper, operation_eor);
}

//
// OR
// Logical OR
//

void inst_or(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_helper, operation_or);
}

//
// Helper: ORI, ANDI, EORI
//

template <typename T, typename O>
void inst_logical_imm_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);
    T* dst_ptr = state.get_pointer<T, false /* Choose status register over immediate */>(mode, reg);

    T dst = state.read(dst_ptr);

    typedef traits<T>::extension_word_type_t extension_t;
    auto imm = state.next<extension_t>();

    T result = O::template execute<T>(dst, T(imm));

    state.set_status_register<bit::negative>(most_significant_bit(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    state.write(dst_ptr, result);
}

//
// ORI
// Logical OR immediate
//

void inst_ori(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_imm_helper, operation_or);
}

//
// ANDI
// Logcal AND immediate
//

void inst_andi(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_imm_helper, operation_and);
}

//
// EORI
// Logcal exclusive OR immediate
//

void inst_eori(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_imm_helper, operation_eor);
}

//
// Helper: SUBI, ADDI
//

struct operation_sub { template <typename T> static T execute(T a, T b) { return a - b; } };
struct operation_add { template <typename T> static T execute(T a, T b) { return a + b; } };

template <typename T, typename O>
void inst_arithmetic_imm_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    T* dst_ptr = state.get_pointer<T>(mode, reg);
    T dst = state.read(dst_ptr);

    typedef traits<T>::extension_word_type_t extension_t;
    auto imm = state.next<extension_t>();

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t result_high_precision = 
        O::template execute<high_precision_t>(
            high_precision_t(dst),
            high_precision_t(imm));

    T result = T(result_high_precision & high_precision_t(traits<T>::max));

    bool carry = has_carry(result_high_precision);
    bool overflow = has_overflow<T>(dst, T(imm), result);

    state.set_status_register<bit::extend>(carry);
    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(overflow);
    state.set_status_register<bit::carry>(carry);

    state.write(dst_ptr, result);
}

//
// SUBI
// Subtract immediate
//

void inst_subi(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_arithmetic_imm_helper, operation_sub);
}

//
// ADDI
// Add immadiate
//

void inst_addi(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_arithmetic_imm_helper, operation_add);
}

//
// CMPI
// Compare immediate
//

template <typename T>
void inst_cmpi_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);
    T* ptr = state.get_pointer<T>(mode, reg);
    T value = state.read(ptr);

    typedef traits<T>::extension_word_type_t extension_t;
    extension_t imm = state.next<extension_t>();

    typedef traits<T>::higher_precision_type_t high_precision_t;
    high_precision_t result_high_precision = 
        high_precision_t(value) -
        high_precision_t(imm);
   
    T result = (T)(result_high_precision & high_precision_t(traits<T>::max));

    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(has_overflow(value, T(imm), result));
    state.set_status_register<bit::carry>(has_carry(result_high_precision));
}

void inst_cmpi(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_cmpi_helper);
}

//
// Helper: BTST, BCHG, BCLR, BSET
//

struct operation_btst { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val; } };
struct operation_bchg { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val ^ (1 << bit_index); } };
struct operation_bclr { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val & (~(1 << bit_index)); } };
struct operation_bset { static uint32_t execute(uint32_t val, uint32_t bit_index) { return val | (1 << bit_index); } };

template <typename O>
inline void inst_bitop_helper(machine_state& state, uint16_t opcode)
{
    auto src_reg = extract_bits<4, 3>(opcode);
    auto bit_index_src = extract_bits<7, 1>(opcode);
    auto dst_mode = extract_bits<10, 3>(opcode);
    auto dst_reg = extract_bits<13, 3>(opcode);

    uint32_t bit_index = 0;
    if (bit_index_src == 0)
    {
        // Immediate
        IF_FALSE_THROW(src_reg == 0x4, "Source register value must be 0x4 in immediate mode");
        bit_index = uint32_t(state.next<uint16_t>());
    }
    else
    {
        // Data register
        uint32_t* ptr = state.get_pointer<uint32_t>(0 /* Data register direct */, src_reg);
        bit_index = state.read(ptr);
    }
    
    switch (dst_mode)
    {
    case 0: bit_index &= 0x1f; break; // Modulo 32 for data registers
    default: bit_index &= 0x7; break; // Modulo 8, otherwise (aka. memory locations)
    }

    uint32_t* dst_ptr = state.get_pointer<uint32_t>(dst_mode, dst_reg);
    uint32_t dst = state.read(dst_ptr);

    auto result = O::template execute(dst, bit_index);

    const uint32_t mask = (1 << bit_index);
    state.set_status_register<bit::zero>((result & mask) == 0);

    state.write(dst_ptr, result);
}

//
// BTST
// Test bit
//

void inst_btst(machine_state& state, uint16_t opcode)
{
    inst_bitop_helper<operation_btst>(state, opcode);
}

//
// BSET
// Test and set bit
//

void inst_bset(machine_state& state, uint16_t opcode)
{
    inst_bitop_helper<operation_bset>(state, opcode);
}

//
// BCLR
// Test and clear bit
//

void inst_bclr(machine_state& state, uint16_t opcode)
{
    inst_bitop_helper<operation_bclr>(state, opcode);
}

//
// BCHG
// Test and change (flip) bit
//

void inst_bchg(machine_state& state, uint16_t opcode)
{
    inst_bitop_helper<operation_bchg>(state, opcode);
}

//
// JMP
// Unconditional jump
//

void inst_jmp(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    uint32_t* jump_to_ptr = state.get_pointer<uint32_t>(mode, reg);
    uint32_t jump_to = state.read(jump_to_ptr);

    state.set_program_counter(jump_to);
}

//
// JSR
// Jump to subroutine
//

void inst_jsr(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    uint32_t* jump_to_ptr = state.get_pointer<uint32_t>(mode, reg);
    uint32_t jump_to = state.read(jump_to_ptr);
    
    state.push_program_counter();
    state.set_program_counter(jump_to);
}

//
// RTS
// Return from subroutine
//

void inst_rts(machine_state& state, uint16_t opcode)
{
    state.pop_program_counter();
}

//
// Helper: ADDQ, SUBQ
//

template <typename T, typename O>
inline void inst_arithmetic_quick_helper(machine_state& state, uint16_t opcode)
{
    auto data = extract_bits<4, 3>(opcode);
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    T* ptr = state.get_pointer<T>(mode, reg);
    T value = state.read(ptr);

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t result_high_precision = O::template execute(
        high_precision_t(value),
        high_precision_t(data));

    T result = T(result_high_precision & high_precision_t(traits<T>::max));

    bool carry = has_carry(result_high_precision);
    bool overflow = has_overflow(value, T(data), result);
    bool negative = is_negative(result);

    state.set_status_register<bit::extend>(carry);
    state.set_status_register<bit::negative>(negative);
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(overflow);
    state.set_status_register<bit::carry>(carry);

    state.write(ptr, result);
}

//
// ADDQ
// Add quick, add small immediate value
//

void inst_addq(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_arithmetic_quick_helper, operation_add);
}

//
// SUBQ
// Subtract quick, subtract small immediate value
//

void inst_subq(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_arithmetic_quick_helper, operation_sub);
}

//
// RTE
// Return from exception handler
//

void inst_rte(machine_state& state, uint16_t opcode)
{
    if (!state.get_status_register<bit::supervisor>())
    {
        state.exception(8 /* Privilege violation */);
    }
    state.pop_status_register();
    state.pop_program_counter();
}

//
// TRAP
// Trap, aka. software interrupt
//

void inst_trap(machine_state& state, uint16_t opcode)
{
    auto vector = extract_bits<12, 4>(opcode);
    state.exception(32 + vector); // Note: Software traps are using entry 32 - 47 in the vector table
}

//
// TRAPV
// Trap on overflow
//

void inst_trapv(machine_state& state, uint16_t opcode)
{
    if (state.get_status_register<bit::overflow>())
    {
        state.exception(7 /* TRAPV */);
    }
}

//
// LEA
// Load effective address
//

void inst_lea(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    uint32_t* src_ptr = state.get_pointer<uint32_t>(src_mode, src_reg);
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(1 /* Address register direct */, dst_reg);

    uint32_t offset = state.pointer_to_memory_offset(src_ptr);
    state.write(dst_ptr, offset);
}

//
// PEA
// Push effective address onto stack
//

void inst_pea(machine_state& state, uint16_t opcode)
{
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    uint32_t* src_ptr = state.get_pointer<uint32_t>(src_mode, src_reg);

    uint32_t offset = state.pointer_to_memory_offset(src_ptr);
    state.push(offset);
}

//
// CHK
// Trap if value is out of specified bounds
//

void inst_chk(machine_state& state, uint16_t opcode)
{
    auto value_reg = extract_bits<4, 3>(opcode);
    int16_t* value_ptr = (int16_t*)state.get_pointer<uint16_t>(0 /* Data register direct */, value_reg);
    int16_t value = state.read(value_ptr);
 
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);
    auto src_ptr = state.get_pointer<uint16_t>(src_mode, src_reg);
    auto upper_bound = state.read(src_ptr);
    
    if (value < 0 || value > upper_bound)
    {
        state.exception(6 /* CHK out of bounds */ );
    }
}

//
// Helper: MOVEM
//

template <typename T, const int direction>
inline void inst_movem_helper(machine_state& state, uint16_t opcode)
{
    auto register_select = state.next<uint16_t>(); // Note: Get this first so we don't interfere with addressing modes, etc.

    auto mem_mode = extract_bits<10, 3>(opcode);
    auto mem_reg = extract_bits<13, 3>(opcode);

    int32_t delta = 1;
    int32_t begin = 0;

    if (direction == 0 /* Register to memory */ && mem_mode == 4 /* Address register indirect with predecrement */)
    {
        // Note: With pre-decrement addressing mode the registers are stored in inverse order (A7-A0, D7-D0)
        delta = -1;
        begin = 15;
    }

    for (int32_t counter = 0, i = begin; counter < 16; counter++, i += delta)
    {
        if (((1 << i) & register_select) != 0)
        {
            uint32_t reg_mode = (i >> 3) & 0x1;
            uint32_t reg_reg = i & 0x7;
            T* reg = state.get_pointer<T>(reg_mode, reg_reg);
            T* mem = state.get_pointer<T>(mem_mode, mem_reg);

            switch (direction)
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
// MOVEM
// Move multiple registers
//

void inst_movem(machine_state& state, uint16_t opcode)
{
    auto direction = extract_bits<5, 1>(opcode);
    auto size = extract_bits<9, 1>(opcode);
    switch (size)
    {
    case 0: // Word
        switch (direction)
        {
        case 0: inst_movem_helper<uint16_t, 0>(state, opcode); break; // Word, register to memory
        case 1: inst_movem_helper<uint16_t, 1>(state, opcode); break; // Word, memory to register
        default: THROW("Invalid direction");
        }
        break;
    case 1: // Long
        switch (direction)
        {
        case 0: inst_movem_helper<uint32_t, 0>(state, opcode); break; // Long, register to memory
        case 1: inst_movem_helper<uint32_t, 1>(state, opcode); break; // Long, memory to register
        default: THROW("Invalid direction");
        }
        break;
    default:
        THROW("Invalid size");
    }
}

//
// Helper: Scc, Bcc
//
inline bool evaluate_condition(machine_state& state, uint32_t condition)
{
    bool set = false;

    bool c = state.get_status_register<bit::carry>();
    bool z = state.get_status_register<bit::zero>();
    bool n = state.get_status_register<bit::negative>();
    bool v = state.get_status_register<bit::overflow>();

    switch (condition)
    {
    case 0x0: set = true; break;                                // True (T)
    case 0x1: set = false; break;                               // False (F)
    case 0x2: set = !c && !z; break;                            // High (HI)
    case 0x3: set = c || z; break;                              // Low or same (LS)
    case 0x4: set = !c; break;                                  // Carry clear (CC)
    case 0x5: set = c; break;                                   // Carry set (CS)
    case 0x6: set = !z; break;                                  // Not equal (NE)
    case 0x7: set = z; break;                                   // Equal (EQ)
    case 0x8: set = !v; break;                                  // Overflow clear (VC)
    case 0x9: set = v; break;                                   // Overflow set (VS)
    case 0xa: set = !n; break;                                  // Plus (PL)
    case 0xb: set = n; break;                                   // Minus (MI)
    case 0xc: set = (n && v) || (!n && !v); break;              // Greater or equal (GE)
    case 0xd: set = (n && !v) || (!n && v); break;              // Less than (LT)
    case 0xe: set = (n && v && !z) || (!n && !v && !z); break;  // Greater than (GT)
    case 0xf: set = z || (n && !v) || (!n && v); break;         // Less or equal (LE)
    default:
        THROW("Invalid condition: " << condition);
    }

    return set;
}


//
// Scc
// Set on condition
//

void inst_scc(machine_state& state, uint16_t opcode)
{
    auto condition = extract_bits<4, 4>(opcode);
    bool result = evaluate_condition(state, condition);
    
    auto dst_mode = extract_bits<10, 3>(opcode);
    auto dst_reg = extract_bits<13, 3>(opcode);
    auto dst_ptr = state.get_pointer<uint8_t>(dst_mode, dst_reg);
    
    state.write<uint8_t>(dst_ptr, uint8_t(result ? 0xff : 0x00));
}

//
// Bcc
// Branch on condition
//

void inst_bcc(machine_state& state, uint16_t opcode)
{
    auto condition = extract_bits<4, 4>(opcode);
    bool result = evaluate_condition(state, condition);

    if (result)
    {
        auto unused = extract_bits<8, 8>(opcode);
        
        auto displacement = state.next<uint16_t>();
        
        // TODO

        //state.set_program_counter()
    }
}

//
// Helper: EXT
//

template <typename T>
inline void inst_ext_helper(machine_state& state, uint16_t opcode)
{
    auto reg = extract_bits<13, 3>(opcode);

    typedef traits<T>::lower_precision_type_t low_precision_t;
    low_precision_t* ptr = state.get_pointer<low_precision_t>(0, reg);
    low_precision_t value = state.read<low_precision_t>(ptr);

    T result = (T)(sign_extend<low_precision_t>(value) & traits<T>::max);

    state.set_status_register<bit::negative>(is_negative<T>(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    state.write<T>((T*)ptr, result);
}

//
// EXT
// Sign extend a data register
//

void inst_ext(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<9, 1>(opcode);
    switch (size)
    {
    case 0: inst_ext_helper<uint16_t>(state, opcode); break;
    case 1: inst_ext_helper<uint32_t>(state, opcode); break;
    default:
        THROW("Invalid size");
    }
}

//
// SWAP
// Swap register halves
//
void inst_swap(machine_state& state, uint16_t opcode)
{
    auto reg = extract_bits<13, 3>(opcode);

    uint32_t* ptr = state.get_pointer<uint32_t>(0, reg);
    uint32_t value = state.read<uint32_t>(ptr);
    uint32_t result = ((value >> 16) & 0xffff) | ((value << 16) & 0xffff0000);

    state.write<uint32_t>(ptr, result);
}