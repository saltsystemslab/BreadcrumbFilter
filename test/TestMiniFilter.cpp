#include <iostream>
#include <cassert>
#include <array>
#include <random>
#include <set>
#include "MiniFilter.hpp"
#include "TestUtility.hpp"

using namespace std;
using namespace PQF;

template<size_t NumKeys, size_t NumMiniBuckets>
struct alignas(1) FakeBucket {
    static constexpr size_t NumBytes = (NumKeys+NumMiniBuckets+7)/8;
    static constexpr size_t NumUllongs = (NumBytes+7)/8;
    static constexpr size_t NumExtraBytes = NumUllongs*8 + 10 - NumBytes; //just some random extra bytes to make sure the filter isn't doing weird stuff
    MiniFilter<NumKeys, NumMiniBuckets> filterBytes;
    std::array<uint8_t, NumExtraBytes> extraBytes;
    
    // std::array<uint64_t, NumUllongs + 1> randomData;

    void basicFunctionTestWrapper(std::function<void(void)> func) {
        std::array<uint8_t, NumExtraBytes> extraBytesCopy = extraBytes;
        func();
        filterBytes.checkCorrectPopCount();
        assert(extraBytes == extraBytesCopy);
    }

    FakeBucket(mt19937 generator) {
        assert(sizeof(*this) == NumBytes+NumExtraBytes);

        uniform_int_distribution<uint8_t> uint8_dist(0, 255);
        for(uint8_t& byte: extraBytes) {
            byte = uint8_dist(generator);
        }

        basicFunctionTestWrapper([&] () -> void {
            filterBytes = MiniFilter<NumKeys, NumMiniBuckets>();
        });
    }

    std::optional<uint64_t> insert(std::size_t miniBucketIndex, std::size_t keyIndex) {
        std::optional<uint64_t> retval;
        basicFunctionTestWrapper([&] () -> void {
            retval = filterBytes.testInsert(miniBucketIndex, keyIndex);
        });
        if(*retval == -1ull) return {};
        return retval;
    }

    void remove(std::size_t miniBucketIndex, std::size_t keyIndex) {
        basicFunctionTestWrapper([&] () -> void {
            filterBytes.remove(miniBucketIndex, keyIndex);
        });
    }

    // void checkRemoveFirstElement(std::size_t expectedMiniBucketIndex) {
    //     basicFunctionTestWrapper([&] () -> void {
    //         std::size_t miniBucketIndex = filterBytes.removeFirstElement();
    //         assert(miniBucketIndex == expectedMiniBucketIndex);
    //     });
    // }

    void checkQuery(size_t miniBucketIndex, pair<size_t, size_t> expectedBounds) {
        basicFunctionTestWrapper([&] () -> void {
            pair<size_t, size_t> bounds = filterBytes.queryMiniBucketBounds(miniBucketIndex);
            // cout << "Expected bounds: " << expectedBounds.first << " " << expectedBounds.second << endl;
            // cout << "Got bounds: " << bounds.first << " " << bounds.second << endl;
            assert(bounds == expectedBounds);
        });
    }

    void checkQueryWhichMiniBucket(size_t keyIndex, size_t expectedMiniBucketIndex) {
        basicFunctionTestWrapper([&] () -> void {
            size_t miniBucketIndex = filterBytes.queryWhichMiniBucket(keyIndex);
            // cout << miniBucketIndex << " " << expectedMiniBucketIndex << endl;
            assert(miniBucketIndex == expectedMiniBucketIndex);
        });
    }

    void checkQueryMask(size_t miniBucketIndex, pair<size_t, size_t> expectedBounds) {
        basicFunctionTestWrapper([&] () -> void {
            pair<uint64_t, uint64_t> boundsMask = filterBytes.queryMiniBucketBoundsMask(miniBucketIndex);
            // cout << "Expected bounds: " << expectedBounds.first << " " << expectedBounds.second << endl;
            // cout << "Got bounds: " << __builtin_ctzll(boundsMask.first) << " " << __builtin_ctzll(boundsMask.second) << endl;
            assert(boundsMask.first == (1ull << expectedBounds.first) && boundsMask.second == (1ull << expectedBounds.second));
        });
    }

    pair<size_t, size_t> query(size_t miniBucketIndex) {
        return filterBytes.queryMiniBucketBounds(miniBucketIndex);
    }

    void checkCount(size_t expectedCount) {
        basicFunctionTestWrapper([&] () -> void {
            assert(filterBytes.countKeys() == expectedCount);
        });
    }
};

