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

    T result = state.read(src_ptr);

    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);
    
    state.write(dst_ptr, result);
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
    else
    {
        state.pop_status_register();
        state.pop_program_counter();
    }
}

//
// RTR
// Return and restore condition codes
//

void inst_rtr(machine_state& state, uint16_t opcode)
{
    // Pop the status register (SR) off the stack, but only set the condition code register (CCR)
    auto status_reg = state.pop<uint16_t>();
    state.set_condition_code_register(uint8_t(status_reg & 0xff));

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
        const uint32_t mask = (1 << i);

        if ((mask & register_select) != 0)
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
        int32_t displacement = int8_t(extract_bits<8, 8>(opcode));
        if (displacement == 0)
        {
            displacement = int32_t(state.next<int16_t>());
        }
        state.offset_program_counter(displacement);
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

    state.set_status_register<bit::negative>(most_significant_bit(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    state.write<uint32_t>(ptr, result);
}

//
// ILLEGAL
// Illegal instruction
//

void inst_illegal(machine_state& state, uint16_t opcode)
{
    state.exception(4 /* Illegal instruction */);
}

//
// Helper: NOT
//

template <typename T>
inline void inst_not_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    auto ptr = state.get_pointer<T>(mode, reg);
    T value = state.read(ptr);
    T result = ~value;

    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    state.write<T>(ptr, result);
}

//
// NOT
// Logical not
//

void inst_not(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_not_helper);
}

//
// LINK
// Link and allocate 
//

void inst_link(machine_state& state, uint16_t opcode)
{
    // The contents of the specified address register is pushed onto the stack
    auto reg = extract_bits<13, 3>(opcode);
    auto ptr = state.get_pointer<uint32_t>(1, reg);
    auto val = state.read<uint32_t>(ptr);
    state.push<uint32_t>(val);

    // Then, the address register is loaded with the updated stack pointer
    auto stack_ptr_ptr = state.get_pointer<uint32_t>(1, 7);
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

void inst_unlk(machine_state& state, uint16_t opcode)
{
    // The stack pointer is loaded from the specified address register...
    auto reg = extract_bits<13, 3>(opcode);
    auto ptr = state.get_pointer<uint32_t>(1, reg);
    auto new_stack_ptr = state.read<uint32_t>(ptr);
    
    // ... and the old contents of the stack pointer is lost
    auto stack_ptr_ptr = state.get_pointer<uint32_t>(1, 7);
    state.write<uint32_t>(stack_ptr_ptr, new_stack_ptr);

    // The address register is then loaded with the longword pulled off the stack.
    auto val = state.pop<uint32_t>();
    state.write<uint32_t>(ptr, val);
}

//
// TAS
// Test and set operand
//

void inst_tas(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    auto ptr = state.get_pointer<uint8_t>(mode, reg);
    auto val = state.read<uint8_t>(ptr);

    state.set_status_register<bit::negative>(most_significant_bit(val));
    state.set_status_register<bit::zero>(val == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);

    val |= (1 << 7);

    state.write<uint8_t>(ptr, val);
}

//
// Helper: TST
//

template <typename T>
inline void inst_tst_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);
    
    auto ptr = state.get_pointer<T>(mode, reg);
    auto val = state.read<T>(ptr);

    state.set_status_register<bit::negative>(is_negative(val));
    state.set_status_register<bit::zero>(val == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(false);
}

//
// TST
// Test an operand
//

void inst_tst(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_tst_helper);
}

//
// RESET
// Reset external devices
//

void inst_reset(machine_state& state, uint16_t opcode)
{
    state.reset();
}

//
// NOP
// No operation
//

void inst_nop(machine_state& state, uint16_t opcode)
{
}

//
// Helper: MOVEP
//

template <typename T>
inline void inst_movep_helper(machine_state& state, uint16_t opcode)
{
    auto reg_reg = extract_bits<4, 3>(opcode);
    auto mem_reg = extract_bits<13, 3>(opcode);

    auto reg_ptr = state.get_pointer<uint8_t>(0, reg_reg) + sizeof(T) - 1; // Note: Endian
    auto mem_ptr = state.get_pointer<uint8_t>(2, mem_reg) + state.next<int16_t>();
    
    auto direction = extract_bits<8, 1>(opcode);
    for (uint32_t i = 0; i < sizeof(T); i++)
    {
        switch (direction)
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
// MOVEP
// Move peripheral data
//

void inst_movep(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<9, 1>(opcode);
    switch (size)
    {
    case 0: inst_movep_helper<uint16_t>(state, opcode); break;
    case 1: inst_movep_helper<uint32_t>(state, opcode); break;
    default:
        THROW("Invalid size");
    }
}

//
// Helper: NEGX, NEG
//

template <typename T, bool use_extend>
inline void inst_negate_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    auto ptr = state.get_pointer<T>(mode, reg);
    auto val = state.read<T>(ptr);

    T extend = (use_extend && state.get_status_register<bit::extend>()) ? T(1) : T(0);

    val = negate(val);
    extend = negate(extend);

    typedef traits<T>::higher_precision_type_t high_precition_t;

    high_precition_t result_high_precision = high_precition_t(val) + high_precition_t(extend);
    T result = (T)(result_high_precision & traits<T>::max);

    bool negative = is_negative(result);
    bool overflow = has_overflow(val, extend, result);

    state.set_status_register<bit::extend>(result != 0);
    state.set_status_register<bit::negative>(negative);
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(overflow);
    state.set_status_register<bit::carry>(result != 0);

    state.write<T>(ptr, result);
}

//
// NEGX
// Negate with extend
//

void inst_negx(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_negate_helper, true);
}

