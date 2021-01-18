#pragma once

// This exists cause some compilers do not have a compile-time macro for Endianess
// This was from the 'rapidjson' and was modified for our needs
#ifndef __BYTE_ORDER__
#define __ORDER_LITTLE_ENDIAN__ 1234
#define __ORDER_BIG_ENDIAN__    4321
// Detect with GLIBC's endian.h.
#if defined(__GLIBC__)
#include <endian.h>
#if (__BYTE_ORDER == __LITTLE_ENDIAN)
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#elif (__BYTE_ORDER == __BIG_ENDIAN)
#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
#else
#error "Unknown machine byteorder endianness detected. Please define '__BYTE_ORDER__'"
#endif
// Detect with _LITTLE_ENDIAN and _BIG_ENDIAN macro.
#elif defined(_LITTLE_ENDIAN) && !defined(_BIG_ENDIAN)
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#elif defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)
#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
// Detect with architecture macros.
#elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) ||         \
  defined(__ppc__) || defined(__hpux) || defined(__hppa) || defined(_MIPSEB) || defined(_POWER) || \
  defined(__s390__)
#define __BYTE_ORDER__ __ORDER_BIG_ENDIAN__
#elif defined(__i386__) || defined(__alpha__) || defined(__ia64) || defined(__ia64__) || \
  defined(_M_IX86) || defined(_M_IA64) || defined(_M_ALPHA) || defined(__amd64) ||       \
  defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64) || defined(__x86_64__) || \
  defined(_M_X64) || defined(__bfin__)
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#elif defined(_MSC_VER) && (defined(_M_ARM) || defined(_M_ARM64))
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#else
#include <cstdint>
#define __BYTE_ORDER_RUNTIME__
static inline bool is_system_little_endian()
{
    const int            value { 0x01 };
    const void *         address                   = static_cast<const void *>(&value);
    const unsigned char *least_significant_address = static_cast<const unsigned char *>(address);
    return (*least_significant_address == 0x01);
}
#endif
#endif
#if (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ != __ORDER_BIG_ENDIAN__)
#error "Unknown machine byteorder endianness detected. Please define '__BYTE_ORDER__'"
#endif