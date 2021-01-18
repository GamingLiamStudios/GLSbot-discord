#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <memory>
#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
#include <cxxabi.h>
#endif

struct f64x3
{
    double x;
    double y;
    double z;
};

struct f32x2
{
    float x;
    float y;
};

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
struct u128
{
    u64 upper;
    u64 lower;
};

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

template<typename T>
std::string type_name()
{
    int         status;
    std::string tname = typeid(T).name();
#if (defined(__GNUC__) || defined(__GNUG__)) && !defined(__clang__)
    char *demangled_name = abi::__cxa_demangle(tname.c_str(), NULL, NULL, &status);
    if (status == 0)
    {
        tname = demangled_name;
        free(demangled_name);
    }
#endif
    return tname;
}