//TODO: Just changed what MiniFilter insert returns, so verify it returns the correct mini bucket of the thing that overflowed (we know the key index is just the highest one).
//In fact I can guarantee it *doesn't* return the right thing.
template<size_t NumKeys, size_t NumMiniBuckets>
void testBucket(mt19937& generator) {
    cout << "Testing with " << NumKeys << " keys and " << NumMiniBuckets << " mini buckets." << endl;
    FakeBucket<NumKeys, NumMiniBuckets> temp(generator);
    // uniform_int_distribution<uint8_t> miniBucketDist(0, NumMiniBuckets);
    cout << "Testing just with putting all in the front." << endl;
    cout << "Testing filling the mini filter: ";
    for(size_t i{1}; i <= NumKeys; i++) {
        // cout << i << endl;
        // uniform_int_distribution<uint8_t> keyIndexDist(0, i);
        size_t randomMiniBucketIndex = NumMiniBuckets-5;
        size_t randomKeyIndex = 0;
        assert(!temp.insert(randomMiniBucketIndex, randomKeyIndex).has_value());
    }
    cout << "pass" << endl;

    cout << "Testing overflowing the mini filter: ";
    for(size_t i{1}; i <= 10; i++) {
        // uniform_int_distribution<uint8_t> keyIndexDist(0, NumKeys+1);
        size_t randomMiniBucketIndex = 5;
        size_t randomKeyIndex = 0;
        assert(temp.insert(randomMiniBucketIndex, randomKeyIndex).has_value());
    }
    cout << "pass" << endl;

    //Bad random cause it favors longer chains and isn't uniform but whatever we'll just keep it here as a test
    cout << "Testing random inserts." << endl;
    temp = FakeBucket<NumKeys, NumMiniBuckets>(generator);
    cout << "Testing filling the mini filter: ";
    for(size_t i{1}; i <= NumKeys; i++) {
        // cout << i << endl;
        uniform_int_distribution<size_t> posDist(0, i-2+NumMiniBuckets);
        size_t posOfKeyToInsert = posDist(generator);
        uniform_int_distribution<size_t> miniBucketIndexDist(0, min(NumMiniBuckets-1, posOfKeyToInsert));
        size_t randomMiniBucketIndex = miniBucketIndexDist(generator);
        size_t randomKeyIndex =  posOfKeyToInsert-randomMiniBucketIndex;
        // cout << "Inserting " << randomKeyIndex << " " << randomMiniBucketIndex << endl;
        assert(!temp.insert(randomMiniBucketIndex, randomKeyIndex).has_value());
        temp.checkCount(i);
    }
    cout << "pass" << endl;
    
    cout << "Testing overflowing the mini filter: ";
    for(size_t i{1}; i <= NumKeys; i++) {
        uniform_int_distribution<size_t> posDist(0, NumKeys+NumMiniBuckets-1);
        size_t posOfKeyToInsert = posDist(generator);
        uniform_int_distribution<size_t> miniBucketIndexDist(0, min(NumMiniBuckets-1, posOfKeyToInsert));
        size_t randomMiniBucketIndex = miniBucketIndexDist(generator);
        size_t randomKeyIndex =  posOfKeyToInsert-randomMiniBucketIndex;
        std::optional<uint64_t> retval = temp.insert(randomMiniBucketIndex, randomKeyIndex);
        assert(retval.has_value());
        temp.checkCount(NumKeys);
    }
    cout << "pass" << endl;

    cout << "Testing inserting randomly properly, then querying elements afterwards: ";
    temp = FakeBucket<NumKeys, NumMiniBuckets>(generator);
    array<size_t, NumMiniBuckets> sizeEachMiniBucket{};
    for(size_t i{0}; i < NumKeys; i++) {
        uniform_int_distribution<size_t> miniBucketIndexDist(0, NumMiniBuckets-1);
        size_t miniBucketIndex = miniBucketIndexDist(generator);
        //Feels bad to do this rather than say a segment tree but like that's absurd for such small values lol
        size_t keyIndex = 0;
        for(size_t j{0}; j < miniBucketIndex; j++) {
            keyIndex += sizeEachMiniBucket[j];
        }
        assert(!temp.insert(miniBucketIndex, keyIndex).has_value());
        temp.checkCount(i+1);
        sizeEachMiniBucket[miniBucketIndex]++;
    }

    size_t minBound = 0;
    size_t maxBound = sizeEachMiniBucket[0];
    for(size_t miniBucketIndex{0}; miniBucketIndex < NumMiniBuckets; miniBucketIndex++) {
        pair<size_t, size_t> expectedBounds{minBound, maxBound};
        temp.checkQuery(miniBucketIndex, expectedBounds);
        if(NumKeys < 64)
            temp.checkQueryMask(miniBucketIndex, expectedBounds);
        minBound += sizeEachMiniBucket[miniBucketIndex];
        if (miniBucketIndex+1 < NumMiniBuckets) {
            maxBound += sizeEachMiniBucket[miniBucketIndex+1];
        }
        else {
            maxBound = NumKeys;
        }
    }
    cout << "pass" << endl;

    if(NumKeys+NumMiniBuckets <= 128) {
        cout << "Testing if removing half and then adding another random half back in works" << endl;
        //Removing half
        for(size_t i{0}; i < NumKeys/2; i++) {
            uniform_int_distribution<size_t> miniBucketIndexDist(0, NumMiniBuckets-1);
            size_t miniBucketIndex = miniBucketIndexDist(generator);
            while(sizeEachMiniBucket[miniBucketIndex] == 0) miniBucketIndex = miniBucketIndexDist(generator);
            size_t keyIndex = 0;
            for(size_t j{0}; j < miniBucketIndex; j++) {
                keyIndex += sizeEachMiniBucket[j];
            }
            temp.remove(miniBucketIndex, keyIndex);
            temp.checkCount(NumKeys-i-1);
            sizeEachMiniBucket[miniBucketIndex]--;
        }

        // cout << "first half of half passed" << endl;

        std::uint64_t indexFirst = 0;
        for(; sizeEachMiniBucket[indexFirst] == 0; indexFirst++);
        // temp.checkRemoveFirstElement(indexFirst);
        // temp.checkCount(NumKeys-NumKeys/2);
        // sizeEachMiniBucket[indexFirst]--;

        //Adding back another half
        for(size_t i{0}; i < NumKeys/2; i++) {
            uniform_int_distribution<size_t> miniBucketIndexDist(0, NumMiniBuckets-1);
            size_t miniBucketIndex = miniBucketIndexDist(generator);
            size_t keyIndex = 0;
            for(size_t j{0}; j < miniBucketIndex; j++) {
                keyIndex += sizeEachMiniBucket[j];
            }
            assert(!temp.insert(miniBucketIndex, keyIndex).has_value());
            temp.checkCount((NumKeys-NumKeys/2) + i + 1);
            sizeEachMiniBucket[miniBucketIndex]++;
        }

        // cout << "2nd half of half" << endl;

        //Querying
        minBound = 0;
        maxBound = sizeEachMiniBucket[0];
        for(size_t miniBucketIndex{0}; miniBucketIndex < NumMiniBuckets; miniBucketIndex++) {
            pair<size_t, size_t> expectedBounds{minBound, maxBound};
            temp.checkQuery(miniBucketIndex, expectedBounds);
            if(NumKeys+NumMiniBuckets < 128) {
                for(size_t k{expectedBounds.first}; k < expectedBounds.second; k++) {
                    temp.checkQueryWhichMiniBucket(k, miniBucketIndex);
                }
            }
            if(NumKeys < 64)
                temp.checkQueryMask(miniBucketIndex, expectedBounds);
            minBound += sizeEachMiniBucket[miniBucketIndex];
            if (miniBucketIndex+1 < NumMiniBuckets) {
                maxBound += sizeEachMiniBucket[miniBucketIndex+1];
            }
            else {
                maxBound = NumKeys;
            }
        }
        cout << "pass" << endl;
    }

    cout << "Testing if overflow values are correct: ";
    temp = FakeBucket<NumKeys, NumMiniBuckets>(generator);
    multiset<uint64_t> miniBucketIndices{};
    for(size_t i{1}; i <= NumKeys; i++) {
        uniform_int_distribution<size_t> miniBucketIndexDist(0, NumMiniBuckets-1);
        size_t randomMiniBucketIndex = miniBucketIndexDist(generator);
        miniBucketIndices.insert(randomMiniBucketIndex);
        // cout << "Inserting " << randomKeyIndex << " " << randomMiniBucketIndex << endl;
        pair<size_t, size_t> bounds = temp.query(randomMiniBucketIndex);
        assert(!temp.insert(randomMiniBucketIndex, bounds.first).has_value());
    }

    for(size_t i{1}; i <= NumKeys; i++) {
        uniform_int_distribution<size_t> miniBucketIndexDist(0, NumMiniBuckets-1);
        size_t randomMiniBucketIndex = miniBucketIndexDist(generator);
        miniBucketIndices.insert(randomMiniBucketIndex);
        pair<size_t, size_t> bounds = temp.query(randomMiniBucketIndex);
        std::optional<uint64_t> retval = temp.insert(randomMiniBucketIndex, bounds.first);
        assert(retval.has_value());
        // cout << *retval << " " << *miniBucketIndices.rbegin() << endl;
        assert(*retval == *miniBucketIndices.rbegin());
        miniBucketIndices.erase(--miniBucketIndices.end());
    }
    cout << "pass" << endl;

    cout << endl;
}

int main() {
    #ifdef __AVX512BW__
        printf("AVX-512BW is supported at compile time.\n");
    #else
        printf("AVX-512BW is NOT supported at compile time.\n");
    #endif


    random_device rd;
    mt19937 generator (rd());

    for(size_t i{0}; i < 5; i++) {
        testBucket<51, 52>(generator);
        testBucket<25, 26>(generator);
        // testBucket<75, 61>(generator);
        // testBucket<400, 3>(generator); //Just to hit extreme cases that I account for but aren't really all that necessary in the actual filter design lol
    }
}