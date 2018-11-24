#pragma once
#include <sstream>
#include <stdexcept>

#define THROW(msg) { std::stringstream __stream; __stream << __FILE__ << ":" << __LINE__ << ": " << msg; throw std::runtime_error(__stream.str()); }
#define IF_FALSE_THROW(expr, msg) { if (!(expr)) { THROW(msg); } }
