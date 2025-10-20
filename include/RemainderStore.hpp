#ifndef REMAINDER_STORE_HPP
#define REMAINDER_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <algorithm>
#include <immintrin.h>
#include <cassert>
#include "TestUtility.hpp"
#include <bit>

namespace PQF {
    //All of these only work with 32 or 64 byte buckets. Not the best design I know, but it implicitly uses the bucket size to determine its pointer and to know what size AVX512 unit to fetch.

    template<std::size_t RemainderSize, std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore {
        static constexpr bool CanSplit = false;
        static_assert(RemainderSize != RemainderSize, "This remainder size is not supported.");

        // inline __m512i loadRemainders();
        // inline void storeRemainders(__m512i remainders);

        inline std::uint64_t insert(std::uint64_t remainder, std::size_t loc);

        inline void remove(std::size_t loc);

        //Removes and returns the value that was there
        inline std::uint64_t removeReturn(std::size_t loc);

        inline std::uint64_t removeFirst();

        inline std::uint64_t get(std::size_t loc) const;

        inline std::uint64_t queryNonVectorized(std::uint64_t remainder, std::pair<std::size_t, std::size_t> bounds);

        inline std::uint64_t queryVectorizedMask(std::uint64_t remainder, std::uint64_t mask);

        inline std::uint64_t queryVectorized(std::uint64_t remainder, std::pair<std::size_t, std::size_t> bounds);

        // Returns a bitmask of which remainders match within the bounds. Maybe this should return not a uint64_t but a mask type? Cause we should be able to do everything with them
        inline std::uint64_t query(std::uint64_t remainder, std::pair<size_t, size_t> bounds);
    };

    template<std::size_t SizeFirst, std::size_t SizeSecond, std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStoreTwoPieces {
        inline static constexpr bool CanSplit = false;

        using StoreFirstType = RemainderStore<SizeFirst, NumRemainders, Offset>;
        inline static constexpr std::size_t TotalSizeFirst = StoreFirstType::Size;
        using StoreSecondType = RemainderStore<SizeSecond, NumRemainders, Offset+TotalSizeFirst>;
        inline static constexpr std::size_t TotalSizeSecond = StoreSecondType::Size;
        inline static constexpr std::size_t Size = TotalSizeFirst+TotalSizeSecond;

        StoreFirstType storeFirstPart;
        StoreSecondType storeSecondPart;

        inline std::uint64_t insert(std::uint64_t remainder, std::size_t loc) {
            if constexpr (DEBUG) {
                assert(remainder <= (1ull << (SizeFirst + SizeSecond)) - 1);
                assert(loc <= NumRemainders);
            }

            uint64_t remainderFirstPart = remainder & ((1ull << SizeFirst) - 1);
            uint64_t remainderSecondPart = remainder >> SizeFirst;
            uint64_t overflow = storeFirstPart.insert(remainderFirstPart, loc) << 4ull;
            overflow |= storeSecondPart.insert(remainderSecondPart, loc);
            return overflow;
        }

        inline void remove(std::size_t loc) {
            storeFirstPart.remove(loc);
            storeSecondPart.remove(loc);
        }

        inline std::uint64_t removeReturn(std::size_t loc) {
            uint64_t retvalFirstPart = storeFirstPart.removeReturn(loc);
            uint64_t retvalSecondPart = storeSecondPart.removeReturn(loc);
            return retvalFirstPart + (retvalSecondPart << SizeFirst);
        }

        inline std::uint64_t get(std::size_t loc) const{
            uint64_t retvalFirstPart = storeFirstPart.get(loc);
            uint64_t retvalSecondPart = storeSecondPart.get(loc);
            return retvalFirstPart + (retvalSecondPart << SizeFirst);
        }

        inline std::uint64_t query(std::uint64_t remainder, std::pair<size_t, size_t> bounds) {
            if constexpr (DEBUG) {
                assert(remainder <= (1ull << (SizeFirst + SizeSecond)) - 1);
                assert(bounds.second <= NumRemainders);
            }

            uint64_t remainderFirstPart = remainder & ((1ull << SizeFirst) - 1);
            uint64_t remainderSecondPart = remainder >> SizeFirst;

            return storeFirstPart.query(remainderFirstPart, bounds) & storeSecondPart.query(remainderSecondPart, bounds);
        }

        //This is really just adhoc stuff to get the deletions to work, so that we find where the keys are that match the bucket we're coming from
        inline std::uint64_t query4BitPartMask(std::uint_fast8_t bits, std::uint64_t mask) {
            return storeSecondPart.queryVectorizedMask(bits, mask);
        }

        inline std::uint64_t queryVectorizedMask(std::uint64_t remainder, std::uint64_t mask) {
            if constexpr (DEBUG) {
                assert(remainder <= (1ull << (SizeFirst + SizeSecond)) - 1);
            }

            uint64_t remainderFirstPart = remainder & ((1ull << SizeFirst) - 1);
            uint64_t remainderSecondPart = remainder >> SizeFirst;

            return storeFirstPart.queryVectorizedMask(remainderFirstPart, mask) & storeSecondPart.queryVectorizedMask(remainderSecondPart, mask);
        }
    };


    #ifdef AVX512

