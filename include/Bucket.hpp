#ifndef BUCKETS_HPP
#define BUCKETS_HPP

#include <cstddef>
#include <cstdint>
#include "MiniFilter.hpp"

namespace DynamicPrefixFilter {
    //Maybe have a bit set to if the bucket is not overflowed? Cause right now the bucket may send you to the backyard even if there is nothing in the backyard, but the bucket is just full. Not that big a deal, but this slight optimization might be worth a bit?
    //Like maybe have one extra key in the minifilter and then basically account for that or smth? Not sure.
    template<std::size_t NumKeys, std::size_t NumMiniBuckets, template<std::size_t, std::size_t> typename TypeOfRemainderStoreTemplate, template<std::size_t> typename TypeOfQRContainerTemplate, std::size_t Size=64>
    struct alignas(Size) Bucket {
        using TypeOfMiniFilter = MiniFilter<NumKeys, NumMiniBuckets, TypeOfRemainderStoreTemplate>;
        TypeOfMiniFilter miniFilter;
        using TypeOfRemainderStore = TypeOfRemainderStoreTemplate<NumKeys, TypeOfMiniFilter::Size>;
        TypeOfRemainderStore remainderStore;
        using TypeOfQRContainer = TypeOfQRContainerTemplate<NumMiniBuckets>;
        
        //Returns an overflowed remainder if there was one to be sent to the backyard.
        TypeOfQRContainer insert(TypeOfQRContainer qr) {
            // if constexpr (DEBUG) {
            //     uint64_t x = countKeys();
            //     std::cout << "fafa " << x << std::endl;
            // }
            std::size_t loc = miniFilter.queryMiniBucketBeginning(qr.miniBucketIndex);
            // if constexpr (DEBUG) {
            //     uint64_t x = countKeys();
            //     std::cout << "fafa2 " << x << std::endl;
            //     std::cout << "loc: " << loc << std::endl;
            // }
            // std::size_t loc = 0;
            if(__builtin_expect(loc >= NumKeys, 0)) return qr; //Find a way to remove this if statement!!! That would shave off an entire second!
            //Can probably do it by slightly changing how the remainder store inserts, right?
            qr.miniBucketIndex = miniFilter.insert(qr.miniBucketIndex, loc);
            // if constexpr (DEBUG) {
            //     uint64_t x = countKeys();
            //     TypeOfMiniFilter::printMiniFilter(miniFilter.filterBytes);
            //     std::cout << std::endl;
            //     std::cout << "fafa3s " << x << std::endl;
            // }
            std::uint64_t overflowRemainder = remainderStore.insert(qr.remainder, loc);
            // if constexpr (DEBUG) {
            //     uint64_t x = countKeys();
            //     TypeOfMiniFilter::printMiniFilter(miniFilter.filterBytes);
            //     std::cout << std::endl;
            //     std::cout << "fafa4s " << x << std::endl;
            // }
            qr.remainder = overflowRemainder;
            return qr;
        }

        //Return 1 if found it, 2 if need to go to backyard, 0 if didn't find and don't need to go to backyard
        std::uint64_t query(TypeOfQRContainer qr) {
            // if(miniFilter.checkDefBackyard(qr.miniBucketIndex)) {
            //     return 2;
            // }
            std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
            // if(boundsMask.first == 0) return 2;
            // std::cout << "Querying: ";
            // printBinaryUInt64(boundsMask.first);
            // std::cout << ", ";
            // printBinaryUInt64(boundsMask.second);
            // std::cout << std::endl;
            // std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first, (void*)this);
            std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
            if(inFilter != 0)
                return 1;
            else if (boundsMask.second == 1ull << NumKeys) [[unlikely]] {
                return 2;
            }
            else {
                return 0;
            }
        }
        
        // std::uint64_t query(TypeOfQRContainer qr) {
        //     if constexpr (NumKeys + NumMiniBuckets <= 64 && TypeOfMiniFilter::StoreMetadata) {
        //         // std::cout << "hi"<<std::endl;
        //         // if(qr.miniBucketIndex > 0 && miniFilter.miniBucketOutofFilterBounds(qr.miniBucketIndex-1)) return 2;
        //         // else if (miniFilter.miniBucketOutofFilterBounds(qr.miniBucketIndex)) {
        //         //     std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //         //     std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
        //         //     if(inFilter != 0)
        //         //         return 1;
        //         //     else if (boundsMask.second == (1ull << NumKeys)) {
        //         //         return 2;
        //         //     }
        //         //     else {
        //         //         return 0;
        //         //     }
        //         // }
        //         // else {
        //         //     std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, (1ull << NumKeys) - 1);
        //         //     if(inFilter == 0)
        //         //         return 0;
        //         //     else if ((inFilter & (inFilter-1)) == 0) {
        //         //         return miniFilter.checkMiniBucketKeyPair(qr.miniBucketIndex, inFilter);
        //         //     }
        //         //     std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //         //     std::uint64_t boundsBitmask = boundsMask.second - boundsMask.first;
        //         //     inFilter = boundsBitmask & inFilter;
        //         //     if(inFilter != 0)
        //         //         return 1;
        //         //     else {
        //         //         return 0;
        //         //     }
        //         // }
        //         size_t bmb = miniFilter.getBiggestMiniBucket();
        //         if constexpr (remainderStore.Size == NumKeys) {
        //         if((miniFilter.full() && qr.miniBucketIndex > bmb)|| (qr.miniBucketIndex == bmb &&  qr.remainder >= remainderStore.remainders[NumKeys-1])) {
        //             return 2;
        //         }
        //         }

