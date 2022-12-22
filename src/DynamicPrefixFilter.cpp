#include "DynamicPrefixFilter.hpp"
#include <optional>
#include <iostream>
#include <map>
#include <utility>
#include <algorithm>

using namespace DynamicPrefixFilter;

// template class DynamicPrefixFilter8Bit<46, 51, 16, 8, 64, 32>; //These fail for obvious reasons, but I wanted to try anyways
// template class DynamicPrefixFilter8Bit<46, 51, 16, 7, 64, 32>;
template class DynamicPrefixFilter8Bit<46, 51, 35, 8, 64, 64>;
template class DynamicPrefixFilter8Bit<48, 51, 35, 8, 64, 64>;
template class DynamicPrefixFilter8Bit<49, 51, 35, 8, 64, 64>;
template class DynamicPrefixFilter8Bit<51, 51, 35, 8, 64, 64>;
template class DynamicPrefixFilter8Bit<22, 25, 17, 8, 32, 32>;
template class DynamicPrefixFilter8Bit<22, 25, 17, 8, 32, 32, true>;
template class DynamicPrefixFilter8Bit<25, 25, 17, 8, 32, 32>;
template class DynamicPrefixFilter8Bit<25, 25, 17, 4, 32, 32>;
template class DynamicPrefixFilter8Bit<25, 25, 35, 8, 32, 64>;
template class DynamicPrefixFilter8Bit<25, 25, 16, 8, 32, 32>;
template class DynamicPrefixFilter8Bit<23, 25, 17, 8, 32, 32>;
template class DynamicPrefixFilter8Bit<24, 25, 17, 8, 32, 32>;
template class DynamicPrefixFilter8Bit<24, 25, 17, 6, 32, 32>;

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::DynamicPrefixFilter8Bit(std::size_t N):

    frontyard((N+BucketNumMiniBuckets-1)/BucketNumMiniBuckets),
    backyard((frontyard.size()+FrontyardToBackyardRatio-1)/FrontyardToBackyardRatio + FrontyardToBackyardRatio),
    // overflows(frontyard.size()),
    capacity{N},
    range{capacity*256}
{}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
std::uint64_t DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity,
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::sizeFilter() {
    
    return (frontyard.size()*sizeof(FrontyardBucketType)) + (backyard.size()*sizeof(BackyardBucketType));
}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, 
    FrontyardBucketSize, BackyardBucketSize, Threaded>::FrontyardQRContainerType DynamicPrefixFilter8Bit<BucketNumMiniBuckets, 
    FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::getQRPairFromHash(std::uint64_t hash) {

    if constexpr (DEBUG) {
        FrontyardQRContainerType f = FrontyardQRContainerType(hash >> 8, hash & 255);
        assert(f.bucketIndex < frontyard.size());
        BackyardQRContainerType fb1(f, 0, frontyard.size());
        BackyardQRContainerType fb2(f, 1, frontyard.size());
        assert(fb1.bucketIndex < backyard.size() && fb2.bucketIndex < backyard.size());
    }
    return FrontyardQRContainerType(hash >> 8, hash & 255);
}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::lockFrontyard(std::size_t i) {
    
    //FIX THE ISSUE OF HAVING TWO LOCKS PER CACHELINE!!!! Not as trivial as sounds, unless can simple make the vector allocate aligned to 64 bytes.
    if constexpr (Threaded) {
        // std::cout << "trying to lock frontyard " << i << std::endl;
        // std::cout << "tf" << std::endl;
        frontyard[i & frontyardLockCachelineMask].lock();
        // std::cout << "tfd" << std::endl;
        // std::cout << "locked" << std::endl;
    }

}


template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::unlockFrontyard(std::size_t i) {
    
    if constexpr (Threaded) {
        frontyard[i & frontyardLockCachelineMask].unlock();
        // std::cout << "unlocked frontyard " << i << std::endl;
    }

}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::lockBackyard(std::size_t i1, std::size_t i2) {

    if constexpr (Threaded) {
        i1 &= backyardLockCachelineMask;
        i2 &= backyardLockCachelineMask;
        if (i1 == i2) { 
            backyard[i1].lock();
            return;
        }
        if (i1 > i2) std::swap(i1, i2);
        // std::cout << "tl" << std::endl;
        backyard[i1].lock();
        backyard[i2].lock();
        // std::cout << "tlr" << std::endl;
    }

}


