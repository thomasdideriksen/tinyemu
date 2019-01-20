#pragma once
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <intrin.h>

#define THROW(msg) { std::stringstream __stream; __stream << __FILE__ << ":" << __LINE__ << ": " << msg; throw std::runtime_error(__stream.str()); }
#define IF_FALSE_THROW(expr, msg) { if (!(expr)) { THROW(msg); } }
#define countof(arr) (sizeof(arr) / sizeof(arr[0]))

#define INLINE __forceinline

template <typename T>
struct traits {};

template <>
struct traits<uint8_t>
{
    typedef int8_t signed_type_t;
    typedef uint16_t higher_precision_type_t;
    typedef uint16_t extension_word_type_t;
    static const uint8_t max = 0xff;
    static const uint32_t bits = 8;
};

template <>
struct traits<uint16_t>
{
    typedef int16_t signed_type_t;
    typedef uint8_t lower_precision_type_t;
    typedef uint32_t higher_precision_type_t;
    typedef uint16_t extension_word_type_t;
    static const uint16_t max = 0xffff;
    static const uint32_t bits = 16;
};

template <>
struct traits<uint32_t>
{
    typedef int32_t signed_type_t;
    typedef uint16_t lower_precision_type_t;
    typedef uint64_t higher_precision_type_t;
    typedef uint32_t extension_word_type_t;
    static const uint32_t max = 0xffffffff;
    static const uint32_t bits = 32;
};


template <uint32_t bit_offset, uint32_t bit_count>
INLINE uint16_t extract_bits(uint16_t src)
{
    return (src >> (16 - bit_offset - bit_count)) & (0xffff >> (16 - bit_count));
}

template <uint32_t mode, uint32_t reg>
INLINE constexpr uint32_t make_effective_address()
{
    return reg | (mode << 3);
}

INLINE uint32_t make_effective_address(uint32_t mode, uint32_t reg)
{
    return reg | (mode << 3);
}

template <typename T>
INLINE uint32_t sign_extend(const T& value)
{ 
    typedef traits<T>::signed_type_t signed_t;
    signed_t* ptr = (signed_t*)&value;
    return uint32_t(*ptr);
}

template <typename T>
INLINE bool is_negative(T value)
{
    typedef traits<T>::signed_type_t signed_t;
    signed_t* ptr = (signed_t*)&value;
    return (*ptr) < 0;
}

template <typename T>
INLINE bool most_significant_bit(T value)
{
    typedef std::make_unsigned<T>::type unsigned_t;
    return (unsigned_t(value) >> ((sizeof(T) * 8) - 1)) != 0;
}

template <typename T>
INLINE bool has_carry(T higher_precision_result)
{
    return ((higher_precision_result >> (sizeof(T) * 4)) & 0x1) != 0;
}

template <typename T>
INLINE bool has_overflow(T op0, T op1, T result)
{
    bool op0_negative = is_negative(op0);
    bool op1_negative = is_negative(op1);
    return (op0_negative == op1_negative) ? op0_negative != is_negative(result) : false;
}

template <typename T>
INLINE T negate(T value)
{
    return (~value) + 1;
}

template <typename T>
INLINE bool last_shifted_out_left(T value, uint32_t shift)
{
    typedef std::make_unsigned<T>::type unsigned_t;
    return ((unsigned_t(value) >> ((sizeof(T) * 8) - shift)) & 0x1) != 0;
}

template <typename T>
INLINE bool last_shifted_out_right(T value, uint32_t shift)
{
    typedef std::make_unsigned<T>::type unsigned_t;
    return ((unsigned_t(value) >> (shift - 1)) & 0x1) != 0;
}

// TODO: Move/abstract intrinsics invocations to platform specific file

template <typename T>
INLINE T swap(T value)
{
    switch (sizeof(T))
    {
    case 1: return T(value);
    case 2: return T(::_byteswap_ushort(value));
    case 4: return T(::_byteswap_ulong(value));
    default:
        THROW("Invalid type size");
    }
}

template <typename T>
INLINE T rotate_left(T value, uint8_t shift)
{
    switch (sizeof(T))
    {
    case 1: return T(::_rotl8(uint8_t(value), shift));
    case 2: return T(::_rotl16(uint16_t(value), shift));
    case 4: return T(::_rotl(uint32_t(value), shift));
    default:
        THROW("Invalid type size");
    }
}

template <typename T>
INLINE T rotate_right(T value, uint8_t shift)
{
    switch (sizeof(T))
    {
    case 1: return T(::_rotr8(uint8_t(value), shift));
    case 2: return T(::_rotr16(uint16_t(value), shift));
    case 4: return T(::_rotr(uint32_t(value), shift));
    default:
        THROW("Invalid type size");
    }
}

INLINE constexpr bool is_address_register(uint32_t effective_address)
{
    auto mode = (effective_address >> 3) & 0x7;
    return mode > 0 && mode < 7; // Modes 1, 2, 3, 4, 5, 6 are all affecting address registers
}