        //         // if(!miniFilter.full() || qr.miniBucketIndex < bmb) {
        //             // std::cout << "FSFS" << std::endl;
        //             std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, (1ull << NumKeys) - 1);
        //             if(inFilter == 0)
        //                 return 0;
        //             else if ((inFilter & (inFilter-1)) == 0) {
        //                 // std::cout <<"FFSFSFSFSFSF" << std::endl;
        //                 return miniFilter.checkMiniBucketKeyPair(qr.miniBucketIndex, inFilter);
        //             }
        //             // std::cout << "FEDCCW" << std::endl;
        //             std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //             std::uint64_t boundsBitmask = boundsMask.second - boundsMask.first;
        //             inFilter = boundsBitmask & inFilter;
        //             if(inFilter != 0)
        //                 return 1;
        //             else {
        //                 return 0;
        //             }
        //             // std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //             // std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
        //             // if(inFilter != 0)
        //             //     return 1;
        //             // // else if (boundsMask.second == (1ull << NumKeys)) {
        //             // //     return 2;
        //             // // }
        //             // else {
        //             //     return 0;
        //             // }
                
        //         // else if(qr.miniBucketIndex > bmb) return 2;
        //         // else {
        //         //     // std::cout << "csccscsc" << std::endl;
        //         //     std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //         //     std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
        //         //     if(inFilter != 0)
        //         //         return 1;
        //         //     else if (boundsMask.second == (1ull << NumKeys)) {
        //         //         return 2;
        //         //     }
        //         //     else {
        //         //         return 0;
        //         //     }
        //         // }
        //     }
        //     // std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //     // std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
        //     // if(inFilter != 0)
        //     //     return 1;
        //     // else if (boundsMask.second == (1ull << NumKeys)) {
        //     //     return 2;
        //     // }
        //     // else {ucketIndex > 0 && miniFilter.miniBucketOutofFilterBounds(qr.miniBucketIndex-1)) return 2;
        //         else {
        //             std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //             std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
        //             if(inFilter != 0)
        //                 return 1;
        //             else if (boundsMask.second == (1ull << NumKeys)) {
        //                 return 2;
        //             }
        //             else {
        //                 return 0;
        //             }
        //         }
        //     //     return 0;
        //     // }
        // }

        // std::uint64_t query(TypeOfQRContainer qr) {
        //     if constexpr (NumKeys + NumMiniBuckets <= 64) {
        //         std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, (1ull << NumKeys) - 1);
        //         if (inFilter == 0) {
        //             if(miniFilter.miniBucketOutofFilterBounds(qr.miniBucketIndex)) return 2;
        //             else return 0;
        //         }
        //         else if((inFilter & (inFilter-1)) == 0) {
        //             return miniFilter.checkMiniBucketKeyPair(qr.miniBucketIndex, inFilter);
        //         }
        //         else {
        //             std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //             std::uint64_t boundsBitmask = boundsMask.second - boundsMask.first;
        //             inFilter = boundsBitmask & inFilter;
        //             if(inFilter != 0)
        //                 return 1;
        //             else if (boundsMask.second == (1ull << NumKeys)) {
        //                 return 2;
        //             }
        //             else {
        //                 return 0;
        //             }
        //         }
        //     }
        //     else {
        //         std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
        //         std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
        //         if(inFilter != 0)
        //             return 1;
        //         else if (boundsMask.second == (1ull << NumKeys)) {
        //             return 2;
        //         }
        //         else {
        //             return 0;
        //         }
        //     }
        // }

        //Returns true if deleted, false if need to go to backyard (we assume that key exists, so we don't expect to not find it somewhere)
        bool remove(TypeOfQRContainer qr) {
            std::pair<std::uint64_t, std::uint64_t> boundsMask = miniFilter.queryMiniBucketBoundsMask(qr.miniBucketIndex);
            std::uint64_t inFilter = remainderStore.queryVectorizedMask(qr.remainder, boundsMask.second - boundsMask.first);
            if(inFilter == 0) {
                return false;
            }
            else {
                std::uint64_t locFirstMatch = __builtin_ctzll(inFilter);
                remainderStore.remove(locFirstMatch);
                miniFilter.remove(qr.miniBucketIndex, locFirstMatch);
                return true;
            }
        }

        std::uint64_t remainderStoreRemoveReturn(std::uint64_t keyIndex, std::uint64_t miniBucketIndex) {
            miniFilter.remove(miniBucketIndex, keyIndex);
            return remainderStore.removeReturn(keyIndex);
        }


        std::size_t queryWhichMiniBucket(std::size_t keyIndex) {
            return miniFilter.queryWhichMiniBucket(keyIndex);
        }

        bool full() {
            return miniFilter.full();
        }

        std::size_t countKeys() {
            return miniFilter.countKeys();
        }
    };
}

#endif