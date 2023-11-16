#ifndef DYNAMIC_PREFIX_FILTER_HPP
#define DYNAMIC_PREFIX_FILTER_HPP

#include <vector>
#include <cstddef>
#include <cstdint>
#include <map>
#include "Bucket.hpp"
#include "QRContainers.hpp"
#include "RemainderStore.hpp"
#include "TinyTable.hpp"

namespace DynamicPrefixFilter {

    template<typename T, size_t alignment>
    class AlignedVector { //Just here to make locking easier, lol
        private:
            std::size_t s;
            std::size_t alignedSize;
            T* vec;

            constexpr size_t getAlignedSize(size_t num) {
                return ((num*sizeof(T)+alignment - 1) / alignment)*alignment;
            }

        public:
            AlignedVector(std::size_t s=0): s{s}, alignedSize{getAlignedSize(s)}, vec{static_cast<T*>(std::aligned_alloc(alignment, alignedSize))} {
                // std::cout << alignedSize << std::endl;
                for(size_t i{0}; i < s; i++) {
                    // if(i%10000 == 0) std::cout << i << std::endl;
                    vec[i] = T();
                }
            }
            ~AlignedVector() {
                if(vec != NULL)
                    free(vec);
            }

            AlignedVector(const AlignedVector& a): s{a.s}, alignedSize{getAlignedSize(s)}, vec{static_cast<T*>(std::aligned_alloc(alignment, alignedSize))} {
                memcpy(vec, a.vec, alignedSize);
            }

            AlignedVector& operator=(const AlignedVector& a) {
                if(vec!=NULL)
                    free(vec);
                s = a.s;
                alignedSize = a.alignedSize;
                vec = static_cast<T*>(std::aligned_alloc(alignment, alignedSize));
                memcpy(vec, a.vec, alignedSize);
                return *this;
            }

            AlignedVector(AlignedVector&& a): s{a.s}, alignedSize{a.alignedSize}, vec{a.vec} {
                a.vec = NULL;
                a.s = 0;
                a.alignedSize = 0;
            }

            AlignedVector& operator=(AlignedVector&& a) {
                vec = a.vec;
                s = a.s;
                alignedSize = a.alignedSize;
                a.s = 0;
                a.alignedSize = 0;
                a.vec = NULL;
                return *this;
            }

            T& operator[](size_t i) {
                return vec[i];
            }

            const T& operator[](size_t i) const {
                return vec[i];
            }

            size_t size() {
                return s;
            }
    };

    template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity = 51, std::size_t BackyardBucketCapacity = 35, std::size_t FrontyardToBackyardRatio = 8, std::size_t FrontyardBucketSize = 64, std::size_t BackyardBucketSize = 64, bool FastSQuery = false, bool Threaded = false>
    class PartitionQuotientFilter {
        static_assert(FrontyardBucketSize == 32 || FrontyardBucketSize == 64);
        static_assert(BackyardBucketSize == 32 || BackyardBucketSize == 64);
        // constexpr static std::size_t FrontyardBucketSize = 64;
        // //Change the following two names? they're not consistent with the rest of everything, although I kind of want it to be different to differentiate global config from the local template param, so idk.
        // constexpr static std::size_t FrontyardBucketCapacity = 51; //number of actual remainders stored before overflow to backyard
        // constexpr static std::size_t BucketNumMiniBuckets = 48; //the average number of keys that we expect to actually go into the bucket
        // //Set BucketNumMiniBuckets to:
        //     //51 to be space efficient (even 52 but that's even slower and cutting it a bit close with space overallocation to not fail)
        //     //49 to match 90% capacity VQF size -- roughly same speed for queries, faster for inserts
        //     //46 to match 85% capacity VQF size -- faster for everything
        //     //22 and FrontyardBucketCapacity to 25 for the really fast filter that needs smaller bucket sizes to work. Probably backyard to size 32 with capacity 17

        // constexpr static std::size_t BackyardBucketSize = 64;
        // constexpr static std::size_t BackyardBucketCapacity = 35;
        // constexpr static std::size_t FrontyardToBackyardRatio = 8; //Max possible = 8
        //seems like even 8 works, so switch to 8?

        private:
            using FrontyardQRContainerType = FrontyardQRContainer<BucketNumMiniBuckets>;
            using FrontyardBucketType = Bucket<SizeRemainders, FrontyardBucketCapacity, BucketNumMiniBuckets, FrontyardQRContainer, FrontyardBucketSize, FastSQuery, Threaded>;
            static_assert(sizeof(FrontyardBucketType) == FrontyardBucketSize);
            using BackyardQRContainerType = BackyardQRContainer<BucketNumMiniBuckets, SizeRemainders, FrontyardToBackyardRatio>;
            template<size_t NumMiniBuckets>
            using WrappedBackyardQRContainerType = BackyardQRContainer<NumMiniBuckets, SizeRemainders, FrontyardToBackyardRatio>;
            using BackyardBucketType = Bucket<SizeRemainders + 4, BackyardBucketCapacity, BucketNumMiniBuckets, WrappedBackyardQRContainerType, BackyardBucketSize, FastSQuery, Threaded>;
            static_assert(sizeof(BackyardBucketType) == BackyardBucketSize);