    struct alignas(64) m512iWrapper {
        static constexpr __m512i zero = {0, 0, 0, 0, 0, 0, 0, 0};
        __m512i m;
        constexpr m512iWrapper(__m512i m = zero) : m(m) {}
        constexpr operator __m512i&() {return m;}
        constexpr operator __m512i() const {return m;}
    };

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<8, NumRemainders, Offset> {
        inline static constexpr bool CanSplit = true;

        inline static constexpr std::size_t Size = NumRemainders;
        std::array<std::uint8_t, NumRemainders> remainders;

        inline static constexpr __mmask64 StoreMask = (NumRemainders + Offset < 64) ? ((1ull << (NumRemainders+Offset)) - (1ull << Offset)) : (-(1ull << Offset));

        inline __m512i* getNonOffsetBucketAddress2() {
            return reinterpret_cast<__m512i*>(reinterpret_cast<std::uint8_t*>(&remainders) - Offset);
        }

        inline __m512i loadRemainders() { //Stopgap measure to at least avoid loading and storing across cache lines
            __m512i* nonOffsetAddr = getNonOffsetBucketAddress2();
            if(Size + Offset <= 32) {
                return _mm512_castsi256_si512(_mm256_load_si256((__m256i*)nonOffsetAddr));
            }
            else{ 
                return _mm512_load_si512(nonOffsetAddr);
            }
        }

        inline void storeRemainders(__m512i remainders) {
            __m512i* nonOffsetAddr = getNonOffsetBucketAddress2();
            if(Size + Offset <= 32) {
                _mm256_store_si256((__m256i*)nonOffsetAddr, _mm512_castsi512_si256(remainders));
            }
            else{ 
                _mm512_store_si512(nonOffsetAddr, remainders);
            }
        }

        //TODO: make insert use the shuffle instruction & precompute all the shuffle vectors. Also do smth with the getNonOffsetBucketAddress? Cause that's an unnecessary instruction.
        inline static constexpr __m512i getShuffleVector(std::size_t loc) {
            std::array<unsigned char, 64> bytes;
            for(size_t i=0; i < 64; i++) {
                if (i < loc+Offset) {
                    bytes[i] = i;
                }
                else if (i == loc+Offset) {
                    bytes[i] = 0;
                }
                else if (i < Size+Offset){
                    bytes[i] = i-1;
                }
                else {
                    bytes[i] = i;
                }
            }
            return std::bit_cast<__m512i>(bytes);
        }

        inline static constexpr std::array<m512iWrapper, 64> getShuffleVectors() {
            std::array<m512iWrapper, 64> masks;
            for(size_t i = 0; i < 64; i++) {
                masks[i] = getShuffleVector(i);
            }
            return masks;
        }

        inline static constexpr __m512i getRemoveShuffleVector(std::size_t loc) {
            std::array<unsigned char, 64> bytes;
            for(size_t i=0; i < 64; i++) {
                if (i < loc+Offset) {
                    bytes[i] = i;
                }
                else if (i < Size+Offset){
                    bytes[i] = i+1;
                }
                else {
                    bytes[i] = i;
                }
            }
            return std::bit_cast<__m512i>(bytes);
        }

        inline static constexpr std::array<m512iWrapper, 64> getRemoveShuffleVectors() {
            std::array<m512iWrapper, 64> masks;
            for(size_t i = 0; i < 64; i++) {
                masks[i] = getRemoveShuffleVector(i);
            }
            return masks;
        }

        inline static constexpr std::array<m512iWrapper, 64> shuffleVectors = getShuffleVectors();
        inline static constexpr std::array<m512iWrapper, 64> removeShuffleVectors = getRemoveShuffleVectors();

        inline std::uint_fast8_t insert(std::uint_fast8_t remainder, std::size_t loc) {
            if constexpr (DEBUG) {
                assert(loc < NumRemainders);
            }
            std::uint_fast8_t retval = remainders[NumRemainders-1];

            __m512i packedStore = loadRemainders();
            __m512i packedStoreWithRemainder = _mm512_mask_set1_epi8(packedStore, 1, remainder);
            packedStore = _mm512_mask_permutexvar_epi8(packedStore, StoreMask, shuffleVectors[loc], packedStoreWithRemainder);
            storeRemainders(packedStore);

            return retval;
        }

        inline void remove(std::size_t loc) {
            __m512i packedStore = loadRemainders();
            packedStore = _mm512_mask_permutexvar_epi8(packedStore, StoreMask, removeShuffleVectors[loc], packedStore);
            storeRemainders(packedStore);
        }

        //Removes and returns the value that was there
        inline std::uint_fast8_t removeReturn(std::size_t loc) {
            std::uint_fast8_t retval = remainders[loc];
            remove(loc);
            return retval;
        }

        inline std::uint_fast8_t removeFirst() {
            std::uint_fast8_t first = remainders[0];
            remove(0);
            return first;
        }

        inline std::uint64_t get(std::size_t loc) const {
            return remainders[loc];
        }

        inline std::uint64_t queryNonVectorized(std::uint_fast8_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            std::uint64_t retMask = 0;
            for(size_t i{bounds.first}; i < bounds.second; i++) {
                if(remainders[i] == remainder) {
                    retMask |= 1ull << i;
                }
            }
            return retMask;
        }

        inline std::uint64_t queryVectorizedMask(std::uint_fast8_t remainder, std::uint64_t mask) {
            __m512i packedStore = loadRemainders();
            __m512i remainderVec = _mm512_maskz_set1_epi8(-1ull, remainder);
            return (_cvtmask64_u64(_mm512_mask_cmpeq_epu8_mask(-1ull, packedStore, remainderVec)) >> Offset) & mask;
        }

        inline std::uint64_t queryVectorized(std::uint_fast8_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            __mmask64 queryMask = _cvtu64_mask64(((1ull << bounds.second) - (1ull << bounds.first)) << Offset);
            __m512i packedStore = loadRemainders();
            __m512i remainderVec = _mm512_maskz_set1_epi8(-1ull, remainder);
            return _cvtmask64_u64(_mm512_mask_cmpeq_epu8_mask(queryMask, packedStore, remainderVec)) >> Offset;
        }

        // Returns a bitmask of which remainders match within the bounds. Maybe this should return not a uint64_t but a mask type? Cause we should be able to do everything with them
        inline std::uint64_t query(std::uint_fast8_t remainder, std::pair<size_t, size_t> bounds) {
            if constexpr (DEBUG) {
                assert(bounds.second <= NumRemainders);
            }
            return queryVectorized(remainder, bounds);
        }

        inline void split(RemainderStore& a, RemainderStore& b, uint64_t mask) const {
            static constexpr uint64_t AddMask = (1ull << Offset) - 1;
            mask <<= Offset;
            uint64_t bmask = (~mask) & StoreMask;

            //Should I even retain the old remainderstore in a? As we are building it. But anyways for safety let's just keep it as is. Not too high overhead, except maybe in the case of non-temporal stores for 512 bit remainder store.
            mask += AddMask;
            bmask += AddMask;
            __m512i packedStore = loadRemainders();
            __m512i aPackedStore = a.loadRemainders();
            __m512i bPackedStore = b.loadRemainders();

            aPackedStore = _mm512_mask_compress_epi8(aPackedStore, mask, packedStore);
            bPackedStore = _mm512_mask_compress_epi8(aPackedStore, bmask, packedStore);

            a.storeRemainders(aPackedStore);
            b.storeRemainders(bPackedStore);
        }
    };


    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<4, NumRemainders, Offset> {
        inline static constexpr bool CanSplit = false;

        inline static constexpr std::size_t Size = (NumRemainders+1)/2;
        std::array<std::uint8_t, Size> remainders;

        inline static constexpr __mmask64 StoreMask = (Size + Offset < 64) ? ((1ull << (Size+Offset)) - (1ull << Offset)) : (-(1ull << Offset));

        inline __m512i* getNonOffsetBucketAddress2() {
            return reinterpret_cast<__m512i*>(reinterpret_cast<std::uint8_t*>(&remainders) - Offset);
        }

        inline __m512i loadRemainders() { //Stopgap measure to at least avoid loading and storing across cache lines
            __m512i* nonOffsetAddr = getNonOffsetBucketAddress2();
            if(Size + Offset <= 32) {
                return _mm512_castsi256_si512(_mm256_load_si256((__m256i*)nonOffsetAddr));
            }
            else{ 
                return _mm512_load_si512(nonOffsetAddr);
            }
        }

        inline void storeRemainders(__m512i remainders) {
            __m512i* nonOffsetAddr = getNonOffsetBucketAddress2();
            if(Size + Offset <= 32) {
                _mm256_store_si256((__m256i*)nonOffsetAddr, _mm512_castsi512_si256(remainders));
            }
            else{ 
                _mm512_store_si512(nonOffsetAddr, remainders);
            }
        }

        //bitGroup = 0 if lower order, 1 if higher order
        inline static std::uint_fast8_t get4Bits(std::uint_fast8_t byte, std::uint_fast8_t bitGroup) {
            if constexpr (DEBUG) assert(bitGroup <= 1 && (byte & (0b1111 << (bitGroup*4))) >> (bitGroup*4) <16);
            return (byte & (0b1111 << (bitGroup*4))) >> (bitGroup*4);
        }

        inline static constexpr __m512i getRemainderStoreMask(std::size_t loc) {
            // return _mm512_maskz_set1_epi8(_cvtu64_mask64(1ull << ((loc >> 1) + Offset)), 15ull << ((loc & 1)*4));
            std::array<unsigned char, 64> bytes;
            for(size_t i=0; i < 64; i++) {
                if(i == ((loc >> 1) + Offset)) {
                    bytes[i] = 15ull << ((loc & 1)*4);
                }
                else {
                    bytes[i] = 0;
                }
            }
            return std::bit_cast<__m512i>(bytes);
        }

        inline static constexpr std::array<m512iWrapper, 64> getRemainderStoreMasks() {
            std::array<m512iWrapper, 64> masks;
            for(size_t i = 0; i < 64; i++) {
                masks[i] = getRemainderStoreMask(i);
            }
            return masks;
        }

        inline static constexpr std::array<m512iWrapper, 64> remainderStoreMasks = getRemainderStoreMasks();

        inline static constexpr __m512i getPackedStoreMask(std::size_t loc) {
            std::array<unsigned char, 64> bytes;
            for(size_t i=0; i < 64; i++) {
                if(i < ((loc >> 1) + Offset)) {
                    bytes[i] = 255;
                }
                else if(i == ((loc >> 1) + Offset) && loc % 2 == 1) {
                    bytes[i] = 15;
                }
                else {
                    bytes[i] = 0;
                }
            }
            return std::bit_cast<__m512i>(bytes);
        }

        inline static constexpr std::array<m512iWrapper, 64> getPackedStoreMasks() {
            std::array<m512iWrapper, 64> masks;
            for(size_t i = 0; i < 64; i++) {
                masks[i] = getPackedStoreMask(i);
            }
            return masks;
        }

        inline static constexpr std::array<m512iWrapper, 64> packedStoreMasks = getPackedStoreMasks();

        inline static void set4Bits(std::uint_fast8_t& byte, std::uint_fast8_t bitGroup, std::uint_fast8_t bits) {
            if constexpr (DEBUG) assert(bitGroup <= 1 && bits < 16);
            byte = (byte & (0b1111 << ((1-bitGroup)*4))) + (bits << (bitGroup*4));
        }

        //Seems quite inneficient honestly
        inline std::uint_fast8_t insert(std::uint_fast8_t remainder, std::size_t loc) {
            if constexpr (DEBUG) {
                assert(loc <= NumRemainders);
            }

            if(loc == NumRemainders) return remainder;

            std::uint_fast8_t retval = get4Bits(remainders[(NumRemainders-1)/2], (NumRemainders-1)%2);
            std::uint_fast8_t remainderDoubledToFullByte = remainder * 17;
            __m512i packedStore = loadRemainders();
            __m512i packedRemainders = _mm512_maskz_set1_epi8(_cvtu64_mask64(-1ull), remainderDoubledToFullByte);
            __m512i shuffleMoveRight = {7, 0, 1, 2, 3, 4, 5, 6};
            __m512i packedStoreShiftedRight = _mm512_permutexvar_epi64(shuffleMoveRight, packedStore);
            __m512i packedStoreShiftedRight4Bits = _mm512_shldi_epi64(packedStore, packedStoreShiftedRight, 4); //Yes this says shldi which is left shift and well we are doing a left shift but that's in big endian, and well when I work with intrinsics I start thinking little endian.
            __m512i newPackedStore = _mm512_ternarylogic_epi32(packedStoreShiftedRight4Bits, packedStore, packedStoreMasks[loc], 0b11011000);
            newPackedStore = _mm512_ternarylogic_epi32(newPackedStore, packedRemainders, remainderStoreMasks[loc], 0b11011000);
            storeRemainders(_mm512_mask_blend_epi8(StoreMask, packedStore, newPackedStore));
            return retval;
        }

        inline void remove(std::size_t loc) {
            __m512i packedStore = loadRemainders();
            __m512i shuffleMoveLeft = {1, 2, 3, 4, 5, 6, 7, 0};
            __m512i packedStoreShiftedLeft = _mm512_permutexvar_epi64(shuffleMoveLeft, packedStore);
            __m512i packedStoreShiftedLeft4Bits = _mm512_shrdi_epi64(packedStore, packedStoreShiftedLeft, 4);
            __m512i newPackedStore = _mm512_ternarylogic_epi32(packedStoreShiftedLeft4Bits, packedStore, packedStoreMasks[loc], 0b11011000);
            storeRemainders(_mm512_mask_blend_epi8(StoreMask, packedStore, newPackedStore));
        }

        inline std::uint_fast8_t removeReturn(std::size_t loc) {
            std::uint_fast8_t retval = get4Bits(remainders[loc/2], loc%2);
            remove(loc);
            return retval;
        }

        inline std::uint64_t get(std::size_t loc) const {
            return get4Bits(remainders[loc/2], loc % 2);
        }

        inline std::uint_fast8_t removeFirst() {
            std::uint_fast8_t first = get4Bits(remainders[0], 0);
            remove(0);
            return first;
        }

        inline std::uint64_t queryNonVectorized(std::uint_fast8_t remainder, std::pair<size_t, size_t> bounds) {
            std::uint64_t retMask = 0;
            for(size_t i{bounds.first}; i < bounds.second; i++) {
                if(get4Bits(remainders[i/2], i%2) == remainder) {
                    retMask |= 1ull << i;
                }assert(remainder < 16);
            }
            return retMask;
        }

        //Resulting vector would have doubled elements of the array of remainders, so that we could then mask off one for the first 4 bit remainder and one for the second
        inline static constexpr __m512i get4BitExpanderShuffle() {
            std::array<unsigned char, 64> bytes;
            for(size_t i=0; i < 64; i++) {
                bytes[i] = (i/2) + Offset;
            }
            return std::bit_cast<__m512i>(bytes);
        }
        
        inline static constexpr __m512i getDoublePackedStoreTernaryMask() {
            std::array<unsigned char, 64> bytes;
            for(size_t i=0; i < 64; i++) {
                bytes[i] = 15 << ((i%2) * 4);
            }
            return std::bit_cast<__m512i>(bytes);
        }

        inline std::uint64_t queryVectorizedMask(std::uint_fast8_t remainder, std::uint64_t mask) {
            __m512i packedStore = loadRemainders();
            __m512i packedRemainder = _mm512_maskz_set1_epi8(-1ull, remainder*17);
            static constexpr __m512i expanderShuffle = get4BitExpanderShuffle();
            __m512i doubledPackedStore = _mm512_permutexvar_epi8(expanderShuffle, packedStore);
            __m512i maskedXNORedPackedStore = _mm512_ternarylogic_epi32(doubledPackedStore, packedRemainder, getDoublePackedStoreTernaryMask(), 0b00101000);
            __mmask64 compared = _knot_mask64(_mm512_test_epi8_mask(maskedXNORedPackedStore, maskedXNORedPackedStore));
            return _cvtmask64_u64(compared) & mask;
        }

        inline std::uint64_t queryVectorized(std::uint_fast8_t remainder, std::pair<size_t, size_t> bounds) {
            __m512i packedStore = loadRemainders();
            __m512i packedRemainder = _mm512_maskz_set1_epi8(_cvtu64_mask64(-1ull), remainder*17);
            static constexpr __m512i expanderShuffle = get4BitExpanderShuffle();
            __m512i doubledPackedStore = _mm512_permutexvar_epi8(expanderShuffle, packedStore);
            __m512i maskedXNORedPackedStore = _mm512_ternarylogic_epi32(doubledPackedStore, packedRemainder, getDoublePackedStoreTernaryMask(), 0b00101000);
            __mmask64 compared = _knot_mask64(_mm512_test_epi8_mask(maskedXNORedPackedStore, maskedXNORedPackedStore));
            return _cvtmask64_u64(compared) & ((1ull << bounds.second) - (1ull << bounds.first));
        }

        // Returns a bitmask of which remainders match within the bounds. Maybe this should return not a uint64_t but a mask type? Cause we should be able to do everything with them
        inline std::uint64_t query(std::uint_fast8_t remainder, std::pair<size_t, size_t> bounds) {
            if constexpr (DEBUG) {
                assert(remainder < 16);
                assert(bounds.second <= NumRemainders);
            }
            return queryVectorized(remainder, bounds);
        }
    };

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<12, NumRemainders, Offset> : RemainderStoreTwoPieces<8, 4, NumRemainders, Offset>{};

