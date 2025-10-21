#ifndef MINI_FILTER_HPP
#define MINI_FILTER_HPP

#include <cstdint>
#include <cstring>
#include <array>
#include <utility>
#include <immintrin.h>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <optional>
#include <bit>
#include "TestUtility.hpp"

namespace PQF {
    //You have to put the minifilter at the beginning of the bucket! Otherwise it may mess stuff up, since it does something admittedly kinda sus
    //Relies on little endian ordering
    //Definitely not fully optimized, esp given the fact that I'm being generic and allowing any mini filter size rather than basically mini filter has to fit in 2 words (ullongs)
    //TODO: Fix the organization of this (ex make some stuff public, some stuff private etc), and make an interface (this goes for all the things written so far).
    template<std::size_t NumKeys, std::size_t NumMiniBuckets, bool Threaded=false>
    struct alignas(1) MiniFilter {
        inline static constexpr std::size_t NumBits = NumKeys+NumMiniBuckets;
        inline static constexpr std::size_t NumBytes = (NumKeys+NumMiniBuckets+7)/8;
        inline static constexpr std::size_t Size = NumBytes; // Again some naming consistency problems to address later
        inline static constexpr std::size_t NumUllongs = (NumBytes+7)/8;
        inline static constexpr std::uint64_t lastSegmentMask = (NumBits%64 == 0) ? -1ull : (1ull << (NumBits%64))-1ull;
        inline static constexpr std::uint64_t lastBitMask = (NumBits%64 == 0) ? (1ull << 63) : (1ull << ((NumBits%64)-1));
        std::array<uint8_t, NumBytes> filterBytes;

        static_assert(NumKeys < 64 && NumBytes <= 16); //Not supporting more cause I don't need it

        inline static constexpr std::size_t LockMask = 1ull << (NumBits%64);
        inline static constexpr std::size_t UnlockMask = ~LockMask;
        static_assert(!Threaded || (NumBits < NumBytes*8)); //Making sure adding the bit doesn't make the filter bigger!

        inline constexpr MiniFilter() {
            int64_t numBitsNeedToSet = NumMiniBuckets;
            for(uint8_t& b: filterBytes) {
                if(numBitsNeedToSet >= 8) {
                    b = -1;
                }
                else if(numBitsNeedToSet > 0) {
                    b = (1 << numBitsNeedToSet) - 1;
                }
                else {
                    b = 0;
                }
                numBitsNeedToSet -= 8;
            }
            
            if constexpr (DEBUG) {
                bool f = false;
                for(uint8_t b: filterBytes) {
                    if(b != 0)
                        f = true;
                }
                assert(f);
                assert(countKeys() == 0);
                checkCorrectPopCount();
            }
        }

        inline void lock() {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes) + NumUllongs-1;
            if constexpr (!Threaded) return;
            while ((__sync_fetch_and_or(fastCastFilter, LockMask) & LockMask) != 0);
        }

