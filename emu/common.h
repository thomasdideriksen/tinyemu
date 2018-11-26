#pragma once
#include <sstream>
#include <stdexcept>

#define THROW(msg) { std::stringstream __stream; __stream << __FILE__ << ":" << __LINE__ << ": " << msg; throw std::runtime_error(__stream.str()); }
#define IF_FALSE_THROW(expr, msg) { if (!(expr)) { THROW(msg); } }

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
    typedef uint32_t higher_precision_type_t;
    typedef uint16_t extension_word_type_t;
    static const uint16_t max = 0xffff;
    static const uint32_t bits = 16;
};

template <>
struct traits<uint32_t>
{
    typedef int32_t signed_type_t;
    typedef uint64_t higher_precision_type_t;
    typedef uint32_t extension_word_type_t;
    static const uint32_t max = 0xffffffff;
    static const uint32_t bits = 32;
};

template <const int bit_offset, const int bit_count>
inline uint16_t extract_bits(uint16_t src)
{
    return (src >> (16 - bit_offset - bit_count)) & (0xffff >> (16 - bit_count));
}

template <typename T>
inline uint32_t sign_extend(const T& value, bool* is_negative = nullptr)
{
    typedef traits<T>::signed_type_t signed_t;
    signed_t* ptr = (signed_t*)&value;
    if (is_negative)
    {
        *is_negative = (*ptr) < 0;
    }
    return uint32_t(*ptr);
}

template <typename T>
inline bool is_negative(T value)
{
    typedef traits<T>::signed_type_t signed_t;
    signed_t* ptr = (signed_t*)&value;
    return (*ptr) < 0;
}