    //Requires the remainder to be aligned to two byte boundary, as otherwise could not use the 2-byte AVX512 instruction and would be inneficient
    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<16, NumRemainders, Offset> {
        inline static constexpr bool CanSplit = false;

        inline static constexpr std::size_t Size = NumRemainders * 2;
        std::array<std::uint16_t, NumRemainders> remainders;

        static_assert(Offset % 2 == 0);

        inline static constexpr size_t WordOffset = Offset/2;

        inline static constexpr __mmask32 StoreMask = (NumRemainders + WordOffset < 32) ? ((1u << (NumRemainders+WordOffset)) - (1u << WordOffset)) : (-(1u << WordOffset));

        inline __m512i* getNonOffsetBucketAddress2() {
            return reinterpret_cast<__m512i*>(reinterpret_cast<std::uint8_t*>(&remainders) - Offset);
        }

        inline __m512i loadRemainders() { //Stopgap measure to at least avoid loading and storing across cache lines
            __m512i* nonOffsetAddr = getNonOffsetBucketAddress2();
            if(Size + Offset <= 32) {
                return _mm512_castsi256_si512(_mm256_load_si256((__m256i*)nonOffsetAddr));
            }
            else{ 
                return _mm512_load_si512(nonOffsetAddr);
            }
        }

        inline void storeRemainders(__m512i remainders) {
            __m512i* nonOffsetAddr = getNonOffsetBucketAddress2();
            if(Size + Offset <= 32) {
                _mm256_store_si256((__m256i*)nonOffsetAddr, _mm512_castsi512_si256(remainders));
            }
            else{ 
                _mm512_store_si512(nonOffsetAddr, remainders);
            }
        }

        inline static constexpr __m512i getShuffleVector(std::size_t loc) {
            std::array<std::uint16_t, 32> words;
            for(size_t i=0; i < 32; i++) {
                if (i < loc+WordOffset) {
                    words[i] = i;
                }
                else if (i == loc+WordOffset) {
                    words[i] = 0;
                }
                else if (i < NumRemainders+WordOffset){
                    words[i] = i-1;
                }
                else {
                    words[i] = i;
                }
            }
            return std::bit_cast<__m512i>(words);
        }

        inline static constexpr std::array<m512iWrapper, 32> getShuffleVectors() {
            std::array<m512iWrapper, 32> masks;
            for(size_t i = 0; i < 32; i++) {
                masks[i] = getShuffleVector(i);
            }
            return masks;
        }

        inline static constexpr __m512i getRemoveShuffleVector(std::size_t loc) {
            std::array<uint16_t, 32> words;
            for(size_t i=0; i < 32; i++) {
                if (i < loc+WordOffset) {
                    words[i] = i;
                }
                else if (i < NumRemainders+WordOffset){
                    words[i] = i+1;
                }
                else {
                    words[i] = i;
                }
            }
            return std::bit_cast<__m512i>(words);
        }

        inline static constexpr std::array<m512iWrapper, 32> getRemoveShuffleVectors() {
            std::array<m512iWrapper, 32> masks;
            for(size_t i = 0; i < 32; i++) {
                masks[i] = getRemoveShuffleVector(i);
            }
            return masks;
        }

        inline static constexpr std::array<m512iWrapper, 32> shuffleVectors = getShuffleVectors();
        inline static constexpr std::array<m512iWrapper, 32> removeShuffleVectors = getRemoveShuffleVectors();

        inline std::uint_fast16_t insert(std::uint_fast16_t remainder, std::size_t loc) {
            if constexpr (DEBUG) {
                assert(loc < NumRemainders);
            }
            std::uint_fast16_t retval = remainders[NumRemainders-1];

            __m512i packedStore = loadRemainders();
            __m512i packedStoreWithRemainder = _mm512_mask_set1_epi16(packedStore, 1, remainder);
            packedStore = _mm512_mask_permutexvar_epi16(packedStore, StoreMask, shuffleVectors[loc], packedStoreWithRemainder);
            storeRemainders(packedStore);

            return retval;
        }

        inline void remove(std::size_t loc) {
            __m512i packedStore = loadRemainders();
            packedStore = _mm512_mask_permutexvar_epi16(packedStore, StoreMask, removeShuffleVectors[loc], packedStore);
            storeRemainders(packedStore);
        }

        //Removes and returns the value that was there
        inline std::uint_fast16_t removeReturn(std::size_t loc) {
            std::uint_fast16_t retval = remainders[loc];
            remove(loc);
            return retval;
        }

        inline std::uint64_t get(std::size_t loc) const {
            return remainders[loc];
        }

        inline std::uint_fast16_t removeFirst() {
            std::uint_fast16_t first = remainders[0];
            remove(0);
            return first;
        }

        inline std::uint64_t queryNonVectorized(std::uint_fast16_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            std::uint64_t retMask = 0;
            for(size_t i{bounds.first}; i < bounds.second; i++) {
                if(remainders[i] == remainder) {
                    retMask |= 1ull << i;
                }
            }
            return retMask;
        }

        inline std::uint64_t queryVectorizedMask(std::uint_fast16_t remainder, std::uint64_t mask) {
            __m512i packedStore = loadRemainders();
            __m512i remainderVec = _mm512_maskz_set1_epi16(-1u, remainder);
            return (_cvtmask32_u32(_mm512_mask_cmpeq_epu16_mask(-1u, packedStore, remainderVec)) >> WordOffset) & mask;
        }

        inline std::uint64_t queryVectorized(std::uint_fast16_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            __mmask32 queryMask = _cvtu32_mask32(((1u << bounds.second) - (1u << bounds.first)) << WordOffset);
            __m512i packedStore = loadRemainders();
            __m512i remainderVec = _mm512_maskz_set1_epi16(-1, remainder);
            return _cvtmask32_u32(_mm512_mask_cmpeq_epu16_mask(queryMask, packedStore, remainderVec)) >> WordOffset;
        }

        // Returns a bitmask of which remainders match within the bounds. Maybe this should return not a uint64_t but a mask type? Cause we should be able to do everything with them
        inline std::uint64_t query(std::uint_fast16_t remainder, std::pair<size_t, size_t> bounds) {
            if constexpr (DEBUG) {
                assert(bounds.second <= NumRemainders);
            }
            return queryVectorized(remainder, bounds);
        }
    };

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<20, NumRemainders, Offset> : RemainderStoreTwoPieces<16, 4, NumRemainders, Offset>{};

    


