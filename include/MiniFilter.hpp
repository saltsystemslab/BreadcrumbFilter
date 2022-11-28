#ifndef MINI_FILTER_HPP
#define MINI_FILTER_HPP

#include <cstdint>
#include <array>
#include <utility>
#include <immintrin.h>
#include <algorithm>
#include <iostream>
#include <cassert>
#include <optional>
#include <bit>
#include "TestUtility.hpp"

//Once get this working, turn this into more of an interface by not having this actually store the data?
//Rather you pass pointers into the class, and it is a temporary class just to make dealing with stuff convenient
//Cause having the class in charge of its own data seems somewhat inneficient cause you can't avoid initialization nonsense
//But also it feels like the Bucket should be in charge of data handling, since it needs to know the sizes of stuff anyways to fit in a cache line
//Can just have some constexpr static function in this class that tells you the size you need or smth
//This would also enable the dynamicprefixfilter to have a resolution of individual bits rather than bytes in terms of space usage, although that's less of a concern


//miniBucket separators are 1s and keys are 0s
//Todo: add offset here so that can put this at the end rather than the beginning of the filter. That actually seems to be the *better* memory configuration!
namespace DynamicPrefixFilter {
    //You have to put the minifilter at the beginning of the bucket! Otherwise it may mess stuff up, since it does something admittedly kinda sus
    //Relies on little endian ordering
    //Definitely not fully optimized, esp given the fact that I'm being generic and allowing any mini filter size rather than basically mini filter has to fit in 2 words (ullongs)
    //TODO: Fix the organization of this (ex make some stuff public, some stuff private etc), and make an interface (this goes for all the things written so far).
    template<std::size_t NumKeys, std::size_t NumMiniBuckets, template<std::size_t, std::size_t> typename TypeOfRemainderStoreTemplate> //store metadata says whether to store the largest bucket in the filter and whether the filter is full in the first few bits of the filter to optimize queries
    struct alignas(1) MiniFilter {
        static constexpr std::size_t clog(std::size_t x) {
            if(x == 0) return -1ull;
            for (size_t i = 1; i <= 64; i++) {
                x /= 2;
                if (x == 0) return i;
            }
        }


        static constexpr bool FallOffInserts = false;
        static constexpr std::size_t WouldBeFrontOffset = clog(NumKeys) + 1;
        // static constexpr bool StoreMetadata = false;
        static constexpr bool StoreMetadata = (NumKeys + NumMiniBuckets <= 64) && ((TypeOfRemainderStoreTemplate<NumKeys, 0>::Size*8 + (NumKeys+NumMiniBuckets+WouldBeFrontOffset)) <= 32*8); //Need to configure later but for now this is all you get. Assumes 32 byte buckets, single word minifilter, and checks if can fit this extra data
        static_assert(StoreMetadata);
        static constexpr bool StoreLockBit = false;
        // static constexpr bool StoreLockBit = !StoreMetadata; //Add lock bit; storemetadata adds by default
        //Needs to store the largest bucket currently in the filter, so clog gives the number of bits needed for that. Extra bit is unecessary but due to way I designed the filter should? help with query times by removing the edge case of the minibucket being zero when queries by basically padding with an extra bit to never worry about that case. Will later be actually useful though, when we do locking, in which case we will just atomically set that bit & it thus serves a dual purpose!
        static constexpr std::size_t FrontOffset = StoreMetadata ? WouldBeFrontOffset: (StoreLockBit? 1 : 0); //Gonna assume this fits in a byte cause we generally assume <= 64 size buckets anyways. Makes some things easier to do that
        static constexpr std::size_t FrontOffsetMinusOne = FrontOffset == 0 ? 0 : FrontOffset - 1;
        static constexpr std::size_t BiggestMiniBucketMask = (1ull << FrontOffsetMinusOne) - 1;
        // static_assert(FrontOffset == 0);
        static_assert((!StoreMetadata) || FrontOffset == 6);

        static constexpr std::size_t NumBits = NumKeys+NumMiniBuckets+FrontOffset;
        static constexpr std::size_t NumBytes = (NumBits+7)/8;
        static_assert((!StoreLockBit) || NumBytes <= 8); //Very temporary
        static_assert((!StoreMetadata) || NumBytes <= 8); //For now
        static constexpr std::size_t Size = NumBytes; // Again some naming consistency problems to address later
        static constexpr std::size_t NumUllongs = (NumBytes+7)/8;

