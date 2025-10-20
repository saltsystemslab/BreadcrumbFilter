#include <iostream>
#include <cassert>
#include <array>
#include <random>
#include <set>
#include <map>
#include <optional>
#include "RemainderStore.hpp"

using namespace std;
using namespace PQF;

template<size_t SizeRemainder, size_t NumKeys, template<size_t, std::size_t, std::size_t> class StoreType>
struct alignas(64) FakeBucket {
    static constexpr size_t frontOffsetSize = (64-StoreType<SizeRemainder, NumKeys, 0>::Size)/2;
    static constexpr size_t backOffsetSize = 64-StoreType<SizeRemainder, NumKeys, 0>::Size-frontOffsetSize;
    
    std::array<uint8_t, frontOffsetSize> frontOffset;
    StoreType<SizeRemainder, NumKeys, frontOffsetSize> remainderStore;
    std::array<uint8_t, backOffsetSize> backOffset;

    void basicFunctionTestWrapper(std::function<void(void)> func) {
        std::array<uint8_t, frontOffsetSize> frontOffsetCopy = frontOffset;
        std::array<uint8_t, backOffsetSize> backOffsetCopy = backOffset;
        func();
        assert(frontOffsetCopy == frontOffset);
        assert(backOffsetCopy == backOffset);
    }

    FakeBucket(mt19937 generator) {
        assert(sizeof(*this) == 64);

        uniform_int_distribution<uint8_t> uint8_dist(0, 255);
        for(uint8_t& byte: frontOffset) {
            byte = uint8_dist(generator);
        }
        for(uint8_t& byte: backOffset) {
            byte = uint8_dist(generator);
        }
    }

    void insert(uint64_t remainder, size_t loc, [[maybe_unused]] optional<uint64_t> expectedOverflow) {
        basicFunctionTestWrapper([&] () -> void {
            remainderStore.insert(remainder, loc);
            // uint64_t overflow = remainderStore.insert(remainder, loc);
            // assert(!expectedOverflow.has_value() || overflow == *expectedOverflow); //Changed things around a bit for this so not going to test it at least not for now
        });
    }

    void checkQuery(std::uint64_t remainder, std::pair<size_t, size_t> bounds, std::uint64_t expectedMask) {
        basicFunctionTestWrapper([&] () -> void {
            uint64_t mask = remainderStore.query(remainder, bounds);
            // cout << "Querying " << (int)remainder << " in [" << bounds.first << ", " << bounds.second << ")" << endl;
            // printBinaryUInt64(mask, true);
            // printBinaryUInt64(expectedMask, true);
            assert(mask == expectedMask);
        });
    }

    void remove(std::size_t loc) {
        basicFunctionTestWrapper([&] () -> void {
            remainderStore.remove(loc);
        });
    }

    void checkRemoveReturn(std::size_t loc, std::uint64_t expectedReturn) {
        basicFunctionTestWrapper([&] () -> void {
            assert(expectedReturn == remainderStore.removeReturn(loc));
        });
    }
};