    #else

    // AVX2 implementations

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<8, NumRemainders, Offset> {
        inline static constexpr bool CanSplit = false;

        inline static constexpr std::size_t Size = NumRemainders;
        std::array<std::uint8_t, NumRemainders> remainders;

        inline __m256i* getNonOffsetBucketAddress2() {
            return reinterpret_cast<__m256i*>(reinterpret_cast<std::uint8_t*>(&remainders[0]) - Offset);
        }

        inline std::uint_fast8_t insert(std::uint_fast8_t remainder, std::size_t loc) {
            // std::cout << "inserting " << ((std::size_t)remainder) << " " << loc << std::endl;
            if (loc >= NumRemainders) return remainder;
            std::uint_fast8_t retval = remainders[NumRemainders-1];
            
            std::memmove((&remainders[loc])+1, &remainders[loc], NumRemainders - loc - 1);
            remainders[loc] = remainder;

            return retval;
        }

        inline void remove(std::size_t loc) {
            // std::cout << "removing " << loc << std::endl;
            std::memmove(&remainders[loc], (&remainders[loc]) + 1, NumRemainders - loc - 1);
        }

        //Removes and returns the value that was there
        inline std::uint_fast8_t removeReturn(std::size_t loc) {
            std::uint_fast8_t retval = remainders[loc];
            remove(loc);
            return retval;
        }

        inline std::uint_fast8_t removeFirst() {
            std::uint_fast8_t first = remainders[0];
            remove(0);
            return first;
        }

        inline std::uint64_t queryNonVectorized(std::uint_fast8_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            std::uint64_t retMask = 0;
            for(size_t i{bounds.first}; i < bounds.second; i++) {
                if(remainders[i] == remainder) {
                    retMask |= 1ull << i;
                }
            }
            return retMask;
        }

        inline std::uint64_t queryVectorizedMask(std::uint_fast8_t remainder, std::uint64_t mask) {
            __m256i remainderVec = _mm256_set1_epi8(remainder);
            __m256i packedStore1 = _mm256_loadu_si256(getNonOffsetBucketAddress2());
            __m256i cmp1 = _mm256_cmpeq_epi8(remainderVec, packedStore1);
            // lolol ridiculous cast but otherwise adds 1s when casting negative number which is rather undesirable.
            std::uint64_t result1 = static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(_mm256_movemask_epi8(cmp1))) >> Offset;

            if constexpr (Offset + NumRemainders <= 32) {
                return result1 & mask;
            }

            __m256i packedStore2 = _mm256_loadu_si256(getNonOffsetBucketAddress2() + 1);
            __m256i cmp2 = _mm256_cmpeq_epi8(remainderVec, packedStore2);
            // Assuming offset < 32 here! Which should be true since otherwise would cross cacheline boundary if more than 32 bytes
            assert (Offset < 32);
            std::uint64_t result2 = static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(_mm256_movemask_epi8(cmp2))) << (32 - Offset);