//
// NEG
// Negate
//

void inst_neg(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_negate_helper, false);
}

//
// Helper: DIVU, DIVS
//

template <typename TDenom, typename TNum, const TNum min_val, const TNum max_val>
inline void inst_divide_helper(machine_state& state, uint16_t opcode)
{
    // Denominator (16 bits)
    auto denom_mode = extract_bits<10, 3>(opcode);
    auto denom_reg = extract_bits<13, 3>(opcode);
    auto denom_ptr = state.get_pointer<uint16_t>(denom_mode, denom_reg);
    auto denom_val = state.read<TDenom>((TDenom*)denom_ptr);

    // Always clear carry flag
    state.set_status_register<bit::carry>(false);

    if (denom_val == 0)
    {
        state.exception(5 /* Divide by zero */);
    }
    else
    {
        // Numerator (32 bits)
        auto num_reg = extract_bits<4, 3>(opcode);
        auto num_ptr = state.get_pointer<uint32_t>(0, num_reg);
        auto num_val = state.read<TNum>((TNum*)num_ptr);

        // Divide
        auto quotient = num_val / TNum(denom_val);

        // Does the result overflow
        bool overflow = (quotient > max_val) || (quotient < min_val);
        state.set_status_register<bit::overflow>(overflow);

        if (!overflow)
        {
            // Generate result (quotient + remainder)
            uint32_t quotient_unsigned = uint32_t(quotient);
            uint32_t remainder = uint32_t(num_val % TNum(denom_val));
            uint32_t result = ((remainder << 16) & 0xffff0000) | (quotient_unsigned & 0xffff);

            // Update status bits
            state.set_status_register<bit::negative>(is_negative(quotient_unsigned));
            state.set_status_register<bit::zero>(quotient == 0);

            // Write result
            state.write<uint32_t>((uint32_t*)num_ptr, result);
        }
    }
}

//
// DIVU
// Divide unsigned
//

void inst_divu(machine_state& state, uint16_t opcode)
{
    inst_divide_helper<uint16_t, uint32_t, 0, 65535>(state, opcode);
}

//
// DIVS
// Divide signed
//

void inst_divs(machine_state& state, uint16_t opcode)
{
    inst_divide_helper<int16_t, int32_t, -32768, 32767>(state, opcode);
}

//
// EXG
// Exchange registers
//

void inst_exg(machine_state& state, uint16_t opcode)
{
    uint32_t mode1;
    uint32_t mode2;

    auto operation = extract_bits<8, 5>(opcode);
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

    auto ptr1 = state.get_pointer<uint32_t>(mode1, reg1);
    auto ptr2 = state.get_pointer<uint32_t>(mode2, reg2);

    auto val1 = state.read<uint32_t>(ptr1);
    auto val2 = state.read<uint32_t>(ptr2);

    state.write<uint32_t>(ptr1, val2);
    state.write<uint32_t>(ptr2, val1);
}

//
// Helper: LSx, register
//

struct operation_shift_left
{
    template <typename T>
    static T execute(T value, uint32_t shift_amount, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_left(value, shift_amount);
        return value << shift_amount;
    }
};

struct operation_shift_right
{
    template <typename T>
    static T execute(T value, uint32_t shift_amount, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_right(value, shift_amount);
        return value >> shift_amount;
    }
};