template<std::size_t SizeRemainder, size_t NumKeys, size_t NumMiniBuckets, template<std::size_t, std::size_t, std::size_t> class StoreType, size_t StoreTypeRemainderSize>
void testBucket(mt19937& generator) {
    cout << "Testing with " << NumKeys << " keys." << endl;

    FakeBucket<SizeRemainder, NumKeys, StoreType> testBucket(generator);
    array<size_t, NumMiniBuckets> sizeEachMiniBucket{};
    // map<pair<size_t, uint64_t>, uint64_t> remToExpectedMask;
    set<pair<pair<size_t, size_t>, uint64_t>> remaindersForUnordered;
    for(size_t i{0}; i < NumKeys*3; i++) {
        // cout << "i is " << i << endl;
        if(i < NumKeys*2) { //Testing insertions
            //Generate random data
            uniform_int_distribution<size_t> miniBucketIndexDist(0, NumMiniBuckets-1);
            size_t miniBucketIndex = miniBucketIndexDist(generator);
            uniform_int_distribution<uint64_t> remainderDist(0, (1ull << StoreTypeRemainderSize)-1);
            uint64_t remainder = remainderDist(generator);
            remaindersForUnordered.insert(make_pair(make_pair(miniBucketIndex, NumKeys*2-i), remainder)); //so that its ordered not by remainders in mini bucket but by time inserted in mini bucket

            //Test insertion
            if(i >= NumKeys) {
                // auto it = remToExpectedMask.begin();
                // size_t countSmaller = 0;
                // for(; it->first > make_pair(miniBucketIndex, remainder); it++, countSmaller++);
                size_t insertLoc = 0;
                auto it = remaindersForUnordered.begin();
                auto expectedOverflow = *(remaindersForUnordered.rbegin());
                for(; (it->first).first < miniBucketIndex; it++, insertLoc++);
                testBucket.insert(remainder, insertLoc, {expectedOverflow.second});
                sizeEachMiniBucket[(remaindersForUnordered.rbegin())->first.first]--;
                remaindersForUnordered.erase(--remaindersForUnordered.end());
            }
            else {
                size_t insertLoc = 0;
                auto it = remaindersForUnordered.begin();
                for(; (it->first).first < miniBucketIndex; it++, insertLoc++);
                testBucket.insert(remainder, insertLoc, {});
            }
            sizeEachMiniBucket[miniBucketIndex]++;
        }

        else { //Testing removals
            uniform_int_distribution<size_t> randomKeyDist(0, NumKeys*3-i-1);
            size_t randomKeyIndex = randomKeyDist(generator);
            auto it = remaindersForUnordered.begin();
            for(size_t i{0}; i < randomKeyIndex; i++, it++);
            sizeEachMiniBucket[it->first.first]--;
            // testBucket.remove(randomKeyIndex);
            testBucket.checkRemoveReturn(randomKeyIndex, it->second);
            remaindersForUnordered.erase(it);
        }

        map<pair<size_t, uint64_t>, size_t> miniBucketRemainderCounts;
        auto it = remaindersForUnordered.begin();
        for(size_t j{0}; j < NumKeys && it != remaindersForUnordered.end(); j++, it++){
            miniBucketRemainderCounts[make_pair((it->first).first, it->second)] += 1ull << j;
        }
        
        size_t minBound = 0;
        //Just temporary cause well it should be fast enough even for 12 bits but testing 16 bits obv can't test everything, so there just make it like test just the things in the map
        for(size_t j{0}; j < NumMiniBuckets && minBound < NumKeys; j++) {
            // for(size_t k{0}; k < (1ull << StoreTypeRemainderSize)-1; k++) {
            //     testBucket.checkQuery(k, make_pair(minBound, min(minBound+sizeEachMiniBucket[j], NumKeys)), miniBucketRemainderCounts[make_pair(j, k)]);
            // }
            for (auto it = miniBucketRemainderCounts.begin(); it != miniBucketRemainderCounts.end(); it++) {
                if((it->first).first != j) continue;
                testBucket.checkQuery((it->first).second, make_pair(minBound, min(minBound+sizeEachMiniBucket[j], NumKeys)), it->second);
            }
            minBound += sizeEachMiniBucket[j];
        }
    }
}

template<template<std::size_t SizeRemainders, std::size_t NumRemainders, std::size_t Offset> class StoreType, size_t StoreTypeRemainderSize>
void runTests(mt19937& generator) {
    for(size_t i{0}; i < 100; i++) {
        testBucket<StoreTypeRemainderSize, 15, 1, StoreType, StoreTypeRemainderSize>(generator);
        testBucket<StoreTypeRemainderSize, 35, 52, StoreType, StoreTypeRemainderSize>(generator);
        testBucket<StoreTypeRemainderSize, 51, 52, StoreType, StoreTypeRemainderSize>(generator);
        testBucket<StoreTypeRemainderSize, 25, 26, StoreType, StoreTypeRemainderSize>(generator);
        testBucket<StoreTypeRemainderSize, 47, 61, StoreType, StoreTypeRemainderSize>(generator);
        testBucket<StoreTypeRemainderSize, 60, 5, StoreType, StoreTypeRemainderSize>(generator);
    }
}

int main() {
    random_device rd;
    mt19937 generator (rd());

    // cout << "Testing 8 bit" << endl;
    // runTests<RemainderStore, 8>(generator);
    // cout << "Testing 4 bit" << endl;
    // runTests<RemainderStore, 4>(generator);

    for(size_t i{0}; i < 100; i++) {
        cout << "Testing 8 bit" << endl;
        testBucket<8, 51, 53, RemainderStore, 8>(generator);
        cout << "Testing 12 bit" << endl;
        testBucket<12, 35, 53, RemainderStore, 12>(generator);
        cout << "Testing 16 bit" << endl;
        testBucket<16, 24, 24, RemainderStore, 16>(generator);
        cout << "Testing 20 bit" << endl;
        testBucket<20, 16, 16, RemainderStore, 20>(generator);
    }
    // cout << "Testing 12 bit (composite of 4 & 8)" << endl;
    // for(size_t i{0}; i < 100; i++) {
    //     testBucket<12, 35, 52, RemainderStore, 12>(generator);
    //     testBucket<12, 15, 1, RemainderStore, 12>(generator);
    //     testBucket<12, 25, 26, RemainderStore, 12>(generator);
    //     testBucket<12, 37, 26, RemainderStore, 12>(generator);
    // }
}