            // std::cout << result1 << " " << result2 << std::endl;

            return (result1 | result2) & mask;
        }

        inline std::uint64_t queryVectorized(std::uint_fast8_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            return queryVectorizedMask(remainder, (1ull << bounds.second) - (1ull << bounds.first));
        }

        // Returns a bitmask of which remainders match within the bounds. Maybe this should return not a uint64_t but a mask type? Cause we should be able to do everything with them
        inline std::uint64_t query(std::uint_fast8_t remainder, std::pair<size_t, size_t> bounds) {
            if constexpr (DEBUG) {
                assert(bounds.second <= NumRemainders);
            }
            return queryVectorized(remainder, bounds);
        }
    };


    // 4 BIT

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<4, NumRemainders, Offset> {
        static constexpr std::size_t OffsetHalf = (Offset >= 32) ? Offset - 32 : Offset;
        inline static constexpr bool CanSplit = false;

        inline static constexpr std::size_t Size = (NumRemainders+1)/2;
        static_assert((Size + Offset-1) / 32 - (Offset / 32) == 0, "AVX2 4 bit store only implemented for something fitting neatly into 256 bits.");
        std::array<std::uint8_t, Size> remainders;

        inline __m256i* getNonOffsetBucketAddress2() {
            return reinterpret_cast<__m256i*>(reinterpret_cast<std::uint8_t*>(&remainders[0]) - OffsetHalf);
        }

        inline static void set4Bits(std::uint_fast8_t& byte, std::uint_fast8_t bitGroup, std::uint_fast8_t bits) {
            if constexpr (DEBUG) assert(bitGroup <= 1 && bits < 16);
            byte = (byte & (0b1111 << ((1-bitGroup)*4))) + (bits << (bitGroup*4));
        }

        //bitGroup = 0 if lower order, 1 if higher order
        inline static std::uint_fast8_t get4Bits(std::uint_fast8_t byte, std::uint_fast8_t bitGroup) {
            if constexpr (DEBUG) assert(bitGroup <= 1 && (byte & (0b1111 << (bitGroup*4))) >> (bitGroup*4) <16);
            return (byte & (0b1111 << (bitGroup*4))) >> (bitGroup*4);
        }

        inline std::uint64_t get(std::size_t loc) const {
            return get4Bits(remainders[loc/2], loc % 2);
        }

        //Just do it direclty with 64 bit numbers rather than avx2, honestly probably not worse (maybe worse but whatever)
        inline std::uint_fast8_t insert(std::uint_fast8_t remainder, std::size_t loc) {
            if (loc >= NumRemainders) {
                return remainder;
            }
            std::uint_fast8_t retval = get(NumRemainders-1);

            std::uint64_t temp[4];
            // WOW &remainders != &remainders[0]!!!! That is crazy! (it was overwriting past the end)
            std::memcpy(temp, (&remainders[0]) + (loc/2), Size - (loc/2));
            std::uint64_t first4 = temp[0] & 0b1111;
            temp[3] = (temp[2] >> 60) | (temp[3] << 4ull);
            temp[2] = (temp[1] >> 60) | (temp[2] << 4ull);
            temp[1] = (temp[0] >> 60) | (temp[1] << 4ull);
            std::uint64_t bitGroup = (loc % 2) * 4;
            std::uint64_t first_byte = (first4 << (4 - bitGroup)) + (remainder << bitGroup);
            temp[0] = ((temp[0] & (~0b1111)) << 4ull) | first_byte;

            // std::cout << "jj " << ((uint64_t)remainders[Size]) << std::endl;
            std::memcpy((&remainders[0]) + (loc/2), temp, Size - (loc/2));
            // std::cout << "jjk " << ((uint64_t)remainders[Size]) << std::endl;

            return retval;
        }

        inline void remove(std::size_t loc) {
            std::uint64_t temp[4];
            std::memcpy(temp, (&remainders[0]) + loc/2, Size - loc/2);
            std::uint64_t first4 = temp[0] & 0b1111;
            temp[0] = (temp[0] >> 4ull) | (temp[1] << 60ull);
            temp[1] = (temp[1] >> 4ull) | (temp[2] << 60ull);
            temp[2] = (temp[2] >> 4ull) | (temp[3] << 60ull);
            temp[3] = temp[3] >> 4ull;
            //means deleting middle 4 of the first byte, not the first 4, so gotta bring first 4 back
            if (loc % 2 == 1) 
                temp[0] = (temp[0] & (~0b1111)) | first4;

            std::memcpy((&remainders[0]) + loc/2, temp, Size - loc/2);
        }

        //Removes and returns the value that was there
        inline std::uint_fast8_t removeReturn(std::size_t loc) {
            std::uint_fast8_t retval = get(loc);
            remove(loc);
            return retval;
        }

        inline std::uint_fast8_t removeFirst() {
            std::uint_fast8_t first = get(0);
            remove(0);
            return first;
        }

        inline std::uint64_t queryNonVectorized(std::uint_fast8_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            std::uint64_t retMask = 0;
            for(size_t i{bounds.first}; i < bounds.second; i++) {
                if(get4Bits(remainders[i/2], i%2) == remainder) {
                    retMask |= 1ull << i;
                }assert(remainder < 16);
            }
            return retMask;
        }

        std::uint64_t interleave_bits(std::uint64_t a, std::uint64_t b) {
            constexpr std::uint64_t mask_even = 0x5555555555555555ull;
            constexpr std::uint64_t mask_odd  = 0xAAAAAAAAAAAAAAAAull;

            std::uint64_t a_spread = _pdep_u64(a, mask_even);
            std::uint64_t b_spread = _pdep_u64(b, mask_odd);
            return a_spread | b_spread;
        }


        inline std::uint64_t queryVectorizedMask(std::uint_fast8_t remainder, std::uint64_t mask) {
            //since we're assuming it fits in 32 bytes shouldn't be too bad.

            constexpr __m256i bottommask = {0x0F0F0F0F0F0F0F0Full, 0x0F0F0F0F0F0F0F0Full, 0x0F0F0F0F0F0F0F0Full, 0x0F0F0F0F0F0F0F0Full};
            //Why did intel choose to do this as a signed integer wth
            constexpr long long int topmaskone = std::bit_cast<long long int>(0xF0F0F0F0F0F0F0F0ull);
            constexpr __m256i topmask = {topmaskone, topmaskone, topmaskone, topmaskone};

            __m256i remainderVec_low = _mm256_set1_epi8(remainder);
            __m256i remainderVec_high = _mm256_set1_epi8(remainder << 4);
            __m256i packedStore = _mm256_loadu_si256(getNonOffsetBucketAddress2());
            __m256i packedStore_low = _mm256_and_si256(packedStore, bottommask);
            __m256i packedStore_high = _mm256_and_si256(packedStore, topmask);
            __m256i cmp_low = _mm256_cmpeq_epi8(packedStore_low, remainderVec_low);
            __m256i cmp_high = _mm256_cmpeq_epi8(packedStore_high, remainderVec_high);
            std::uint64_t result_low = static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(_mm256_movemask_epi8(cmp_low)));
            std::uint64_t result_high = static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(_mm256_movemask_epi8(cmp_high)));
            std::uint64_t result = interleave_bits(result_low >> OffsetHalf, result_high >> OffsetHalf);

            // std::cout << result << std::endl;

            return result & mask;
        }

        inline std::uint64_t queryVectorized(std::uint_fast8_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            return queryVectorizedMask(remainder, (1ull << bounds.second) - (1ull << bounds.first));
        }

        // Returns a bitmask of which remainders match within the bounds. Maybe this should return not a uint64_t but a mask type? Cause we should be able to do everything with them
        inline std::uint64_t query(std::uint_fast8_t remainder, std::pair<size_t, size_t> bounds) {
            if constexpr (DEBUG) {
                assert(bounds.second <= NumRemainders);
            }
            return queryVectorized(remainder, bounds);
        }
    };

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<12, NumRemainders, Offset> : RemainderStoreTwoPieces<8, 4, NumRemainders, Offset>{};

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<16, NumRemainders, Offset> {
        inline static constexpr bool CanSplit = false;

        inline static constexpr std::size_t Size = NumRemainders * 2;
        std::array<std::uint16_t, NumRemainders> remainders;

        static_assert(Offset % 2 == 0);

        inline static constexpr size_t WordOffset = Offset/2;

        inline __m256i* getNonOffsetBucketAddress2() {
            return reinterpret_cast<__m256i*>(reinterpret_cast<std::uint16_t*>(&remainders[0]) - WordOffset);
        }

        inline std::uint_fast16_t insert(std::uint_fast16_t remainder, std::size_t loc) {
            // std::cout << "inserting " << ((std::size_t)remainder) << " " << loc << " " << remainders[0] << std::endl;
            if (loc >= NumRemainders) return remainder;
            std::uint_fast16_t retval = remainders[NumRemainders-1];
            
            std::memmove((&remainders[loc])+1, &remainders[loc], (NumRemainders - loc - 1)*2);
            remainders[loc] = remainder;

            return retval;
        }

        inline void remove(std::size_t loc) {
            // std::cout << "removing " << loc << std::endl;
            std::memmove(&remainders[loc], (&remainders[loc]) + 1, (NumRemainders - loc - 1)*2);
        }

        //Removes and returns the value that was there
        inline std::uint_fast16_t removeReturn(std::size_t loc) {
            std::uint_fast16_t retval = remainders[loc];
            remove(loc);
            return retval;
        }

        inline std::uint_fast16_t removeFirst() {
            std::uint_fast16_t first = remainders[0];
            remove(0);
            return first;
        }

        inline std::uint64_t queryNonVectorized(std::uint_fast16_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            std::uint64_t retMask = 0;
            for(size_t i{bounds.first}; i < bounds.second; i++) {
                if(remainders[i] == remainder) {
                    retMask |= 1ull << i;
                }
            }
            return retMask;
        }

        inline std::uint64_t queryVectorizedMask(std::uint_fast16_t remainder, std::uint64_t mask) {
            // std::cout << remainders[12] << " " << remainder << " " << WordOffset << std::endl;
            __m256i remainderVec = _mm256_set1_epi16(remainder);
            __m256i packedStore1 = _mm256_loadu_si256(getNonOffsetBucketAddress2());
            __m256i cmp1 = _mm256_cmpeq_epi16(remainderVec, packedStore1);
            
            // std::cout << std::bit_cast<std::uint32_t>(_mm256_movemask_epi8(cmp1)) << std::endl;
            std::uint64_t result1 = static_cast<std::uint64_t>(_pext_u32(std::bit_cast<std::uint32_t>(_mm256_movemask_epi8(cmp1)), 0x55555555u)) >> WordOffset;

            if constexpr (WordOffset + NumRemainders <= 16) {
                return result1 & mask;
            }

            __m256i packedStore2 = _mm256_loadu_si256(getNonOffsetBucketAddress2() + 1);
            __m256i cmp2 = _mm256_cmpeq_epi16(remainderVec, packedStore2);
            // Assuming offset < 32 here! Which should be true since otherwise would cross cacheline boundary if more than 32 bytes
            assert (Offset < 32);
            std::uint64_t result2 = static_cast<std::uint64_t>(_pext_u32(std::bit_cast<std::uint32_t>(_mm256_movemask_epi8(cmp2)), 0x55555555u)) << (16 - WordOffset);

            // std::cout << result1 << " " << result2 << " " << (*((uint16_t*)(&packedStore2))) << std::endl;

            return (result1 | result2) & mask;
        }

        inline std::uint64_t queryVectorized(std::uint_fast16_t remainder, std::pair<std::size_t, std::size_t> bounds) {
            return queryVectorizedMask(remainder, (1ull << bounds.second) - (1ull << bounds.first));
        }

        // Returns a bitmask of which remainders match within the bounds. Maybe this should return not a uint64_t but a mask type? Cause we should be able to do everything with them
        inline std::uint64_t query(std::uint_fast16_t remainder, std::pair<size_t, size_t> bounds) {
            if constexpr (DEBUG) {
                assert(bounds.second <= NumRemainders);
            }
            return queryVectorized(remainder, bounds);
        }
    };

    template<std::size_t NumRemainders, std::size_t Offset>
    struct alignas(1) RemainderStore<20, NumRemainders, Offset> : RemainderStoreTwoPieces<16, 4, NumRemainders, Offset>{};

    #endif
}

#endif