template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::unlockBackyard(std::size_t i1, std::size_t i2) {
    
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

// template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
//     std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
// template<typename Function>
// void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
//     FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::backyardLockWrapper(FrontyardQRContainerType frontyardQR, Function func) {
    
//     BackyardQRContainerType firstBackyardQR(frontyardQR, 0, frontyard.size());
//     BackyardQRContainerType secondBackyardQR(frontyardQR, 1, frontyard.size());

//     lockBackyard(firstBackyardQR, secondBackyardQR);

//     auto retval = func(frontyardQR, firstBackyardQR, secondBackyardQR);

//     unlockBackyard(firstBackyardQR, secondBackyardQR);

//     return retval;

// }




template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::insertOverflow(FrontyardQRContainerType overflow, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR) {
    
    // BackyardQRContainerType firstBackyardQR(overflow, 0, frontyard.size());
    // BackyardQRContainerType secondBackyardQR(overflow, 1, frontyard.size());
    // lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

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
        // if constexpr (PARTIAL_DEBUG || DEBUG)
        //     assert(backyard[firstBackyardQR.bucketIndex].insert(firstBackyardQR).miniBucketIndex == -1ull); //Failing this would be *really* bad, as it is the main unproven assumption this algo relies on
        // else 
            backyard[firstBackyardQR.bucketIndex].insert(firstBackyardQR);
        // assert(query(hash));
    }
    else {
        // if constexpr (PARTIAL_DEBUG || DEBUG)
        //     assert(backyard[secondBackyardQR.bucketIndex].insert(secondBackyardQR).miniBucketIndex == -1ull);
        // else
            backyard[secondBackyardQR.bucketIndex].insert(secondBackyardQR);
        // assert(query(hash));
    }

    // unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);
}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
bool DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::queryBackyard(FrontyardQRContainerType frontyardQR, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR) {
    
    return backyard[firstBackyardQR.bucketIndex].querySimple(firstBackyardQR) || backyard[secondBackyardQR.bucketIndex].querySimple(secondBackyardQR);

}


