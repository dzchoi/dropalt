#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "fmt.h"



// Lua calls this hook to display numbers. It replaces newlib's floating-point printf
// formatter, which is not linked to reduce firmware size, while preserving Lua's %.7g
// style for 32-bit floats.
size_t lua_user_number2str(char* buffer, size_t size, float value)
{
    // Lua supplies a 50-byte buffer. The longest result here needs 14 bytes.
    if ( size < 14 ) {
        if ( size )
            *buffer = '\0';
        return 0;
    }

    union {
        float value;
        uint32_t bits;
    } number = { .value = value };
    const bool negative = number.bits >> 31;
    number.bits &= 0x7fffffffu;

    char* output = buffer;
    if ( negative )
        *output++ = '-';
    if ( number.bits > 0x7f800000u ) {
        __builtin_memcpy(output, "nan", 4);
        return output + 3 - buffer;
    }
    if ( number.bits == 0x7f800000u ) {
        __builtin_memcpy(output, "inf", 4);
        return output + 3 - buffer;
    }
    if ( number.bits == 0 ) {
        *output++ = '0';
        *output = '\0';
        return output - buffer;
    }

    // Keep the conversion in single precision so Cortex-M4F can use its hardware FPU.
    float normalized = number.value;
    int exponent = 0;
    while ( normalized >= 10.0f ) {
        normalized /= 10.0f;
        exponent++;
    }
    while ( normalized < 1.0f ) {
        normalized *= 10.0f;
        exponent--;
    }

    float scaled = number.value;
    for ( int places = 6 - exponent ; places > 0 ; places-- )
        scaled *= 10.0f;
    for ( int places = exponent - 6 ; places > 0 ; places-- )
        scaled /= 10.0f;
    uint32_t rounded = (uint32_t)scaled;
    float remainder = scaled - rounded;
    if ( remainder > 0.5f || (remainder == 0.5f && (rounded & 1)) )
        rounded++;
    if ( rounded == 10000000 ) {
        rounded = 1000000;
        exponent++;
    }

    char digits[7];
    size_t significant = fmt_u32_dec(digits, rounded);
    while ( significant > 1 && digits[significant - 1] == '0' )
        significant--;

    // Match %.7g: scientific notation below 1e-4 and from 1e7 upward.
    if ( exponent < -4 || exponent >= 7 ) {
        *output++ = digits[0];
        if ( significant > 1 ) {
            *output++ = '.';
            __builtin_memcpy(output, digits + 1, significant - 1);
            output += significant - 1;
        }
        *output++ = 'e';
        *output++ = exponent < 0 ? '-' : '+';
        unsigned absolute_exponent = exponent < 0 ? -exponent : exponent;
        *output++ = '0' + absolute_exponent / 10;
        *output++ = '0' + absolute_exponent % 10;
    }
    else if ( exponent < 0 ) {
        *output++ = '0';
        *output++ = '.';
        for ( int zeroes = -exponent - 1 ; zeroes ; zeroes-- )
            *output++ = '0';
        __builtin_memcpy(output, digits, significant);
        output += significant;
    }
    else {
        size_t integer_digits = exponent + 1;
        size_t copied = significant < integer_digits ? significant : integer_digits;
        __builtin_memcpy(output, digits, copied);
        output += copied;
        while ( copied++ < integer_digits )
            *output++ = '0';
        if ( significant > integer_digits ) {
            *output++ = '.';
            __builtin_memcpy(output, digits + integer_digits,
                             significant - integer_digits);
            output += significant - integer_digits;
        }
    }

    *output = '\0';
    return output - buffer;
}