template <typename T, typename O>
inline void inst_lsx_reg_helper(machine_state& state, uint16_t opcode)
{
    uint32_t shift_amount = 0;

    auto rotation = extract_bits<4, 3>(opcode);
    auto mode = extract_bits<10, 1>(opcode);

    switch (mode)
    {
    case 0: // Immediate
        shift_amount = rotation;
        break;
    case 1: // Data register
        shift_amount = state.read<uint32_t>(state.get_pointer<uint32_t>(0, rotation));
        break;
    default:
        THROW("Invalid mode");
    }

    // Modulo 64
    shift_amount &= 0x3f;

    auto reg = extract_bits<13, 3>(opcode);
    auto ptr = state.get_pointer<T>(0, reg);
    auto val = state.read<T>(ptr);

    bool last_shifted_out = false;
    T result = O::template execute<T>(val, shift_amount, last_shifted_out);

    if (shift_amount > 0)
    {
        state.set_status_register<bit::extend>(last_shifted_out);
        state.set_status_register<bit::carry>(last_shifted_out);
    }
    else
    {
        // The X (extend) bit is unaffected if the shift amount is zero, but C (carry) is cleared
        state.set_status_register<bit::carry>(false);
    }
    
    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);

    state.write<T>(ptr, result);
}

//
// LSL, register
// Logical left shift
//

void inst_lsl_reg(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_lsx_reg_helper, operation_shift_left);
}

//
// LSR, register
// Logical right shift
//

void inst_lsr_reg(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_lsx_reg_helper, operation_shift_right);
}

//
// Helper: LSx, Memory
//

template <typename O>
inline void inst_lsx_mem_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    auto ptr = state.get_pointer<uint16_t>(mode, reg);
    uint16_t val = state.read<uint16_t>(ptr);

    bool last_shifted_out = false;
    uint16_t result = O::template execute<uint16_t>(val, 1, last_shifted_out);

    state.set_status_register<bit::extend>(last_shifted_out);
    state.set_status_register<bit::negative>(is_negative(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(last_shifted_out);

    state.write<uint16_t>(ptr, result);
}

//
// LSL, memory
// Logical shift left
//

void inst_lsl_mem(machine_state& state, uint16_t opcode)
{
    inst_lsx_mem_helper<operation_shift_left>(state, opcode);
}

//
// LSR, memory
// Logical shift right
//

void inst_lsr_mem(machine_state& state, uint16_t opcode)
{
    inst_lsx_mem_helper<operation_shift_right>(state, opcode);
}

//
// BRA
// Branch always
//

void inst_bra(machine_state& state, uint16_t opcode)
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

void inst_bsr(machine_state& state, uint16_t opcode)
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
// DBcc
// Test condition, decrement and branch
//

void inst_dbcc(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// Helper: ROx, memory
//

struct operation_rotate_left
{
    template <typename T>
    static T execute(T value, uint32_t shift_amount, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_left(value, shift_amount);
        return rotate_left<T>(value, shift_amount);
    }
};

struct operation_rotate_right
{
    template <typename T>
    static T execute(T value, uint32_t shift_amount, bool& last_shifted_out)
    {
        last_shifted_out = last_shifted_out_right(value, shift_amount);
        return rotate_right<T>(value, shift_amount);
    }
};

template <typename O>
inline void inst_rox_mem_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    auto ptr = state.get_pointer<uint16_t>(mode, reg);
    auto val = state.read<uint16_t>(ptr);

    bool last_shifted_out = false;
    auto result = O::template execute(val, 1, last_shifted_out);

    state.set_status_register<bit::negative>(most_significant_bit(result));
    state.set_status_register<bit::zero>(result == 0);
    state.set_status_register<bit::overflow>(false);
    state.set_status_register<bit::carry>(last_shifted_out);

    state.write<uint16_t>(ptr, result);
}

//
// ROL, memory
// Rotate left
//

void inst_rol_mem(machine_state& state, uint16_t opcode)
{
    inst_rox_mem_helper<operation_rotate_left>(state, opcode);
}

//
// ROR, memory
// Rotate right
//

void inst_ror_mem(machine_state& state, uint16_t opcode)
{
    inst_rox_mem_helper<operation_rotate_right>(state, opcode);
}

//
// ROL, register
// Rotate left
//

void inst_rol_reg(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ROR, register
// Rotate right
//

void inst_ror_reg(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ROXL, memory
// Rotate left with extend
//

void inst_roxl_mem(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ROXR, memory
// Rotate right with extend
// 

void inst_roxr_mem(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ROXL, register
// Rotate left with extend
//

void inst_roxl_reg(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ROXR, register
// Rotate right with extend
//

void inst_roxr_reg(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ASL, memory
// Arithmetic shift left
//

void inst_asl_mem(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ASR, memory
// Arithmetic shift right
//

void inst_asr_mem(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ASL, register
// Arithmetic shift left
//

void inst_asl_reg(machine_state& state, uint16_t opcode)
{
    // TODO
}

//
// ASR, register
// Arithmetic shift right
//

void inst_asr_reg(machine_state& state, uint16_t opcode)
{
    // TODO
}
