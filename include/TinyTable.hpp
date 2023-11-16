#ifndef TINY_TABLE_HPP
#define TINY_TABLE_HPP

#include <cstdint>
#include <array>
#include <utility>
#include <immintrin.h>
#include <algorithm>

namespace DynamicPrefixFilter {
    inline constexpr __m512i GetResetVal() {
        std::array<std::uint64_t, 8> bytes;
        for(size_t i=0; i < 8; i++) {
            bytes[i] = -1ull;
        }
        return std::bit_cast<__m512i>(bytes);
    }

    struct alignas(64) TinyBuffer {
        static constexpr __m512i ResetVal = GetResetVal();

        __m512i buff;

        constexpr TinyBuffer() {
            buff = ResetVal;
        }

        void reset() {
            buff = ResetVal;
        }

        bool insert(std::uint64_t x) {
            __m512i vx = _mm512_set1_epi64(x);
            __mmask8 c = _mm512_cmp_epi64_mask(ResetVal, buff, _MM_CMPINT_EQ);
            __mmask8 lowbit = c & (~(c-1));
            buff = _mm512_mask_blend_epi64(lowbit, buff, vx);
            return c == lowbit;
        }

        bool query(std::uint64_t x) {
            __m512i vx = _mm512_set1_epi64(x);
            return _mm512_cmp_epi64_mask(vx, buff, _MM_CMPINT_EQ);
        }

        bool remove(std::uint64_t x) {
            __m512i vx = _mm512_set1_epi64(x);
            __mmask8 c = _mm512_cmp_epi64_mask(vx, buff, _MM_CMPINT_EQ);
            __mmask8 lowbit = c & (~(c-1));
            buff = _mm512_mask_blend_epi64(lowbit, buff, ResetVal);
            return c;
        }

        std::array<std::uint64_t, 8> getArray() {
            return std::bit_cast<std::array<std::uint64_t, 8>>(buff);
        }

        bool notFull() {
            return !_mm512_cmp_epi64_mask(ResetVal, buff, _MM_CMPINT_EQ);
        }
    };

    template<std::size_t NumBins>
    struct alignas(64) TinyTable{
        std::array<TinyBuffer, NumBins> table;

        bool insert(std::uint64_t x) {
            return table[x % NumBins].insert(x);
        }

        void reset() {
            std::fill(table.begin(), table.end(), TinyBuffer());
        }

        bool query(std::uint64_t x) {
            return table[x % NumBins].query(x);
        }

        bool remove(std::uint64_t x) {
            return table[x % NumBins].remove(x);
        }

        std::array<std::uint64_t, NumBins*8> getArray() {
            return std::bit_cast<std::array<std::uint64_t, NumBins*8>>(table);
        }

        bool notFull(std::uint64_t x) {
            return table[x % NumBins].notFull();
        }
    };
}

#endif