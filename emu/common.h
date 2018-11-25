#pragma once
#include <sstream>
#include <stdexcept>

#define THROW(msg) { std::stringstream __stream; __stream << __FILE__ << ":" << __LINE__ << ": " << msg; throw std::runtime_error(__stream.str()); }
#define IF_FALSE_THROW(expr, msg) { if (!(expr)) { THROW(msg); } }

template <const int bit_offset, const int bit_count>
inline uint16_t extract_bits(uint16_t src)
{
    return (src >> (16 - bit_offset - bit_count)) & (0xffff >> (16 - bit_count));
}

template <typename T>
inline uint32_t sign_extend(const T& value, bool* is_negative = nullptr)
{
    typedef std::make_signed<T>::type TSigned;
    TSigned* ptr = (TSigned*)&value;
    if (is_negative)
    {
        *is_negative = (*ptr) < 0;
    }
    return uint32_t(*ptr);
}


template <typename T>
struct extension_word {};

template <>
struct extension_word<uint8_t> { typedef uint16_t type; };

template <>
struct extension_word<uint16_t> { typedef uint16_t type; };

template <>
struct extension_word<uint32_t> { typedef uint32_t type; };



template <typename> 
struct increased_precision {};

template <>
struct increased_precision<uint8_t> { typedef uint16_t type; };

template <>
struct increased_precision<uint16_t> { typedef uint32_t type; };

template <>
struct increased_precision<uint32_t> { typedef uint64_t type; };