        static constexpr std::uint64_t firstSegmentMask = -1ull - ((FrontOffset == 0) ? 0 : ((1ull << FrontOffset) - 1));
        static_assert((!StoreMetadata) || firstSegmentMask == ~63);
        static constexpr std::uint64_t lastSegmentMask = (NumBits%64 == 0) ? -1ull : (1ull << (NumBits%64))-1ull;
        static constexpr std::uint64_t lastSegmentMaskWOLastBit = (NumBits%64 == 0) ? (-1ull >> 1) : (1ull << ((NumBits%64) -1))-1ull;
        static constexpr std::uint64_t combinedSegmentMask = firstSegmentMask & lastSegmentMask; //For case when first is last (metadata fits in a long)
        static constexpr std::uint64_t combinedSegmentMaskWOLastBit = firstSegmentMask & lastSegmentMaskWOLastBit; //For case when first is last (metadata fits in a long)
        static constexpr std::uint64_t lastBitMask = (NumBits%64 == 0) ? (1ull << 63) : (1ull << ((NumBits%64)-1));
        std::array<uint8_t, NumBytes> filterBytes;

        static_assert(NumKeys < 64 && NumBytes <= 16); //Not supporting more cause I don't need it

        constexpr MiniFilter() {
            // std::cout << StoreMetadata << std::endl;
            if constexpr (StoreMetadata || StoreLockBit) {
                //Gonna assume FrontOffset + NumKeys >= 8, cause seems ridiculous to not have that
                filterBytes[0] = static_cast<uint8_t>(- (1ull << (FrontOffset-1)));
                // std::cout << "Filterbytes[0] " << (uint32_t)filterBytes[0] << " " << (uint32_t) ((filterBytes[0] & combinedSegmentMask)) << std::endl;
                // assert(filterBytes[0] == 224 && ((filterBytes[0] & combinedSegmentMask) == 192));
                //If want locking support, this bit should be set to zero initially instead, so that when we lock it its 1 and the mini filter is happy.

                int64_t numBitsNeedToSet = NumMiniBuckets - (8 - FrontOffset);
                for(std::size_t i = 1; i < NumBytes; i++) {
                    if(numBitsNeedToSet >= 8) {
                        filterBytes[i] = -1;
                    }
                    else if(numBitsNeedToSet > 0) {
                        filterBytes[i] = (1 << numBitsNeedToSet) - 1;
                    }
                    else {
                        filterBytes[i] = 0;
                    }
                    numBitsNeedToSet -= 8;
                }
                
            }
            else {
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

        bool checkDefBackyard(size_t miniBucketIndex) {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            return full() && (miniBucketIndex >= ((*fastCastFilter) & BiggestMiniBucketMask));
        }

        size_t getBiggestMiniBucket() {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            return (*fastCastFilter) & BiggestMiniBucketMask;
        }

        bool full() {
            if constexpr (NumBytes <= 8) {
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                return *fastCastFilter & lastBitMask;
            }
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            return *(fastCastFilter + NumUllongs - 1) & lastBitMask; //If the last element is a miniBucket separator, we know we are full! Otherwise, there are keys "waiting" to be allocated to a mini bucket.
        }

        std::size_t select(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) {
            uint64_t isolateBit = _pdep_u64(1ull << miniBucketSegmentIndex, filterSegment);
            if constexpr (FallOffInserts) { //Cause the isolate bit may be zero, in which case to maintain compatiblity we need to add the bit at the end
                isolateBit |= 1ull << (NumBits - 1);
            }
            // std::cout << miniBucketSegmentIndex << " " << isolateBit << std::endl;
            return __builtin_ctzll(isolateBit);
        }

        std::size_t getKeyIndex(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) {
            return select(filterSegment, miniBucketSegmentIndex) - miniBucketSegmentIndex;
        }

        std::size_t selectNoCtzll(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) {
            return _pdep_u64(1ull << miniBucketSegmentIndex, filterSegment);
        }

        std::size_t getKeyMask(uint64_t filterSegment, uint64_t miniBucketSegmentIndex) {
            // if constexpr (StoreMetadata && NumBytes <= 8) {
            //     return selectNoCtzll(filterSegment, miniBucketSegmentIndex) >> (miniBucketSegmentIndex + FrontOffsetMinusOne);
            // }
            return selectNoCtzll(filterSegment, miniBucketSegmentIndex) >> miniBucketSegmentIndex;
        }

        std::pair<std::uint64_t, std::uint64_t> queryMiniBucketBoundsMask(std::size_t miniBucketIndex) { //maybe try to fuse into just one pdep? Not convinced about that but idk maybe?
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);

            if constexpr (StoreMetadata && NumBytes <= 8) {
                // std::uint64_t shiftedFilter = (*fastCastFilter) >> (FrontOffsetMinusOne);
                // // std::uint64_t shiftedFilter = (*fastCastFilter) & (~BiggestMiniBucketMask);
                // return std::make_pair(getKeyMask(shiftedFilter, miniBucketIndex), getKeyMask(shiftedFilter, miniBucketIndex+1));
                std::uint64_t shiftedFilter = (*fastCastFilter) >> (FrontOffset);
                if(miniBucketIndex == 0) {
                    return std::make_pair(1, getKeyMask(shiftedFilter, miniBucketIndex));
                }
                else {
                    return std::make_pair(getKeyMask(shiftedFilter, miniBucketIndex-1), getKeyMask(shiftedFilter, miniBucketIndex));
                }
            }

            if constexpr (StoreLockBit && NumBytes <= 8) {
                std::uint64_t shiftedFilter = *fastCastFilter;
                return std::make_pair(getKeyMask(shiftedFilter, miniBucketIndex), getKeyMask(shiftedFilter, miniBucketIndex+1));
            }

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

        // std::uint64_t queryMiniBucketBoundsMaskS(std::size_t miniBucketIndex) { //maybe try to fuse into just one pdep? Not convinced about that but idk maybe?
        //     uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);

        //     if constexpr (StoreMetadata && NumBytes <= 8) {
        //         std::uint64_t shiftedFilter = (*fastCastFilter) >> (FrontOffsetMinusOne);
        //         // std::uint64_t shiftedFilter = (*fastCastFilter) & (~BiggestMiniBucketMask);
        //         return getKeyMask(shiftedFilter, miniBucketIndex+1) - getKeyMask(shiftedFilter, miniBucketIndex);
        //     }

        //     if constexpr (StoreLockBit && NumBytes <= 8) {
        //         std::uint64_t shiftedFilter = *fastCastFilter;
        //         return getKeyMask(shiftedFilter, miniBucketIndex+1)-getKeyMask(shiftedFilter, miniBucketIndex);
        //     }

        //     if constexpr (NumBytes <= 8) {
        //         if(miniBucketIndex == 0) {
        //             return getKeyMask(*fastCastFilter, miniBucketIndex)-1;
        //         }
        //         else {
        //             return getKeyMask(*fastCastFilter, miniBucketIndex)-getKeyMask(*fastCastFilter, miniBucketIndex-1);
        //         }
        //     }
        //     else if (NumBytes <= 16 && NumKeys < 64) { //A bit sus implementation for now but should work?
        //         uint64_t segmentMiniBucketCount = __builtin_popcountll(*fastCastFilter);
        //         if(miniBucketIndex == 0) {
        //             return getKeyMask(*fastCastFilter, miniBucketIndex)-1;
        //         }
        //         else if (miniBucketIndex < segmentMiniBucketCount) {
        //             return getKeyMask(*fastCastFilter, miniBucketIndex)-getKeyMask(*fastCastFilter, miniBucketIndex-1);
        //         }
        //         else if (miniBucketIndex == segmentMiniBucketCount) {
        //             return (getKeyMask(*(fastCastFilter+1), miniBucketIndex - segmentMiniBucketCount) << (64 - segmentMiniBucketCount))-getKeyMask(*fastCastFilter, miniBucketIndex-1);
        //         }
        //         else {
        //             return (getKeyMask(*(fastCastFilter+1), miniBucketIndex - segmentMiniBucketCount) << (64 - segmentMiniBucketCount))-(getKeyMask(*(fastCastFilter+1), miniBucketIndex-segmentMiniBucketCount-1) << (64 - segmentMiniBucketCount));
        //         }
        //     }
        // }

        //Returns a pair representing [start, end) of the minibucket. So basically miniBucketIndex to keyIndex conversion
        std::pair<std::size_t, std::size_t> queryMiniBucketBounds(std::size_t miniBucketIndex) {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);

            if constexpr (StoreMetadata && NumBytes <= 8) {
                std::uint64_t shiftedFilter = (*fastCastFilter) >> FrontOffsetMinusOne;
                return std::make_pair(getKeyIndex(shiftedFilter, miniBucketIndex), getKeyIndex(shiftedFilter, miniBucketIndex+1));
            }

            if constexpr (StoreLockBit && NumBytes <= 8) {
                return std::make_pair(getKeyIndex(*fastCastFilter, miniBucketIndex), getKeyIndex(*fastCastFilter, miniBucketIndex+1));
            }

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
        std::size_t queryWhichMiniBucket(std::size_t keyIndex) {
            uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);

            if constexpr (StoreMetadata) {
                std::uint64_t invFilterSegment = ~(*fastCastFilter);
                // std::uint64_t segmentKeyCount = __builtin_popcountll(invFilterSegment);
                if constexpr (NumBytes <= 8) {
                    return getKeyIndex(invFilterSegment >> FrontOffset, keyIndex);
                }
            }

            if constexpr (StoreLockBit && NumBytes <= 8) {
                std::uint64_t invFilterSegment = ~(*fastCastFilter);
                return getKeyIndex(invFilterSegment >> FrontOffset, keyIndex);
            }

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
                    // std::cout << "Hello" << std::endl;
                    return getKeyIndex(invFilterSegment, keyIndex-segmentKeyCount) + 64 - segmentKeyCount;
                }
            }
        }

