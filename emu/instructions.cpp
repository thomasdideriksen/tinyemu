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

    *dst_ptr = *src_ptr;

    typedef traits<T>::signed_type_t signed_t;

    state.set_status_register<sr::negative>(*((signed_t*)src_ptr) < 0);
    state.set_status_register<sr::zero>(*src_ptr == 0);
    state.set_status_register<sr::overflow>(false);
    state.set_status_register<sr::carry>(false);
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
//

void inst_moveq(machine_state& state, uint16_t opcode)
{
    const auto dst_reg = extract_bits<4, 3>(opcode);
    const uint8_t data = uint8_t(extract_bits<8, 8>(opcode));
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(0, dst_reg);

    bool is_negative;
    auto result = sign_extend(data, &is_negative);
    
    *dst_ptr = result;

    state.set_status_register<sr::negative>(is_negative);
    state.set_status_register<sr::zero>(result == 0);
    state.set_status_register<sr::overflow>(false);
    state.set_status_register<sr::carry>(false);
}

//
// CLR
//

template <typename T>
void inst_clr_helper(machine_state& state, uint16_t opcode)
{
    auto mode = extract_bits<10, 3>(opcode);
    auto reg = extract_bits<13, 3>(opcode);

    T* ptr = state.get_pointer<T>(mode, reg);
    *ptr = 0x0;

    state.set_status_register<sr::negative>(false);
    state.set_status_register<sr::zero>(true);
    state.set_status_register<sr::overflow>(false);
    state.set_status_register<sr::carry>(false);
}

void inst_clr(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_clr_helper);
}

//
// MOVEA
//

template <typename T>
inline void inst_movea_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    uint32_t* dst_ptr = state.get_pointer<uint32_t>(1 /* Always "address register direct" mode */, dst_reg);

    *dst_ptr = sign_extend(*src_ptr);
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

    typedef traits<T>::higher_precision_type_t high_precision_t;

    high_precision_t result_high_precision =
        high_precision_t(*src_ptr) +
        high_precision_t(*dst_ptr);

    T result = T(result_high_precision & high_precision_t(traits<T>::max));

    switch (direction)
    {
    case 0: *dst_ptr = T(result); break; // Write to 'dst'
    case 1: *src_ptr = T(result); break; // Write to 'src'
    default:
        THROW("Invalid direction");
    }

    bool carry = ((result_high_precision >> traits<T>::bits) & 0x1) != 0;
    bool negative = is_negative(result);

    state.set_status_register<sr::extend>(carry);
    state.set_status_register<sr::negative>(negative);
    state.set_status_register<sr::zero>(result == 0);
    //state.set_ccr_bit<ccr_bit::overflow>(); <--- TODO
    state.set_status_register<sr::carry>(carry);
}

void inst_add(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_add_helper);
}

//
// AND
//

template <typename T>
inline void inst_and_helper(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto direction = extract_bits<7, 1>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    T* src_ptr = state.get_pointer<T>(src_mode, src_reg);
    T* dst_ptr = state.get_pointer<T>(0 /* Always "data register direct" mode */, dst_reg);

    T result = (*src_ptr) & (*dst_ptr);

    switch (direction)
    {
    case 0: *dst_ptr = T(result); break; // Write to 'dst'
    case 1: *src_ptr = T(result); break; // Write to 'src'
    default:
        THROW("Invalid direction");
    }

    //bool carry = ((result_high_precision >> traits<T>::bits) & 0x1) != 0;
    //bool negative = is_negative(result);

    //state.set_ccr_bit<ccr_bit::extend>(carry);
    //state.set_ccr_bit<ccr_bit::negative>(negative);
    //state.set_ccr_bit<ccr_bit::zero>(result == 0);
    ////state.set_ccr_bit<ccr_bit::overflow>(); <--- TODO
    //state.set_ccr_bit<ccr_bit::carry>(carry);
}

void inst_and(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE(size, inst_and_helper);
}

//
// EOR
//

template <typename T>
void inst_eor_helper(machine_state& state, uint16_t opcode)
{

}

void inst_eor(machine_state& state, uint16_t opcode)
{
    auto dst_reg = extract_bits<4, 3>(opcode);
    auto direction = extract_bits<7, 1>(opcode);
    auto src_mode = extract_bits<10, 3>(opcode);
    auto src_reg = extract_bits<13, 3>(opcode);

    THROW("Not implemented");
}

//
// ORI, ANDI, EORI
//

struct operation_or { template <typename T> static void execute(T* src_dst, T operand) { *src_dst |= operand; } };
struct operation_and { template <typename T> static void execute(T* src_dst, T operand) { *src_dst &= operand; } };
struct operation_eor { template <typename T> static void execute(T* src_dst, T operand) { *src_dst ^= operand; } };

template <typename T, typename O>
void inst_logical_imm_helper(machine_state& state, uint16_t opcode)
{
    T* dst_ptr = nullptr;

    switch (opcode)
    {
    case 0x3c: // CCR
    case 0x7c: // SR
        dst_ptr = (T*)state.get_status_register_pointer();
        break;

    default:
        auto mode = extract_bits<10, 3>(opcode);
        auto reg = extract_bits<13, 3>(opcode);
        dst_ptr = state.get_pointer<T>(mode, reg);
        break;
    }

    typedef traits<T>::extension_word_type_t extension_t;
    auto imm = state.next<extension_t>();

    O::template execute<T>(dst_ptr, T(imm));

    state.set_status_register<sr::negative>(most_significant_bit(*dst_ptr));
    state.set_status_register<sr::zero>(*dst_ptr == 0);
    state.set_status_register<sr::overflow>(false);
    state.set_status_register<sr::carry>(false);
}

//
// ORI
//

void inst_ori(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_imm_helper, operation_or);
}

//
// ANDI
//

void inst_andi(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_imm_helper, operation_and);
}

//
// EORI
//

void inst_eori(machine_state& state, uint16_t opcode)
{
    auto size = extract_bits<8, 2>(opcode);
    PROCESS_SIZE_WITH_TEMPLATE_PARAM(size, inst_logical_imm_helper, operation_eor);
}