            static constexpr double NormalizingFactor = (double)FrontyardBucketCapacity / (double) BucketNumMiniBuckets * (double)(1+FrontyardToBackyardRatio*FrontyardBucketSize/BackyardBucketSize)/(FrontyardToBackyardRatio*FrontyardBucketSize/BackyardBucketSize);

            static constexpr uint64_t HashMask = (1ull << SizeRemainders) - 1;

            static_assert(64 % FrontyardBucketSize == 0 && 64 % BackyardBucketSize == 0);

            static constexpr std::size_t frontyardLockCachelineMask = ~(64ull / FrontyardBucketSize - 1); //So that if multiple buckets in same cacheline, we always pick the same one to lock to not get corruption.
            inline void lockFrontyard(std::size_t i);
            inline void unlockFrontyard(std::size_t i);

            static constexpr std::size_t backyardLockCachelineMask = ~(64ull / BackyardBucketSize - 1);
            inline void lockBackyard(std::size_t i1, std::size_t i2);
            inline void unlockBackyard(std::size_t i1, std::size_t i2);
            
            std::uint64_t R;
            std::map<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t> backyardToFrontyard; //Comment this out when done with testing I guess?
            // std::vector<size_t> overflows;
            FrontyardQRContainerType getQRPairFromHash(std::uint64_t hash);
            inline bool insertOverflow(FrontyardQRContainerType overflow, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR);
            inline std::uint64_t queryBackyard(FrontyardQRContainerType overflow, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR);
            inline bool removeFromBackyard(FrontyardQRContainerType overflow, BackyardQRContainerType firstBackyardQR, BackyardQRContainerType secondBackyardQR, bool elementInFrontyard);

            inline bool insertInner(FrontyardQRContainerType frontyardQR);
            inline std::uint64_t queryWhereInner(FrontyardQRContainerType frontyardQR);
            inline bool queryInner(FrontyardQRContainerType frontyardQR);
            inline bool removeInner(FrontyardQRContainerType frontyardQR);
        
        private:
            static constexpr bool EnableBuff = !Threaded;
            static_assert(!(EnableBuff && Threaded)); //Not supported for now
            // TinyBuffer buff;
            static constexpr size_t SizeBuff = 32;
            TinyTable<SizeBuff> buff;
            // static constexpr size_t BuffCacheLines = 2; //hardcoded for now
            // std::vector<uint64_t> buff;
            // size_t buffIndex = 0;

            // void resetBuffer();
            // bool queryBuffer(std::uint64_t hash);

        public:
            bool insertFailure = false;
            std::size_t failureFB = 0;
            std::size_t failureBucket1 = 0;
            std::size_t failureBucket2 = 0;
            std::size_t failureWFB = 0;
            std::size_t backyardLookupCount = 0;

            // std::size_t normalizedCapacity;
            std::size_t capacity;
            std::size_t range;
            PartitionQuotientFilter(std::size_t N, bool Normalize = true);
            bool insert(std::uint64_t hash);
            bool forceInsert(std::uint64_t hash);
            void insertBatch(const std::vector<size_t>& hashes, std::vector<bool>& status, const uint64_t num_keys); //Really lazy and not super optimized implementation just to show that morton filters can easily be beaten
            std::uint64_t queryWhere(std::uint64_t hash); //also queries where the item is (backyard or frontyard)
            bool query(std::uint64_t hash);
            void queryBatch(const std::vector<size_t>& hashes, std::vector<bool>& status, const uint64_t num_keys);
            std::uint64_t sizeFilter();
            bool remove(std::uint64_t hash);
            void removeBatch(const std::vector<size_t>& hashes, std::vector<bool>& status, const uint64_t num_keys);

            size_t getNumBuckets();
            // double getAverageOverflow();
        
        private:
            AlignedVector<FrontyardBucketType, 64> frontyard;
            // std::vector<FrontyardBucketType> frontyard;
            AlignedVector<BackyardBucketType, 64> backyard;
            // std::vector<BackyardBucketType> backyard;

    };


    // template<std::size_t SizeRemainders, std::size_t BucketNumMiniBuckets, std::size_t FrontyardBucketCapacity, std::size_t BackyardBucketCapacity, std::size_t FrontyardToBackyardRatio, std::size_t FrontyardBucketSize, std::size_t BackyardBucketSize, bool FastSQuery>
    // class PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, true>: public PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, false> {
    //     static_assert(!Threaded);
    //     public:
    //         using PartitionQuotientFilter<SizeRemainders, BucketNumMiniBuckets, FrontyardBucketCapacity, BackyardBucketCapacity, FrontyardToBackyardRatio, FrontyardBucketSize, BackyardBucketSize, FastSQuery, false>::PartitionQuotientFilter;
    //         PartitionQuotientFilter& operator=(const PartitionQuotientFilter& a) = delete;
    //         PartitionQuotientFilter(PartitionQuotientFilter&& a) = delete;
    //         PartitionQuotientFilter& operator=(PartitionQuotientFilter&& a) = delete;

    //         //Deletion is still default but here its important to not delete until you're actually done operating with all your threads!
    // };
}

#endif