        std::size_t queryMiniBucketBeginning(std::size_t miniBucketIndex) {
            //Highly, sus, but whatever
            if constexpr (StoreMetadata) {
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                //For now. Will expand later?
                if constexpr (NumBytes <= 8) {
                    return getKeyIndex((*fastCastFilter) >> FrontOffsetMinusOne, miniBucketIndex);
                }
            }
            if constexpr (StoreLockBit && NumBytes <= 8) {
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                return getKeyIndex(*fastCastFilter, miniBucketIndex);
            }
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

        //really just a "__m128i" version of ~((1ull << loc) - 1) or like -(1ull << loc)
        static constexpr __m128i getShiftMask(std::size_t loc) {
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

        static constexpr std::array<m128iWrapper, 128> getShiftMasks() {
            std::array<m128iWrapper, 128> masks;
            for(size_t i = 0; i < 128; i++) {
                masks[i] = getShiftMask(i);
            }
            return masks;
        }

        static constexpr std::array<m128iWrapper, 128> ShiftMasks = getShiftMasks();

        static constexpr __m128i getZeroMask(std::size_t loc) {
            std::array<std::uint64_t, 2> ulongs;
            for(size_t i=0; i < 2; i++, loc-=64) {
                if(loc >= 64) ulongs[i] = -1ull; //funny that this works even when loc "wraps around" and becomes massive cause its unsigned
                else {
                    ulongs[i] = ~(1ull << loc);
                }
            }
            return std::bit_cast<__m128i>(ulongs);
        }

        static constexpr std::array<m128iWrapper, 128> getZeroMasks() {
            std::array<m128iWrapper, 128> masks;
            for(size_t i = 0; i < 128; i++) {
                masks[i] = getZeroMask(i);
            }
            return masks;
        }

        static constexpr std::array<m128iWrapper, 128> ZeroMasks = getZeroMasks();

        //Vectorize as in the 4 bit remainder store? Probably not needed for if fits in 64 bits, so have a constexpr there.
        //Probably not the most efficient implementation, but this one is at least somewhatish straightforward. Still not great and maybe not even correct
        bool shiftFilterBits(std::size_t in) {
            if constexpr (NumBytes <= 8) {
                //Point of this is to drastically simplify this, to not have the overkill AVX512 instructions? Although then again idk if that's even gonna help. But yeah the point is we shift, and do NOT pay attention to the overflow. Just that beforehand we check if the filter is full, which is equivalent. Just simplifies coding for this special case.
                bool startedFull = full(); //means overflow
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                // std::cout << fastCastFilter << std::endl;
                uint64_t shiftmask = (~((1ull << in) - 1));
                uint64_t shiftmaskWOBit = shiftmask & combinedSegmentMaskWOLastBit;
                uint64_t shiftmaskWBit = shiftmask & combinedSegmentMask;
                *fastCastFilter = (((*fastCastFilter) & shiftmaskWOBit) << 1) | ((*fastCastFilter) & (~shiftmaskWBit));
                return startedFull;
            }

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

        //TODO: specialize it for just up to two ullongs to simplify the code (& possibly small speedup but probably not)
        //Keys are zeros, so "fix overflow" basically makes it so  that we overflow the key, not the mini bucket. We essentially aim to replace the last zero with a one
        uint64_t fixOverflow() {
            if constexpr (NumBytes <=8 && StoreMetadata) {
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                uint64_t biggestMiniBucket = (*fastCastFilter) & BiggestMiniBucketMask;
                *fastCastFilter = (*fastCastFilter) | (1ull << (biggestMiniBucket + NumKeys + FrontOffset));
                return biggestMiniBucket;
            }
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

        void updateLargestMiniBucket() {
            if constexpr (StoreMetadata) { //fix this
                if (full()){
                    uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                    uint64_t lastSegmentInverse = (~(*fastCastFilter)) & combinedSegmentMask;
                    size_t lastKeyReverseIndex = _lzcnt_u64(lastSegmentInverse); //Everything after this is a minibucket
                    //NumMiniBuckets - 1 - ((lastKeyReverseIndex-1) - (64-NumBits)) // cause NumBits counts up to the end, we want how many bits from the end to subtract off
                    size_t lastKeyMiniBucket = (NumMiniBuckets + 64 - NumBits) - lastKeyReverseIndex;
                    // std::cout << "lastKeyminibucket " << lastKeyMiniBucket << " " << lastKeyReverseIndex << " " << std::endl;
                    *fastCastFilter = ((*fastCastFilter) & (~BiggestMiniBucketMask)) | (lastKeyMiniBucket);
                    // uint64_t offsetMiniBuckets = NumBits; //Probably fix this line? Not sure. Honestly probably rewrite this.
                    //Also either make it only run when its full (probably best) or use countkeys cause you kinda have to know how many keys there are in order to do the offsets
                    // uint64_t biggestMiniBucket = NumMiniBuckets-(skipMiniBuckets - 64 + offsetMiniBuckets)-1;
                    // *fastCastFilter = ((*fastCastFilter) & (~BiggestMiniBucketMask)) | (biggestMiniBucket);
                }
            }
        }
        
        //Returns true if the filter was full and had to kick somebody to make room.
        //Since we assume that keyIndex was obtained with a query or is at least valid, we have an implicit assertion that keyIndex <= NumKeys (so can essentially be the key bigger than all the other keys in the filter & it becomes the overflow)
        std::uint64_t insert(std::size_t miniBucketIndex, std::size_t keyIndex) {
            size_t x;
            if constexpr (DEBUG) {
                x = countKeys();
            }
            // if constexpr (DEBUG) assert(keyIndex != NumBits);
            // if(keyIndex == NumKeys) return NumKeys;
            // std::size_t bitIndex = miniBucketIndex + keyIndex;
            std::size_t bitIndex = miniBucketIndex + keyIndex + FrontOffset;

            // if constexpr (NumBytes <= 8 && FallOffInserts) {
            //     // std::cout << bitIndex << " " << miniBucketIndex << " " << keyIndex << " " << FrontOffset << std::endl;
            //     assert(bitIndex < NumBits);
            //     static_assert(StoreLockBit); //For now
            //     // bool startedFull = full(); //means overflow
            //     uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
            //     // std::cout << fastCastFilter << std::endl;
            //     uint64_t shiftmask = (~((1ull << bitIndex) - 1));
            //     uint64_t shiftmaskWOBit = shiftmask & combinedSegmentMaskWOLastBit;
            //     uint64_t shiftmaskWBit = shiftmask & combinedSegmentMask;
            //     *fastCastFilter = (((*fastCastFilter) & shiftmaskWOBit) << 1) | ((*fastCastFilter) & (~shiftmaskWBit));
            //     if (startedFull) {
            //         std::uint64_t NumBucketsLeft = __builtin_popcountll((*fastCastFilter) & combinedSegmentMask); //This is actually the minibucket that we are in!
            //         // if (NumBucketsLeft < NumMiniBuckets)
            //         return NumBucketsLeft - 1; // To account for the one bit of storelockbit
            //     }
            //     return -1ull;
            //     // return startedFull;
            // }

            if constexpr (DEBUG) assert(bitIndex < NumBits);
            if constexpr (StoreMetadata) {
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                if(full() && miniBucketIndex >= ((*fastCastFilter) & BiggestMiniBucketMask))
                    return miniBucketIndex;
            }
            bool overflow = shiftFilterBits(bitIndex);
            if (overflow) {
                if constexpr (StoreMetadata) {
                    uint64_t miniBucket = fixOverflow();
                    updateLargestMiniBucket();
                    return miniBucket;
                }
                return fixOverflow();
            }
            
            updateLargestMiniBucket();

            if constexpr (DEBUG) {
                if(!(countKeys() == x+1 || countKeys() == NumKeys))
                    std::cout << x << " " << countKeys() << " " << bitIndex << std::endl;
                assert(countKeys() == x+1 || countKeys() == NumKeys);
            }
            return -1ull;
        }

        void remove(std::size_t miniBucketIndex, std::size_t keyIndex) {
            std::size_t index = miniBucketIndex + keyIndex + FrontOffset;
            if constexpr (NumBytes <= 8){
                uint64_t* fastCastFilter = reinterpret_cast<uint64_t*> (&filterBytes);
                uint64_t shiftBitIndex = 1ull << index;
                uint64_t shiftMask = (-(shiftBitIndex)) & lastSegmentMask;
                uint64_t shiftedSegment = ((*fastCastFilter) & shiftMask) >> 1;
                *fastCastFilter = (*fastCastFilter & (~shiftMask)) | (shiftedSegment & shiftMask);
                if constexpr (FallOffInserts) {
                    *fastCastFilter |= 1ull << (NumBits - 1); //I think this works?
                }
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

        //Maybe remove the for loop & specialize it for <= 2 ullongs
        //We implement this by counting where the last bucket cutoff is, and then the number of keys is just that minus the number of buckets. So p similar to fixOverflow()
        std::size_t countKeys() {
            if constexpr (NumBytes <= 8 && StoreMetadata) {
                uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes));
                uint64_t segment = (*fastCastFilter) & combinedSegmentMask;
                return (64 - _lzcnt_u64(segment)) - NumMiniBuckets - FrontOffset;
            }
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
        bool miniBucketOutofFilterBounds(std::size_t miniBucket) {
            uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes));
            // static_assert(StoreMetadata);
            if constexpr (NumBytes <= 8) {
                if constexpr (StoreMetadata) {
                    return miniBucket >= ((*fastCastFilter) & BiggestMiniBucketMask);
                }
                // std::size_t previousElementsMask = (~(((1ull<<NumKeys) << miniBucket) - 1)) & lastSegmentMask; //Basically, we want to see if there is a zero (meaning a key) after where the bucket should be if it is after all the keys
                // return ((*fastCastFilter) & lastSegmentMask) >= previousElementsMask; //Works since each miniBucket is a one, so we test if t
                std::size_t previousElementsMask = ((((-1ull)<<NumKeys) << miniBucket)) & lastSegmentMask;
                return ((*fastCastFilter) & lastSegmentMask) >= previousElementsMask;

                // 01000111. 00000100 -> 00000111 -> 01000111 >= 00000111
            }
        }

        std::size_t checkMiniBucketKeyPair(std::size_t miniBucket, std::size_t keyBit) {
            uint64_t* fastCastFilter = (reinterpret_cast<uint64_t*> (&filterBytes));
            if constexpr (NumBytes <= 8) {
                std::size_t keyBucketLoc = keyBit << miniBucket;
                std::size_t shiftedFilter = (*fastCastFilter) >> FrontOffset;
                std::size_t pcnt = __builtin_popcountll((keyBucketLoc-1) & shiftedFilter);
                if (pcnt == miniBucket && (keyBucketLoc & shiftedFilter) == 0) {
                    return 1;
                }
                // else if (miniBucketOutofFilterBounds(miniBucket)){
                //     return 2;
                // }
                else {
                    return 0;
                }
                // else return 0;
            }
        }


        //Functions for testing below***************************************************************************
        static void printMiniFilter(std::array<uint8_t, NumBytes> filterBytes, bool withExtraBytes = false) {
            int64_t bitsLeftInFilter = withExtraBytes ? NumUllongs*64 : NumBits;
            for(std::size_t i{0}; i < NumBytes; i++) {
                uint8_t byte = filterBytes[i];
                for(int64_t j{0}; j < 8 && j < bitsLeftInFilter; j++, byte >>= 1) {
                    std::cout << (byte & 1);
                }
                bitsLeftInFilter -= 8;
            }
        }

        std::optional<uint64_t> testInsert(std::size_t miniBucketIndex, std::size_t keyIndex) {
            // std::cout << "Trying to insert " << miniBucketIndex << " " << keyIndex << std::endl;
            // printMiniFilter(filterBytes, true);
            // std::cout << std::endl;
            std::array<uint8_t, NumBytes> expectedFilterBytes;
            bool expectedOverflowBit = 0;
            std::optional<uint64_t> expectedOverflow = {};
            // bool startedShifting = false;
            int64_t bitsNeedToSet = miniBucketIndex+keyIndex+FrontOffset;
            int64_t bitsLeftInFilter = std::min(countKeys() + NumMiniBuckets+1 + FrontOffset, NumBits);
            size_t lastZeroPos = -1;
            size_t secondLastZeroPos = -1;
            for(std::size_t i{0}; i < NumBytes; i++) {
                uint8_t byte = filterBytes[i];
                uint8_t shiftedByte = 0;
                for(int64_t j{0}; j < 8 && j < bitsLeftInFilter; j++, byte >>= 1, bitsNeedToSet--) {
                    if(bitsNeedToSet <= 0) {
                        shiftedByte += ((uint8_t)expectedOverflowBit) << j;
                        if(!expectedOverflowBit) {
                            secondLastZeroPos = lastZeroPos;
                            lastZeroPos = i*8 + j;
                        }
                        expectedOverflowBit = byte & 1;
                    }
                    else {
                        shiftedByte += (byte & 1) << j;
                        if((byte & 1) == 0) {
                            secondLastZeroPos = lastZeroPos;
                            lastZeroPos = i*8 + j;
                        }
                    }
                }
                expectedFilterBytes[i] = shiftedByte;
                bitsLeftInFilter -= 8;
            }
            if(expectedOverflowBit) {
                uint8_t* byteToChange = &expectedFilterBytes[0];
                expectedOverflow = lastZeroPos - NumKeys - FrontOffset;
                for(size_t i{1}; lastZeroPos >= 8; lastZeroPos-=8, i++) {
                    byteToChange = &expectedFilterBytes[i];
                }
                *byteToChange |= 1 << lastZeroPos;
                expectedFilterBytes[0] = (expectedFilterBytes[0] & (~BiggestMiniBucketMask)) + secondLastZeroPos - NumKeys - FrontOffset + 1; //fix this checking func or just ignore it entirely?
            }
            else if (countKeys() == NumKeys - 1) {
                expectedFilterBytes[0] = (expectedFilterBytes[0] & (~BiggestMiniBucketMask)) + lastZeroPos - countKeys() - FrontOffset;
                // std::cout << (int)(expectedFilterBytes[0] & BiggestMiniBucketMask) << " " << lastZeroPos << " " << countKeys() << " " << FrontOffset << std::endl;
            }
            std::uint64_t overflow = insert(miniBucketIndex, keyIndex);
            // std::cout << (int)(filterBytes[0] & BiggestMiniBucketMask) <<" " << (int)(expectedFilterBytes[0] & BiggestMiniBucketMask) << " " << lastZeroPos << " " << countKeys() << " " << FrontOffset << std::endl;
            // std::cout << std::endl;
            // printMiniFilter(filterBytes, true);
            // std::cout << std::endl;
            // std::cout << std::endl;
            // printMiniFilter(expectedFilterBytes, true);
            // std::cout << std::endl;
            // std::cout << overflow << " " << *expectedOverflow << std::endl;
            assert(expectedFilterBytes == filterBytes);
            assert((overflow != -1ull) == expectedOverflow.has_value());
            if((overflow != -1ull)) {
                // std::cout << (*overflow) << " " << (*expectedOverflow) << std::endl;
                assert(overflow == *expectedOverflow);
            }
            return overflow;
        }

        void checkCorrectPopCount() {
            uint64_t totalPopcount = 0;
            for(uint8_t byte: filterBytes) {
                totalPopcount += __builtin_popcountll(byte);
            }
            if constexpr (StoreMetadata) {
                totalPopcount -= __builtin_popcountll(filterBytes[0] & (~firstSegmentMask));
            }
            assert(totalPopcount == NumMiniBuckets);
        }
    };
}

#endif