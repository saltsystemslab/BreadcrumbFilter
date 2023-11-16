#include "DynamicPrefixFilter.hpp"
#include <optional>
#include <iostream>
#include <map>
#include <utility>
#include <execution>

using namespace DynamicPrefixFilter;

// template class PartitionQuotientFilter<46, 51, 16, 8, 64, 32>; //These fail for obvious reasons, but I wanted to try anyways
// template class PartitionQuotientFilter<46, 51, 16, 7, 64, 32>;
template class PartitionQuotientFilter<8, 46, 51, 35, 8, 64, 64>;
template class PartitionQuotientFilter<8, 46, 51, 35, 6, 64, 64>;
template class PartitionQuotientFilter<8, 46, 51, 35, 4, 64, 64>;
template class PartitionQuotientFilter<8, 48, 51, 35, 8, 64, 64>;
template class PartitionQuotientFilter<8, 49, 51, 35, 8, 64, 64>;
template class PartitionQuotientFilter<8, 51, 51, 35, 8, 64, 64>;
template class PartitionQuotientFilter<8, 51, 51, 35, 6, 64, 64>;
template class PartitionQuotientFilter<8, 52, 51, 35, 8, 64, 64>;
template class PartitionQuotientFilter<8, 52, 51, 35, 8, 64, 64, true>;
template class PartitionQuotientFilter<8, 52, 51, 35, 8, 64, 64, false, true>;
template class PartitionQuotientFilter<8, 52, 51, 35, 8, 64, 64, true, true>;
template class PartitionQuotientFilter<8, 53, 51, 35, 8, 64, 64>;
template class PartitionQuotientFilter<8, 53, 51, 35, 8, 64, 64, true>;
template class PartitionQuotientFilter<8, 62, 50, 34, 8, 64, 64>;
template class PartitionQuotientFilter<8, 62, 50, 34, 8, 64, 64, true>;
// template class PartitionQuotientFilter<8, 61, 50, 34, 8, 64, 64, false, true>;
// template class PartitionQuotientFilter<8, 61, 50, 34, 8, 64, 64, true, true>;
template class PartitionQuotientFilter<8, 22, 26, 18, 8, 32, 32>;
template class PartitionQuotientFilter<8, 22, 26, 18, 8, 32, 32, true>;
template class PartitionQuotientFilter<8, 22, 26, 37, 8, 32, 64>;
template class PartitionQuotientFilter<8, 22, 26, 37, 8, 32, 64, true>;
template class PartitionQuotientFilter<8, 21, 26, 18, 8, 32, 32, false, true>;
template class PartitionQuotientFilter<8, 21, 26, 18, 8, 32, 32, true, true>;
template class PartitionQuotientFilter<8, 22, 25, 17, 8, 32, 32, false, true>;
template class PartitionQuotientFilter<8, 22, 25, 17, 8, 32, 32, true, true>;
template class PartitionQuotientFilter<8, 31, 25, 17, 8, 32, 32>;
template class PartitionQuotientFilter<8, 31, 25, 17, 8, 32, 32, true>;
template class PartitionQuotientFilter<8, 30, 25, 17, 8, 32, 32, false, true>;
template class PartitionQuotientFilter<8, 30, 25, 17, 8, 32, 32, true, true>;
template class PartitionQuotientFilter<8, 22, 25, 17, 6, 32, 32>;
template class PartitionQuotientFilter<8, 22, 25, 17, 4, 32, 32>;
template class PartitionQuotientFilter<8, 25, 25, 17, 8, 32, 32>;
template class PartitionQuotientFilter<8, 25, 25, 17, 6, 32, 32>;
template class PartitionQuotientFilter<8, 25, 25, 17, 4, 32, 32>;
template class PartitionQuotientFilter<8, 25, 25, 35, 8, 32, 64>;
template class PartitionQuotientFilter<8, 25, 25, 16, 8, 32, 32>;
template class PartitionQuotientFilter<8, 23, 25, 17, 8, 32, 32>;
template class PartitionQuotientFilter<8, 23, 25, 17, 6, 32, 32>;
template class PartitionQuotientFilter<8, 23, 25, 17, 4, 32, 32>;
template class PartitionQuotientFilter<8, 24, 25, 17, 8, 32, 32>;
template class PartitionQuotientFilter<8, 24, 25, 17, 6, 32, 32>;
template class PartitionQuotientFilter<16, 36, 28, 22, 8, 64, 64>;
template class PartitionQuotientFilter<16, 36, 28, 22, 8, 64, 64, true>;
template class PartitionQuotientFilter<16, 35, 28, 22, 8, 64, 64, false, true>;
template class PartitionQuotientFilter<16, 35, 28, 22, 8, 64, 64, true, true>;

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::PartitionQuotientFilter(std::size_t N, bool Normalize): 
    // buff(8*BuffCacheLines, -1ull),
    capacity{Normalize ? static_cast<size_t>(N/NormalizingFactor) : N},
    range{capacity << SizeRemainders},
    frontyard((capacity+BucketNumMiniBuckets-1)/BucketNumMiniBuckets),
    backyard((frontyard.size()+FrontyardToBackyardRatio-1)/FrontyardToBackyardRatio + FrontyardToBackyardRatio*2)
    // overflows(frontyard.size()),
{
    R = frontyard.size() / FrontyardToBackyardRatio / FrontyardToBackyardRatio + 1;
    if(R % (FrontyardToBackyardRatio - 1) == 0) R++;
    // std::cout << NormalizingFactor << " " << frontyard.size() << std::endl;

    // resetBuffer();
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
std::uint64_t PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::sizeFilter() {
    return (frontyard.size()*sizeof(FrontyardBucketType)) + (backyard.size()*sizeof(BackyardBucketType));
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::FrontyardQRContainerType PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::getQRPairFromHash(std::uint64_t hash) {
    if constexpr (DEBUG) {
        FrontyardQRContainerType f = FrontyardQRContainerType(hash >> SizeRemainders, hash & HashMask);
        assert(f.bucketIndex < frontyard.size());
        BackyardQRContainerType fb1(f, 0, R);
        BackyardQRContainerType fb2(f, 1, R);
        assert(fb1.bucketIndex < backyard.size() && fb2.bucketIndex < backyard.size());
    }
    return FrontyardQRContainerType(hash >> SizeRemainders, hash & HashMask);
}


template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::lockFrontyard(std::size_t i) {
    
    //FIX THE ISSUE OF HAVING TWO LOCKS PER CACHELINE!!!! Not as trivial as sounds, unless can simple make the vector allocate aligned to 64 bytes.
    if constexpr (Threaded) {
        // std::cout << "tl" << i << std::endl;
        // std::cout << "tf" << std::endl;
        frontyard[i & frontyardLockCachelineMask].lock();
        // std::cout << "tfd" << std::endl;
        // std::cout << "l" << i << std::endl;
    }

}


template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::unlockFrontyard(std::size_t i) {
    
    if constexpr (Threaded) {
        frontyard[i & frontyardLockCachelineMask].unlock();
        // std::cout << "u" << i << std::endl;
    }

}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::lockBackyard(std::size_t i1, std::size_t i2) {

    if constexpr (Threaded) {
        i1 &= backyardLockCachelineMask;
        i2 &= backyardLockCachelineMask;
        // std::cout << "tlb" << std::endl;
        if (i1 == i2) { 
            backyard[i1].lock();
            return;
        }
        if (i1 > i2) std::swap(i1, i2);
        backyard[i1].lock();
        backyard[i2].lock();
        // std::cout << "tlbr" << std::endl;
    }

}


template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::unlockBackyard(std::size_t i1, std::size_t i2) {
    
    if constexpr (Threaded) {
        i1 &= backyardLockCachelineMask;
        i2 &= backyardLockCachelineMask;
        if (i1 == i2) { 
            backyard[i1].unlock();
            return;
        }
        // if (i1 > i2) std::swap(i1, i2);
        backyard[i2].unlock();
        backyard[i1].unlock();
    }

}



template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::insertOverflow(FrontyardQRContainerType overflow, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR) {
    // BackyardQRContainerType firstBackyardQR(overflow, 0, R);
    // BackyardQRContainerType secondBackyardQR(overflow, 1, R);
    if constexpr (DEBUG) {
        if(backyardToFrontyard.count(std::make_pair(firstBackyardQR.bucketIndex, firstBackyardQR.whichFrontyardBucket)) == 0) {
            backyardToFrontyard[std::make_pair(firstBackyardQR.bucketIndex, firstBackyardQR.whichFrontyardBucket)] = overflow.bucketIndex;
        }
        if(backyardToFrontyard.count(std::make_pair(secondBackyardQR.bucketIndex, secondBackyardQR.whichFrontyardBucket)) == 0) {
            backyardToFrontyard[std::make_pair(secondBackyardQR.bucketIndex, secondBackyardQR.whichFrontyardBucket)] = overflow.bucketIndex;
        }
        assert(backyardToFrontyard[std::make_pair(firstBackyardQR.bucketIndex, firstBackyardQR.whichFrontyardBucket)] == overflow.bucketIndex);
        assert(backyardToFrontyard[std::make_pair(secondBackyardQR.bucketIndex, secondBackyardQR.whichFrontyardBucket)] == overflow.bucketIndex);
    }
    
    std::size_t fillOfFirstBackyardBucket = backyard[firstBackyardQR.bucketIndex].countKeys();
    std::size_t fillOfSecondBackyardBucket = backyard[secondBackyardQR.bucketIndex].countKeys();
    
    if(fillOfFirstBackyardBucket < fillOfSecondBackyardBucket) {
        if constexpr (PARTIAL_DEBUG || DEBUG)
            assert(backyard[firstBackyardQR.bucketIndex].insert(firstBackyardQR).miniBucketIndex == -1ull); //Failing this would be *really* bad, as it is the main unproven assumption this algo relies on
        else {
            if constexpr (DIAGNOSTICS) {
                insertFailure = backyard[firstBackyardQR.bucketIndex].insert(firstBackyardQR).miniBucketIndex != -1ull;
                failureFB = overflow.bucketIndex;
                failureBucket1 = firstBackyardQR.bucketIndex;
                failureBucket2 = secondBackyardQR.bucketIndex;
                failureWFB = firstBackyardQR.whichFrontyardBucket;
            }
            else {
                backyard[firstBackyardQR.bucketIndex].insert(firstBackyardQR);
            }
        }
        // assert(query(hash));
    }
    else {
        if constexpr (PARTIAL_DEBUG || DEBUG)
            assert(backyard[secondBackyardQR.bucketIndex].insert(secondBackyardQR).miniBucketIndex == -1ull);
        else {
            if constexpr (DIAGNOSTICS) {
                insertFailure = backyard[secondBackyardQR.bucketIndex].insert(secondBackyardQR).miniBucketIndex != -1ull;
                failureFB = overflow.bucketIndex;
                failureBucket1 = firstBackyardQR.bucketIndex;
                failureBucket2 = secondBackyardQR.bucketIndex;
                failureWFB = firstBackyardQR.whichFrontyardBucket;
            }
            else {
                return backyard[secondBackyardQR.bucketIndex].insert(secondBackyardQR).miniBucketIndex == -1ull;
            }
        }
        // assert(query(hash));
    }
    return true;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::insertInner(FrontyardQRContainerType frontyardQR) {
    // FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    FrontyardQRContainerType overflow = frontyard[frontyardQR.bucketIndex].insert(frontyardQR);
    if constexpr (DEBUG) {
        assert((uint64_t)(&frontyard[frontyardQR.bucketIndex]) % FrontyardBucketSize == 0);
    }
    if(overflow.miniBucketIndex != -1ull) {
        // overflows[frontyardQR.bucketIndex]++;
        BackyardQRContainerType firstBackyardQR(overflow, 0, R);
        BackyardQRContainerType secondBackyardQR(overflow, 1, R);
        lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

        bool retval = insertOverflow(overflow, firstBackyardQR, secondBackyardQR);

        unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);
        return retval;
    }
    return true;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::insertBatch(const std::vector<size_t>& hashes, std::vector<bool>& status, const uint64_t num_keys) {
    constexpr size_t bsize = 16;
    for (size_t j=0; j < num_keys; j+=bsize) {
        for (size_t i=j; i < std::min(num_keys, j+bsize); i++) {
            FrontyardQRContainerType frontyardQR = getQRPairFromHash(hashes[i]);
            __builtin_prefetch(&frontyard[frontyardQR.bucketIndex]);
        }
        for(size_t i=j; i < std::min(num_keys, j+bsize); i++) {
            // status[i] = insert(hashes[i]);
            status[i] = forceInsert(hashes[i]);
        }
    }
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
std::uint64_t PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::queryWhereInner(FrontyardQRContainerType frontyardQR) {
    // FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    std::uint64_t frontyardQuery = frontyard[frontyardQR.bucketIndex].query(frontyardQR);
    if(frontyardQuery != 2) return frontyardQuery;

    if constexpr (DIAGNOSTICS) {
        backyardLookupCount ++;
    }

    BackyardQRContainerType firstBackyardQR(frontyardQR, 0, R);
    BackyardQRContainerType secondBackyardQR(frontyardQR, 1, R);
    // return (backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR).first || backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR).first) | 2; //Return true if find it in either of the backyard buckets
    // return ((backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR) | backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR)) & 1) | 2;
    lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    std::uint64_t retval = queryBackyard(frontyardQR, firstBackyardQR, secondBackyardQR);

    unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    return retval | 2;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::queryInner(FrontyardQRContainerType frontyardQR) {
    // FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    std::uint64_t frontyardQuery = frontyard[frontyardQR.bucketIndex].query(frontyardQR);
    if(frontyardQuery != 2) return frontyardQuery;
    
    // return true;

    if constexpr (DIAGNOSTICS) {
        backyardLookupCount ++;
    }

    BackyardQRContainerType firstBackyardQR(frontyardQR, 0, R);
    BackyardQRContainerType secondBackyardQR(frontyardQR, 1, R);
    // return (backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR).first || backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR).first) | 2; //Return true if find it in either of the backyard buckets
    // return backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR) || backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR);
    lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    bool retval = queryBackyard(frontyardQR, firstBackyardQR, secondBackyardQR);

    unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    return retval;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::queryBatch(const std::vector<size_t>& hashes, std::vector<bool>& status, const uint64_t num_keys) {
    constexpr size_t bsize = 16;
    for (size_t j=0; j < num_keys; j+=bsize) {
        for (size_t i=j; i < std::min(num_keys, j+bsize); i++) {
            FrontyardQRContainerType frontyardQR = getQRPairFromHash(hashes[i]);
            __builtin_prefetch(&frontyard[frontyardQR.bucketIndex]);
        }
        for (size_t i=j; i < std::min(num_keys, j+bsize); i++) {
            status[i] = query(hashes[i]);
        }
    }
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::removeFromBackyard(FrontyardQRContainerType frontyardQR, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR, bool elementInFrontyard) {
    if (elementInFrontyard) { //In the case we removed it from the frontyard bucket, we need to bring back an element from the backyard (if there is one)
        //Pretty messy since need to figure out who in the backyard has the key with a smaller miniBucket, since we want to bring the key with the smallest miniBucket index back into the frontyard
        std::size_t fillOfFirstBackyardBucket = backyard[firstBackyardQR.bucketIndex].countKeys();
        std::size_t fillOfSecondBackyardBucket = backyard[secondBackyardQR.bucketIndex].countKeys();
        if(fillOfFirstBackyardBucket==0 && fillOfSecondBackyardBucket==0) return true;
        std::uint64_t keysFromFrontyardInFirstBackyard = backyard[firstBackyardQR.bucketIndex].remainderStore.query4BitPartMask(firstBackyardQR.whichFrontyardBucket, (1ull << fillOfFirstBackyardBucket) - 1);
        std::uint64_t keysFromFrontyardInSecondBackyard = backyard[secondBackyardQR.bucketIndex].remainderStore.query4BitPartMask(secondBackyardQR.whichFrontyardBucket, (1ull << fillOfSecondBackyardBucket) - 1);
        if constexpr (DEBUG) {
            assert(firstBackyardQR.whichFrontyardBucket == firstBackyardQR.remainder >> SizeRemainders);
            assert(secondBackyardQR.whichFrontyardBucket == secondBackyardQR.remainder >> SizeRemainders);
        }
        if(keysFromFrontyardInFirstBackyard == 0 && keysFromFrontyardInSecondBackyard == 0) return true;
        std::uint64_t firstKeyBackyard = __builtin_ctzll(keysFromFrontyardInFirstBackyard);
        std::uint64_t secondKeyBackyard = __builtin_ctzll(keysFromFrontyardInSecondBackyard);
        std::uint64_t firstMiniBucketBackyard = backyard[firstBackyardQR.bucketIndex].queryWhichMiniBucket(firstKeyBackyard);
        std::uint64_t secondMiniBucketBackyard = backyard[secondBackyardQR.bucketIndex].queryWhichMiniBucket(secondKeyBackyard);
        if constexpr (DEBUG) {
            assert(firstMiniBucketBackyard >= frontyard[frontyardQR.bucketIndex].queryWhichMiniBucket(FrontyardBucketCapacity-2) && secondMiniBucketBackyard >= frontyard[frontyardQR.bucketIndex].queryWhichMiniBucket(FrontyardBucketCapacity-2));
        }
        if(firstMiniBucketBackyard < secondMiniBucketBackyard) {
            frontyardQR.miniBucketIndex = firstMiniBucketBackyard;
            frontyardQR.remainder = backyard[firstBackyardQR.bucketIndex].remainderStoreRemoveReturn(firstKeyBackyard, firstMiniBucketBackyard) & HashMask;
        }
        else {
            frontyardQR.miniBucketIndex = secondMiniBucketBackyard;
            frontyardQR.remainder = backyard[secondBackyardQR.bucketIndex].remainderStoreRemoveReturn(secondKeyBackyard, secondMiniBucketBackyard) & HashMask;
        }
        frontyard[frontyardQR.bucketIndex].insert(frontyardQR);
    }
    else {
        if (!backyard[firstBackyardQR.bucketIndex].remove(firstBackyardQR)) {
            return backyard[secondBackyardQR.bucketIndex].remove(secondBackyardQR);
            // if(!backyard[secondBackyardQR.bucketIndex].remove(secondBackyardQR)) {
            //     return false;
            // }
        }
    }
    return true;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::removeInner(FrontyardQRContainerType frontyardQR) {
    // FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    BackyardQRContainerType firstBackyardQR(frontyardQR, 0, R);
    BackyardQRContainerType secondBackyardQR(frontyardQR, 1, R);
    bool frontyardBucketFull = frontyard[frontyardQR.bucketIndex].full();
    bool elementInFrontyard = frontyard[frontyardQR.bucketIndex].remove(frontyardQR);
    if(!frontyardBucketFull) {
        return elementInFrontyard;
    }
    else {
        BackyardQRContainerType firstBackyardQR(frontyardQR, 0, R);
        BackyardQRContainerType secondBackyardQR(frontyardQR, 1, R);
        lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

        bool retval = removeFromBackyard(frontyardQR, firstBackyardQR, secondBackyardQR, elementInFrontyard);

        unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);
        return retval;
    }
    return true;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::removeBatch(const std::vector<size_t>& hashes, std::vector<bool>& status, const uint64_t num_keys) {
    constexpr size_t bsize = 16;
    for (size_t j=0; j < num_keys; j+=bsize) {
        for (size_t i=j; i < std::min(num_keys, j+bsize); i++) {
            FrontyardQRContainerType frontyardQR = getQRPairFromHash(hashes[i]);
            __builtin_prefetch(&frontyard[frontyardQR.bucketIndex]);
        }
        for (size_t i=j; i < std::min(num_keys, j+bsize); i++) {
            status[i] = remove(hashes[i]);
        }
    }
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
size_t PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::getNumBuckets() {
    return frontyard.size();
}







template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
std::uint64_t PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::queryBackyard(FrontyardQRContainerType frontyardQR, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR) {
    
    return backyard[firstBackyardQR.bucketIndex].querySimple(firstBackyardQR) || backyard[secondBackyardQR.bucketIndex].querySimple(secondBackyardQR);

}

//Optimize the following if it turns out they're suboptimal
// template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
// void PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::resetBuffer() {
//     std::fill(std::execution::unseq, buff.begin(), buff.end(), -1ull); //fill with some value larger than R that can't appear as input. Can do R+1, but idk just doing -1ull
// }

// template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
// bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::queryBuffer(std::uint64_t hash) {
//     return std::any_of(std::execution::unseq, buff.begin(), buff.end(), [hash] (std::uint64_t x) {return x == hash;});
// }


template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::forceInsert(std::uint64_t hash) {
    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    lockFrontyard(frontyardQR.bucketIndex);
    // size_t i = frontyardQR.bucketIndex;
    // frontyard[frontyardQR.bucketIndex & frontyardLockCachelineMask].miniFilter.assertLocked();

    bool retval = insertInner(frontyardQR);

    // assert(i == frontyardQR.bucketIndex);
    // frontyard[frontyardQR.bucketIndex & frontyardLockCachelineMask].miniFilter.assertLocked();

    unlockFrontyard(frontyardQR.bucketIndex);

    return retval;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::insert(std::uint64_t hash) {
//    static size_t i = 0;
//    i++;
    if constexpr (EnableBuff) {
        // auto it = std::find(std::execution::unseq, buff.begin(), buff.end(), -1ull);
        // if(it != buff.end()) {
        //     *it = hash;
	    // // std::cout << "gamingle" << std::endl;
        //     return true;
        // }
        // else {
        //     std::vector<bool> statuses(BuffCacheLines*8);
        //     insertBatch(buff, statuses, BuffCacheLines*8);
        //     bool retval = std::reduce(statuses.begin(), statuses.end(), true, [] (bool pres, bool r) {return pres && r;});
        //     resetBuffer();
        //     buff[0] = hash;
	    // // std::cout << i << " " << retval << std::endl;
        //     return retval;
        // }

        static constexpr bool OptimisticallyPrefetch = true;

        if(buff.insert(hash)) {
            // std::cout << "Hello " << i << std::endl;
            auto arr = buff.getArray();
            bool retval = true;

            std::array<bool, arr.size()> status;
            std::fill(status.begin(), status.end(), true);
            constexpr size_t bsize = 32;

            if constexpr (!OptimisticallyPrefetch) {
                for (size_t j=0; j < arr.size(); j+=bsize) {
                    for (size_t i=j; i < std::min(arr.size(), j+bsize); i++) {
                        if(arr[i] == -1ull) continue;
                        FrontyardQRContainerType frontyardQR = getQRPairFromHash(arr[i]);
                        __builtin_prefetch(&frontyard[frontyardQR.bucketIndex]);
                    }
                    for(size_t i=j; i < std::min(arr.size(), j+bsize); i++) {
                        // status[i] = insert(hashes[i]);
                        if(arr[i] == -1ull) continue;
                        // if(forceInsert(arr[i])) {
                        //     arr[i] = -1ull;
                        // }
                        status[i] = forceInsert(arr[i]);
                    }
                }
            }
            else {
                for(size_t i=0; i < arr.size(); i++) {
                    if(arr[i] == -1ull) continue;
                    // if(forceInsert(arr[i])) {
                    //     arr[i] = -1ull;
                    // }
                    status[i] = forceInsert(arr[i]);
                }
            }

            for(bool s: status) {
                retval &= s;
            }
            
            // for (size_t i=0; i < arr.size(); i++) {
            //     if(arr[i] == -1ull) continue;
            //     FrontyardQRContainerType frontyardQR = getQRPairFromHash(arr[i]);
            //     __builtin_prefetch(&frontyard[frontyardQR.bucketIndex]);
            // }
            // for(size_t i=0; i < arr.size(); i++) {
            //     if(arr[i] == -1ull) continue;
            //     retval &= forceInsert(arr[i]);
            // }

            buff.reset();
            return retval;

            // return buff.notFull(hash);
        }
        else {
            if constexpr (OptimisticallyPrefetch) {
                FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
                __builtin_prefetch(&frontyard[frontyardQR.bucketIndex]);
            }
        }
        return true;
    }
    else {
        return forceInsert(hash);
    }

    return false;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
std::uint64_t PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::queryWhere(std::uint64_t hash) {
    if(EnableBuff) {
        std::cerr << "Fix query where with buff or disable this entirely." << std::endl;
        exit(1);
    }

    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    lockFrontyard(frontyardQR.bucketIndex);

    std::uint64_t retval = queryWhereInner(frontyardQR);

    unlockFrontyard(frontyardQR.bucketIndex);

    return retval;

}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::query(std::uint64_t hash) {
    
    // if(EnableBuff) {
    //     if(queryBuffer(hash)) return true;
    // }
    bool retval = false;
    if constexpr (EnableBuff) {
        retval = buff.query(hash);
    }

    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    // if(frontyardQR.miniBucketIndex > BucketNumMiniBuckets*7/8) {
    //     BackyardQRContainerType firstBackyardQR(frontyardQR, 0, R);
    //     BackyardQRContainerType secondBackyardQR(frontyardQR, 1, R);
        
    //     __builtin_prefetch(&backyard[firstBackyardQR.bucketIndex]);
    //     __builtin_prefetch(&backyard[secondBackyardQR.bucketIndex]);
    // }

    lockFrontyard(frontyardQR.bucketIndex);

    // bool retval = queryInner(frontyardQR);
    retval = retval || queryInner(frontyardQR);

    unlockFrontyard(frontyardQR.bucketIndex);
    
    return retval;
}

template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery, bool Threaded>
bool PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, Threaded>::remove(std::uint64_t hash) {
    
    if constexpr (DEBUG)
        assert(query(hash));

    if constexpr (EnableBuff) {
        // auto it = std::find(std::execution::unseq, buff.begin(), buff.end(), -1ull);
        // if(it != buff.end()) {
        //     *it = -1ull;
        //     return true;
        // }
        if(buff.remove(hash)) {
            return true;
        }
    }
    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    lockFrontyard(frontyardQR.bucketIndex);

    bool retval = removeInner(frontyardQR);

    unlockFrontyard(frontyardQR.bucketIndex);

    return retval;
}



// double PartitionQuotientFilter::getAverageOverflow() {
//     double overflow = 0.0;
//     for(size_t o: overflows) overflow+=o;
//     return overflow/frontyard.size();
// }
