#ifndef TEST_UTILITY_HPP
#define TEST_UTILITY_HPP

#include <iostream>
#include <cstddef>
#include <immintrin.h>

// #define __AVX512BW__

namespace PQF {
    constexpr bool DEBUG = false;
    constexpr bool PARTIAL_DEBUG = false;
    constexpr bool NEW_HASH = false;
    constexpr bool DIAGNOSTICS = false;

    struct alignas(16) m128iWrapper {
        static constexpr __m128i zero = {0, 0};
        __m128i m;
        constexpr m128iWrapper(__m128i m = zero) : m(m) {}
        constexpr operator __m128i&() {return m;}
        constexpr operator __m128i() const {return m;}
    };

    // struct alignas(64) m512iWrapper {
    //     static constexpr __m512i zero = {0, 0, 0, 0, 0, 0, 0, 0};
    //     __m512i m;
    //     constexpr m512iWrapper(__m512i m = zero) : m(m) {}
    //     constexpr operator __m512i&() {return m;}
    //     constexpr operator __m512i() const {return m;}
    // };
}

void printBinaryUInt64(uint64_t x, bool newline=false, int divider=64);
// void print_vec(__m512i X, bool binary, int divider=64);

#endif