        inline void assertLocked() {
            if constexpr ((DEBUG || PARTIAL_DEBUG) && Threaded) {
                uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes)) + NumUllongs-1;
                assert(((*fastCastFilter) & LockMask) != 0);
            }
        }

        inline void assertUnlocked() {
            if constexpr ((DEBUG || PARTIAL_DEBUG) && Threaded) {
                uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes)) + NumUllongs-1;
                assert(((*fastCastFilter) & LockMask) == 0);
            }
        }

        inline void unlock() {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes) + NumUllongs-1;
            if constexpr (!Threaded) return;
            __sync_fetch_and_and(fastCastFilter, UnlockMask);
        }

        inline bool full() const {
            const uint64_t* fastCastFilter = reinterpret_cast<const uint64_t*> (&filterBytes);
            return *(fastCastFilter + NumUllongs - 1) & lastBitMask; //If the last element is a miniBucket separator, we know we are full! Otherwise, there are keys "waiting" to be allocated to a mini bucket.
        }

        inline std::size_t select(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) const {
            uint64_t isolateBit = _pdep_u64(1ull << miniBucketSegmentIndex, filterSegment);
            // std::cout << miniBucketSegmentIndex << " " << isolateBit << std::endl;
            return __builtin_ctzll(isolateBit);
        }

        inline std::size_t getKeyIndex(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) const {
            return select(filterSegment, miniBucketSegmentIndex) - miniBucketSegmentIndex;
        }

        inline std::size_t selectNoCtzll(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) const {
            return _pdep_u64(1ull << miniBucketSegmentIndex, filterSegment);
        }

        inline std::size_t getKeyMask(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) const {
            return selectNoCtzll(filterSegment, miniBucketSegmentIndex) >> miniBucketSegmentIndex;
        }

        inline std::pair<std::uint64_t, std::uint64_t> queryMiniBucketBoundsMask(std::size_t miniBucketIndex) {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            if constexpr (NumBytes <= 8) {
                if(miniBucketIndex == 0) {
                    return std::make_pair(1, getKeyMask(*fastCastFilter, miniBucketIndex));
                }
                else {
                    return std::make_pair(getKeyMask(*fastCastFilter, miniBucketIndex-1), getKeyMask(*fastCastFilter, miniBucketIndex));
                }
            }
            else if (NumBytes <= 16 && NumKeys < 64) { //A bit sus implementation for now but should work?
                uint64_t segmentMiniBucketCount = __builtin_popcountll(*fastCastFilter);
                if(miniBucketIndex == 0) {
                    return std::make_pair(1, getKeyMask(*fastCastFilter, miniBucketIndex));
                }
                else if (miniBucketIndex < segmentMiniBucketCount) {
                    return std::make_pair(getKeyMask(*fastCastFilter, miniBucketIndex-1), getKeyMask(*fastCastFilter, miniBucketIndex));
                }
                else if (miniBucketIndex == segmentMiniBucketCount) {
                    return std::make_pair(getKeyMask(*fastCastFilter, miniBucketIndex-1), (getKeyMask(*(fastCastFilter+1), miniBucketIndex - segmentMiniBucketCount) << (64 - segmentMiniBucketCount)));
                }
                else {
                    return std::make_pair(getKeyMask(*(fastCastFilter+1), miniBucketIndex-segmentMiniBucketCount-1) << (64 - segmentMiniBucketCount), getKeyMask(*(fastCastFilter+1), miniBucketIndex - segmentMiniBucketCount) << (64 - segmentMiniBucketCount));
                }
            }
        }

        //Returns a pair representing [start, end) of the minibucket. So basically miniBucketIndex to keyIndex conversion
        inline std::pair<std::size_t, std::size_t> queryMiniBucketBounds(std::size_t miniBucketIndex) {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            if constexpr (NumBytes <= 8) {
                if(miniBucketIndex == 0) {
                    return std::make_pair(0, getKeyIndex(*fastCastFilter, miniBucketIndex));
                }
                else {
                    return std::make_pair(getKeyIndex(*fastCastFilter, miniBucketIndex-1), getKeyIndex(*fastCastFilter, miniBucketIndex));
                }
            }
            else if (NumBytes <= 16 && NumKeys < 64) { //A bit sus implementation for now but should work?
                uint64_t segmentMiniBucketCount = __builtin_popcountll(*fastCastFilter);
                if(miniBucketIndex == 0) {
                    return std::make_pair(0, getKeyIndex(*fastCastFilter, miniBucketIndex));
                }
                else if (miniBucketIndex < segmentMiniBucketCount) {
                    return std::make_pair(getKeyIndex(*fastCastFilter, miniBucketIndex-1), getKeyIndex(*fastCastFilter, miniBucketIndex));
                }
                else if (miniBucketIndex == segmentMiniBucketCount) {
                    return std::make_pair(getKeyIndex(*fastCastFilter, miniBucketIndex-1), getKeyIndex(*(fastCastFilter+1), miniBucketIndex-segmentMiniBucketCount) + 64 - segmentMiniBucketCount);
                }
                else {
                    return std::make_pair(getKeyIndex(*(fastCastFilter+1), miniBucketIndex-segmentMiniBucketCount-1) + 64 - segmentMiniBucketCount, getKeyIndex(*(fastCastFilter+1), miniBucketIndex-segmentMiniBucketCount) + 64 - segmentMiniBucketCount);
                }
            }
        }

        //Tells you which mini bucket a key belongs to. Really works same as queryMniBucketBeginning but just does bit inverse of fastCastFilter. Returns a number larger than the number of miniBuckets if keyIndex is nonexistent (should be--test this)
        inline std::size_t queryWhichMiniBucket(std::size_t keyIndex) const {
            const uint64_t* fastCastFilter = reinterpret_cast<const uint64_t*> (&filterBytes);
            if constexpr (NumBytes <= 8) {
                return getKeyIndex(~(*fastCastFilter), keyIndex);
            }
            else if (NumBytes <= 16) {
                std::uint64_t invFilterSegment = ~(*fastCastFilter);
                std::uint64_t segmentKeyCount = __builtin_popcountll(invFilterSegment);
                if (keyIndex < segmentKeyCount) {
                    return getKeyIndex(invFilterSegment, keyIndex);
                }
                else {
                    invFilterSegment = ~(*(fastCastFilter+1));
                    return getKeyIndex(invFilterSegment, keyIndex-segmentKeyCount) + 64 - segmentKeyCount;
                }
            }
        }

        inline std::size_t queryMiniBucketBeginning(std::size_t miniBucketIndex) {
            //Highly, sus, but whatever
            if(miniBucketIndex == 0) {
                return 0;
            }
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            if constexpr (NumBytes <= 8) {
                return getKeyIndex(*fastCastFilter, miniBucketIndex-1);
            }
            else if (NumBytes <= 16) { //The code below should be expanded to this but I'm doing this to be safe. Obviously these if statements need somehow to go if we wanna optimize?
                uint64_t segmentMiniBucketCount = __builtin_popcountll(*fastCastFilter);
                if (miniBucketIndex <= segmentMiniBucketCount) {
                    return getKeyIndex(*fastCastFilter, miniBucketIndex-1);
                }
                else {
                    return getKeyIndex(*(fastCastFilter+1), miniBucketIndex-segmentMiniBucketCount-1) + 64 - segmentMiniBucketCount;
                }
            }
        }

        #ifdef __AVX512BW__

        //really just a "__m128i" version of ~((1ull << loc) - 1) or like -(1ull << loc)
        inline static constexpr __m128i getShiftMask(std::size_t loc) {
            std::array<std::uint64_t, 2> ulongs;
            for(size_t i=0; i < 2; i++, loc-=64) {
                if(loc >= 128) ulongs[i] = -1ull; //wrapped around so we want to set everything to zero
                else if (loc >= 64){
                    ulongs[i] = 0;
                }
                else {
                    ulongs[i] = -(1ull << loc);
                }
            }
            ulongs[1] &= lastSegmentMask;
            return std::bit_cast<__m128i>(ulongs);
        }

        inline static constexpr std::array<m128iWrapper, 128> getShiftMasks() {
            std::array<m128iWrapper, 128> masks;
            for(size_t i = 0; i < 128; i++) {
                masks[i] = getShiftMask(i);
            }
            return masks;
        }

        static constexpr std::array<m128iWrapper, 128> ShiftMasks = getShiftMasks();

        inline static constexpr __m128i getZeroMask(std::size_t loc) {
            std::array<std::uint64_t, 2> ulongs;
            for(size_t i=0; i < 2; i++, loc-=64) {
                if(loc >= 64) ulongs[i] = -1ull; //funny that this works even when loc "wraps around" and becomes massive cause its unsigned
                else {
                    ulongs[i] = ~(1ull << loc);
                }
            }
            return std::bit_cast<__m128i>(ulongs);
        }

        inline static constexpr std::array<m128iWrapper, 128> getZeroMasks() {
            std::array<m128iWrapper, 128> masks;
            for(size_t i = 0; i < 128; i++) {
                masks[i] = getZeroMask(i);
            }
            return masks;
        }

        static constexpr std::array<m128iWrapper, 128> ZeroMasks = getZeroMasks();

        //Vectorize as in the 4 bit remainder store? Probably not needed for if fits in 64 bits, so have a constexpr there.
        //Probably not the most efficient implementation, but this one is at least somewhatish straightforward. Still not great and maybe not even correct
        inline bool shiftFilterBits(std::size_t in) {
            int64_t index = in;
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            std::size_t endIndex = NumBits;
            uint64_t oldCarryBit = 0;
            uint64_t carryBit = 0;
            if constexpr (NumBytes > 8 && NumBytes <= 16) {
                // carryBit = *(fastCastFilter+1) && (1ull << (NumBits-64)); //Why does this single line make the code FIVE TIMES slower? From 3 secs to 15?
                __m128i* castedFilterAddress = reinterpret_cast<__m128i*>(&filterBytes);
                __m128i filterVec = _mm_loadu_si128(castedFilterAddress);
                static constexpr __m128i extractCarryBit = {0, (NumBits-65) << 56};
                carryBit = _mm_bitshuffle_epi64_mask(filterVec, extractCarryBit) >> 15;
                __m128i filterVecShiftedLeftByLong = _mm_bslli_si128(filterVec, 8);
                __m128i shiftedFilterVec = _mm_shldi_epi64(filterVec, filterVecShiftedLeftByLong, 1);
                filterVec = _mm_ternarylogic_epi32(filterVec, shiftedFilterVec, ShiftMasks[in], 0b11011000);
                filterVec = _mm_and_si128(filterVec, ZeroMasks[in]); //ensuring the new bit 
                _mm_storeu_si128(castedFilterAddress, filterVec);
            }
            else { //TODO: remove this else statement & just support it for NumBytes <= 8 with like two lines of code. Probably wouldn't be any (or at least much) faster really, but would simplify code
                for(size_t i{0}; i < NumUllongs; i++, fastCastFilter++, index-=64, endIndex-=64) {
                    std::size_t segmentStartIndex = std::max((long long)index, 0ll);
                    if(segmentStartIndex >= 64) continue;
                    uint64_t shiftBitIndex = 1ull << segmentStartIndex;
                    uint64_t shiftMask = -(shiftBitIndex);
                    uint64_t shiftedSegment = ((*fastCastFilter) & shiftMask) << 1;
                    if(endIndex < 64) {
                        shiftMask &= (1ull << endIndex) - 1;
                        carryBit = (*fastCastFilter) & (1ull << (endIndex-1));
                    }
                    else {
                        carryBit = (*fastCastFilter) & (1ull << 63);
                    }
                    carryBit = carryBit != 0;
                    *fastCastFilter = (*fastCastFilter & (~shiftMask)) | (shiftedSegment & shiftMask) | (oldCarryBit << segmentStartIndex);
                    oldCarryBit = carryBit;
                }
            }

            return carryBit;
        }

        inline void remove(std::size_t miniBucketIndex, std::size_t keyIndex) {
            std::size_t index = miniBucketIndex + keyIndex;
            if constexpr (NumBytes <= 8){
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                uint64_t shiftBitIndex = 1ull << index;
                uint64_t shiftMask = (-(shiftBitIndex)) & lastSegmentMask;
                uint64_t shiftedSegment = ((*fastCastFilter) & shiftMask) >> 1;
                *fastCastFilter = (*fastCastFilter & (~shiftMask)) | (shiftedSegment & shiftMask);
            }
            else if (NumBytes > 8 && NumBytes <= 16) {
                __m128i* castedFilterAddress = reinterpret_cast<__m128i*>(&filterBytes);
                __m128i filterVec = _mm_loadu_si128(castedFilterAddress);
                __m128i filterVecShiftedRightByLong = _mm_bsrli_si128(filterVec, 8);
                __m128i shiftedFilterVec = _mm_shrdi_epi64(filterVec, filterVecShiftedRightByLong, 1);
                filterVec = _mm_ternarylogic_epi32(filterVec, shiftedFilterVec, ShiftMasks[index], 0b11011000);
                filterVec = _mm_and_si128(filterVec, ZeroMasks[NumBits-1]); //Ensuring we are zeroing out the bit we just added, as we assume the person is removing a key, not a bucket (as that would make no sense)
                _mm_storeu_si128(castedFilterAddress, filterVec);
            }
        }

        #else

        inline bool shiftFilterBits(std::size_t in) {
            // std::cout << "shifting" << std::endl;
            int64_t index = in;
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            std::size_t endIndex = NumBits;
            uint64_t oldCarryBit = 0;
            uint64_t carryBit = 0;
            // thankfully didn't remove this loop, hehe
            for(size_t i{0}; i < NumUllongs; i++, fastCastFilter++, index-=64, endIndex-=64) {
                std::size_t segmentStartIndex = std::max((long long)index, 0ll);
                if(segmentStartIndex >= 64) continue;
                uint64_t shiftBitIndex = 1ull << segmentStartIndex;
                uint64_t shiftMask = -(shiftBitIndex);
                uint64_t shiftedSegment = ((*fastCastFilter) & shiftMask) << 1;
                if(endIndex < 64) {
                    shiftMask &= (1ull << endIndex) - 1;
                    carryBit = (*fastCastFilter) & (1ull << (endIndex-1));
                }
                else {
                    carryBit = (*fastCastFilter) & (1ull << 63);
                }
                carryBit = carryBit != 0;
                *fastCastFilter = (*fastCastFilter & (~shiftMask)) | (shiftedSegment & shiftMask) | (oldCarryBit << segmentStartIndex);
                oldCarryBit = carryBit;
            }

            return carryBit;
        }

        inline void remove(std::size_t miniBucketIndex, std::size_t keyIndex) {
            std::size_t index = miniBucketIndex + keyIndex;
            // std::cout << "removing at " << index << std::endl;
            if constexpr (NumBytes <= 8){
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                uint64_t shiftBitIndex = 1ull << index;
                uint64_t shiftMask = (-(shiftBitIndex)) & lastSegmentMask;
                uint64_t shiftedSegment = ((*fastCastFilter) & shiftMask) >> 1;
                *fastCastFilter = (*fastCastFilter & (~shiftMask)) | (shiftedSegment & shiftMask);
            }
            else if (NumBytes > 8 && NumBytes <= 16) {
                uint64_t temp[2] = {0, 0};
                // printBinaryUInt64(*(uint64_t*)(&filterBytes[0]), true);
                // printBinaryUInt64(*(uint64_t*)(&filterBytes[8]), true);
                size_t offset = (index >= 64) * 8;
                std::memcpy(temp, &filterBytes[0] + offset, NumBytes - offset);
                
                uint64_t shiftBitIndex = 1ull << (index % 64);
                uint64_t shiftMask = (-(shiftBitIndex));
                uint64_t shiftedSegment = ((temp[0] & shiftMask) >> 1) | (temp[1] << 63ull);
                temp[0] = (temp[0] & (~shiftMask)) | (shiftedSegment & shiftMask);
                temp[1] >>= 1;

                std::memcpy(&filterBytes[0] + offset, temp, NumBytes - offset);
                // printBinaryUInt64(*(uint64_t*)(&filterBytes[0]), true);
                // printBinaryUInt64(*(uint64_t*)(&filterBytes[8]), true);
            }
        }

        #endif


        //TODO: specialize it for just up to two ullongs to simplify the code (& possibly small speedup but probably not)
        //Keys are zeros, so "fix overflow" basically makes it so  that we overflow the key, not the mini bucket. We essentially aim to replace the last zero with a one
        inline uint64_t fixOverflow() {
            uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes)) + NumUllongs-1;
            uint64_t lastSegmentInverse = (~(*fastCastFilter)) & lastSegmentMask;
            uint64_t offsetMiniBuckets = NumBits-(NumUllongs-1)*64; //Again bad name. We want to return the mini bucket index, and to do that we are counting how many mini buckets from the end we have. Originally had this be popcount, but we don't need popcount, since we only continue if everything is ones basically!
            if (lastSegmentInverse != 0) {
                size_t skipMiniBuckets = _lzcnt_u64(lastSegmentInverse); //get a better name oops
                *fastCastFilter = (*fastCastFilter) | (1ull << (63-skipMiniBuckets));
                return NumMiniBuckets-(skipMiniBuckets - 64 + offsetMiniBuckets)-1;
            }
            fastCastFilter--;
            if constexpr (DEBUG)
                assert(fastCastFilter >= reinterpret_cast<uint64_t*> (&filterBytes));
            uint64_t segmentInverse = ~(*fastCastFilter); //This gives a warning in -O2 cause it may be out of bounds even though I assert it never happens!
            for(; segmentInverse == 0; offsetMiniBuckets += 64, fastCastFilter--) {
                if constexpr (DEBUG)
                    assert(fastCastFilter >= reinterpret_cast<uint64_t*> (&filterBytes));
                segmentInverse = ~(*fastCastFilter);
            }
            size_t skipMiniBuckets = _lzcnt_u64(segmentInverse);
            *fastCastFilter = (*fastCastFilter) | (1ull << (63-skipMiniBuckets));
            return NumMiniBuckets-skipMiniBuckets-offsetMiniBuckets-1;
        }
        
        //Returns true if the filter was full and had to kick somebody to make room.
        //Since we assume that keyIndex was obtained with a query or is at least valid, we have an implicit assertion that keyIndex <= NumKeys (so can essentially be the key bigger than all the other keys in the filter & it becomes the overflow)
        inline std::uint64_t insert(std::size_t miniBucketIndex, std::size_t keyIndex) {
            size_t x;
            if constexpr (DEBUG) {
                x = countKeys();
            }
            if constexpr (DEBUG) assert(keyIndex != NumBits);
            std::size_t bitIndex = miniBucketIndex + keyIndex;
            bool overflow = shiftFilterBits(bitIndex);
            if (overflow) {
                return fixOverflow();
            }
            if constexpr (DEBUG) {
                assert(countKeys() == x+1 || countKeys() == NumKeys);
            }
            return -1ull;
        }

        //Maybe remove the for loop & specialize it for <= 2 ullongs
        //We implement this by counting where the last bucket cutoff is, and then the number of keys is just that minus the number of buckets. So p similar to fixOverflow()
        inline std::size_t countKeys() {
            uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes)) + NumUllongs-1;
            uint64_t segment = (*fastCastFilter) & lastSegmentMask;
            size_t offset = (NumUllongs-1) * 64;
            for(; segment == 0; fastCastFilter--, segment = *fastCastFilter, offset -= 64);
            if constexpr (DEBUG) {
                if (!(fastCastFilter >= (reinterpret_cast<uint64_t*> (&filterBytes)))) {
                    std::cout << *(reinterpret_cast<uint64_t*> (&filterBytes)) << " " << *((reinterpret_cast<uint64_t*> (&filterBytes)) + NumUllongs-1) << " cccvc " << (NumUllongs -1) << std::endl;
                }
                assert(fastCastFilter >= (reinterpret_cast<uint64_t*> (&filterBytes)));
            }
            return (64 - _lzcnt_u64(segment)) + offset - NumMiniBuckets;
        }

        //Tells you if a mini bucket is at the very "end" of a filter. Basically, the point is to tell you if you need to go to the backyard.
        inline bool miniBucketOutofFilterBounds(std::size_t miniBucket) {
            uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes));
            if constexpr (NumBytes <= 8) {
                std::size_t previousElementsMask = ((((-1ull)<<NumKeys) << miniBucket)) & lastSegmentMask;
                return ((*fastCastFilter) & lastSegmentMask) >= previousElementsMask;
            }
            else if (NumBytes <= 16) {
                if (NumKeys + miniBucket >= 64) {
                    std::size_t previousElementsMask = (((-1ull)<<(NumKeys + miniBucket - 64))) & lastSegmentMask;
                    return ((*(fastCastFilter+1)) & lastSegmentMask) >= previousElementsMask;
                }
                else {
                    return false; //Technically wrong but we'll say that it is very unlikely. However maybe need to fix this!!
                }
            }
            return true;
        }

        inline std::size_t checkMiniBucketKeyPair(std::size_t miniBucket, std::size_t keyBit) {
            uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes));
            if constexpr (NumBytes <= 8) {
                std::size_t keyBucketLoc = keyBit << miniBucket;
                std::size_t pcnt = __builtin_popcountll((keyBucketLoc-1) & (*fastCastFilter));
                if (pcnt == miniBucket && (keyBucketLoc & (*fastCastFilter)) == 0) {
                    return 1;
                }
                else {
                    return 0;
                }
            }
            return 0; //should not get here
        }


        //Functions for testing below***************************************************************************
        inline static void printMiniFilter(std::array<uint8_t, NumBytes> filterBytes, bool withExtraBytes = false) {
            int64_t bitsLeftInFilter = withExtraBytes ? NumUllongs*64 : NumBits;
            for(std::size_t i{0}; i < NumBytes; i++) {
                uint8_t byte = filterBytes[i];
                for(int64_t j{0}; j < 8 && j < bitsLeftInFilter; j++, byte >>= 1) {
                    std::cout << (byte & 1);
                }
                bitsLeftInFilter -= 8;
            }
        }

        inline std::optional<uint64_t> testInsert(std::size_t miniBucketIndex, std::size_t keyIndex) {
            std::array<uint8_t, NumBytes> expectedFilterBytes;
            bool expectedOverflowBit = 0;
            std::optional<uint64_t> expectedOverflow = {};
            int64_t bitsNeedToSet = miniBucketIndex+keyIndex;
            int64_t bitsLeftInFilter = NumBits;
            size_t lastZeroPos = -1;
            for(std::size_t i{0}; i < NumBytes; i++) {
                uint8_t byte = filterBytes[i];
                uint8_t shiftedByte = 0;
                for(int64_t j{0}; j < 8 && j < bitsLeftInFilter; j++, byte >>= 1, bitsNeedToSet--) {
                    if(bitsNeedToSet <= 0) {
                        shiftedByte += ((uint8_t)expectedOverflowBit) << j;
                        if(!expectedOverflowBit) {
                            lastZeroPos = i*8 + j;
                        }
                        expectedOverflowBit = byte & 1;
                    }
                    else {
                        shiftedByte += (byte & 1) << j;
                        if((byte & 1) == 0) {
                            lastZeroPos = i*8 + j;
                        }
                    }
                }
                expectedFilterBytes[i] = shiftedByte;
                bitsLeftInFilter -= 8;
            }
            if(expectedOverflowBit) {
                uint8_t* byteToChange = &expectedFilterBytes[0];
                expectedOverflow = lastZeroPos - NumKeys;
                for(size_t i{1}; lastZeroPos >= 8; lastZeroPos-=8, i++) {
                    byteToChange = &expectedFilterBytes[i];
                }
                *byteToChange |= 1 << lastZeroPos;
            }
            std::uint64_t overflow = insert(miniBucketIndex, keyIndex);
            assert(expectedFilterBytes == filterBytes);
            assert((overflow != -1ull) == expectedOverflow.has_value());
            if((overflow != -1ull)) {
                assert(overflow == *expectedOverflow);
            }
            return overflow;
        }

        void checkCorrectPopCount() {
            uint64_t totalPopcount = 0;
            for(uint8_t byte: filterBytes) {
                totalPopcount += __builtin_popcountll(byte);
            }
            assert(totalPopcount == NumMiniBuckets);
        }
    };
}

#endif