template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
bool DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::removeFromBackyard(FrontyardQRContainerType frontyardQR, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR, bool elementInFrontyard){

    if (elementInFrontyard) { //In the case we removed it from the frontyard bucket, we need to bring back an element from the backyard (if there is one)
        //Pretty messy since need to figure out who in the backyard has the key with a smaller miniBucket, since we want to bring the key with the smallest miniBucket index back into the frontyard
        

        std::size_t fillOfFirstBackyardBucket = backyard[firstBackyardQR.bucketIndex].countKeys();
        std::size_t fillOfSecondBackyardBucket = backyard[secondBackyardQR.bucketIndex].countKeys();
        if(fillOfFirstBackyardBucket==0 && fillOfSecondBackyardBucket==0) return true;
        std::uint64_t keysFromFrontyardInFirstBackyard = backyard[firstBackyardQR.bucketIndex].remainderStore.
                query4BitPartMask(firstBackyardQR.whichFrontyardBucket, (1ull << fillOfFirstBackyardBucket) - 1);
        std::uint64_t keysFromFrontyardInSecondBackyard = backyard[secondBackyardQR.bucketIndex].remainderStore.
                query4BitPartMask(secondBackyardQR.whichFrontyardBucket, (1ull << fillOfSecondBackyardBucket) - 1);
        if constexpr (DEBUG) {
            assert(firstBackyardQR.whichFrontyardBucket == firstBackyardQR.remainder >> 8);
            assert(secondBackyardQR.whichFrontyardBucket == secondBackyardQR.remainder >> 8);
        }
        if(keysFromFrontyardInFirstBackyard == 0 && keysFromFrontyardInSecondBackyard == 0) return true;
        std::uint64_t firstKeyBackyard = __builtin_ctzll(keysFromFrontyardInFirstBackyard);
        std::uint64_t secondKeyBackyard = __builtin_ctzll(keysFromFrontyardInSecondBackyard);
        std::uint64_t firstMiniBucketBackyard = backyard[firstBackyardQR.bucketIndex].queryWhichMiniBucket(firstKeyBackyard);
        std::uint64_t secondMiniBucketBackyard = backyard[secondBackyardQR.bucketIndex].queryWhichMiniBucket(secondKeyBackyard);
        if constexpr (DEBUG) {
            assert(firstMiniBucketBackyard >= frontyard[frontyardQR.bucketIndex].queryWhichMiniBucket(FrontyardBucketCapacity-2) 
                    && secondMiniBucketBackyard >= frontyard[frontyardQR.bucketIndex].queryWhichMiniBucket(FrontyardBucketCapacity-2));
        }
        if(firstMiniBucketBackyard < secondMiniBucketBackyard) {
            frontyardQR.miniBucketIndex = firstMiniBucketBackyard;
            frontyardQR.remainder = backyard[firstBackyardQR.bucketIndex].remainderStoreRemoveReturn(firstKeyBackyard, firstMiniBucketBackyard) & 255;
        }
        else {
            frontyardQR.miniBucketIndex = secondMiniBucketBackyard;
            frontyardQR.remainder = backyard[secondBackyardQR.bucketIndex].remainderStoreRemoveReturn(secondKeyBackyard, secondMiniBucketBackyard) & 255;
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



template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::insertInner(FrontyardQRContainerType frontyardQR) {

    FrontyardQRContainerType overflow = frontyard[frontyardQR.bucketIndex].insert(frontyardQR);
    if constexpr (DEBUG) {
        assert((uint64_t)(&frontyard[frontyardQR.bucketIndex]) % FrontyardBucketSize == 0);
    }
    if(overflow.miniBucketIndex != -1ull) {
        BackyardQRContainerType firstBackyardQR(overflow, 0, frontyard.size());
        BackyardQRContainerType secondBackyardQR(overflow, 1, frontyard.size());
        lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

        // overflows[frontyardQR.bucketIndex]++;
        insertOverflow(overflow, firstBackyardQR, secondBackyardQR);

        unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);
    }

}


template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
bool DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::queryWhereInner(FrontyardQRContainerType frontyardQR){

    std::uint64_t frontyardQuery = frontyard[frontyardQR.bucketIndex].query(frontyardQR);
    if(frontyardQuery != 2) {
        return frontyardQuery;
    }

    BackyardQRContainerType firstBackyardQR(frontyardQR, 0, frontyard.size());
    BackyardQRContainerType secondBackyardQR(frontyardQR, 1, frontyard.size());
    lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    // return (backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR).first || backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR).first) | 2; //Return true if find it in either of the backyard buckets
    // std::uint64_t retval =  ((backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR) | 
    //         backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR)) & 1) | 2;

    std::uint64_t retval = queryBackyard(frontyardQR, firstBackyardQR, secondBackyardQR) | 2;

    unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    return retval;

}


template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
bool DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::queryInner(FrontyardQRContainerType frontyardQR){

    std::uint64_t frontyardQuery = frontyard[frontyardQR.bucketIndex].query(frontyardQR);
    if(frontyardQuery != 2) {
        return frontyardQuery;
    }

    BackyardQRContainerType firstBackyardQR(frontyardQR, 0, frontyard.size());
    BackyardQRContainerType secondBackyardQR(frontyardQR, 1, frontyard.size());
    lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    // return (backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR).first || backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR).first) | 2; //Return true if find it in either of the backyard buckets
    // bool retval =  backyard[firstBackyardQR.bucketIndex].query(firstBackyardQR) || backyard[secondBackyardQR.bucketIndex].query(secondBackyardQR);

    bool retval = queryBackyard(frontyardQR, firstBackyardQR, secondBackyardQR);

    unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

    return retval;
}


template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
bool DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::removeInner(FrontyardQRContainerType frontyardQR) {
    
    bool frontyardBucketFull = frontyard[frontyardQR.bucketIndex].full();
    bool elementInFrontyard = frontyard[frontyardQR.bucketIndex].remove(frontyardQR);
    bool retval;
    if(!frontyardBucketFull) {
        retval = elementInFrontyard;
    }
    else {
        BackyardQRContainerType firstBackyardQR(frontyardQR, 0, frontyard.size());
        BackyardQRContainerType secondBackyardQR(frontyardQR, 1, frontyard.size());
        lockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);

        retval = removeFromBackyard(frontyardQR, firstBackyardQR, secondBackyardQR, elementInFrontyard);

        unlockBackyard(firstBackyardQR.bucketIndex, secondBackyardQR.bucketIndex);
        
    }

    return retval;
    
}








template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
void DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::insert(std::uint64_t hash) {

    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    lockFrontyard(frontyardQR.bucketIndex);

    insertInner(frontyardQR);

    unlockFrontyard(frontyardQR.bucketIndex);
}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
std::uint64_t DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::queryWhere(std::uint64_t hash) {
    
    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    lockFrontyard(frontyardQR.bucketIndex);

    std::uint64_t retval = queryWhereInner(frontyardQR);

    unlockFrontyard(frontyardQR.bucketIndex);

    return retval;

}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
bool DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::query(std::uint64_t hash) {
    
    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    lockFrontyard(frontyardQR.bucketIndex);

    bool retval = queryInner(frontyardQR);

    unlockFrontyard(frontyardQR.bucketIndex);
    
    return retval;
}

template<std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, 
    std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool Threaded>
bool DynamicPrefixFilter8Bit<BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, 
    FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, Threaded>::remove(std::uint64_t hash) {
    
    if constexpr (DEBUG)
        assert(query(hash));
    FrontyardQRContainerType frontyardQR = getQRPairFromHash(hash);
    lockFrontyard(frontyardQR.bucketIndex);

    bool retval = removeInner(frontyardQR);

    unlockFrontyard(frontyardQR.bucketIndex);

    return retval;
}

// double DynamicPrefixFilter8Bit::getAverageOverflow() {
//     double overflow = 0.0;
//     for(size_t o: overflows) overflow+=o;
//     return overflow/frontyard.size();
// }