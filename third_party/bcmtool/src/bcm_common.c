#include <stdint.h>
#include "bcm_common.h"


static inline uint32_t bswap_32(uint32_t x) {
    return ((x & 0x000000FF) << 24) |
           ((x & 0x0000FF00) << 8) |
           ((x & 0x00FF0000) >> 8) |
           ((x & 0xFF000000) >> 24);
}

static inline uint16_t bswap_16(uint16_t x) {
    return (x << 8) | (x >> 8);
}

/** @brief converts 32-bit data from little endian format to Native's endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_LETONATIVE32_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return aData
    @endcode
*/
uint32_t CPU_LEToNative32(uint32_t aData)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return bswap_32(aData);
#else
    return aData;
#endif
}

/** @brief converts 32-bit data from Native's endian format to little endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_NATIVETOLE32_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return aData
    @endcode
*/
uint32_t CPU_NativeToLE32(uint32_t aData)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return bswap_32(aData);
#else
    return aData;
#endif
}

/** @brief converts 16-bit data from Native's endian format to little endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_NATIVETOLE16_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return aData
    @endcode
*/
uint16_t CPU_NativeToLE16(uint16_t aData)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return bswap_16(aData);
#else
    return aData;
#endif
}

/** @brief converts 16-bit data from big endian format to Native's endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_BETONATIVE16_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return CPU_NativeToBE16(aData)
    @endcode
*/
uint16_t CPU_BEToNative16(uint16_t aData)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return aData;
#else
    return bswap_16(aData);
#endif
}

/** @brief converts 32-bit data from big endian format to Native's endian format.

    @limitations None
    @trace #BRCM_SWARCH_CPU_BETONATIVE32_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    return CPU_NativeToBE32(aData)
    @endcode
*/
uint32_t CPU_BEToNative32(uint32_t aData)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return aData;
#else
    return bswap_32(aData);
#endif
}

/** @brief converts 64-bit data from little endian To Native's endian format

    @trace #BRCM_SWARCH_CPU_LETONATIVE64_PROC
    @trace #BRCM_SWREQ_CPU_ABSTRACTION
    @code{.c}
    @endcode
*/
uint64_t CPU_LEToNative64(uint64_t aData)
{
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return (((uint64_t)(CPU_LEToNative32(
                 (uint32_t)(aData))) << 32UL) |
                 CPU_LEToNative32(((uint32_t)(aData >> 32UL))));
#else
    return aData;
#endif
}

uint32_t ByteToU32(const uint8_t *bytes) 
{
    return (uint32_t)bytes[0] << 24 | 
           (uint32_t)bytes[1] << 16 | 
           (uint32_t)bytes[2] << 8  | 
           (uint32_t)bytes[3];
}

uint32_t ByteToU16(const uint8_t *bytes) 
{
    return (uint16_t)bytes[0] << 8  | 
           (uint16_t)bytes[1];
}
