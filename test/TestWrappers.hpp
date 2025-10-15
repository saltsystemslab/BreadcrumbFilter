#ifndef WRAPPERS_HPP
#define WRAPPERS_HPP


#include "gqf.h"
#include "gqf_int.h"

#include "vqf_filter.h"
#include "min_pd256.hpp"
#include "tc-sym.hpp"
#include "TC-shortcut.hpp"
#include "wrappers.hpp"
// #include "cuckoofilter.h"
#include "morton_sample_configs.h"
#include "simd-block-fixed-fpp.h"
#include <immintrin.h>
#include <algorithm>

//The methods in this function were copied from main.cc in VQF
class VQFWrapper {
    size_t nslots;
    vqf_filter *filter;
    static constexpr size_t
    QUQU_SLOTS_PER_BLOCK = 48; //Defined in vqf_filter.cpp so just copied from here. However there its defined based on remainder size, but here we just assume 8

public:
    size_t range;
    // bool insertFailure;

    VQFWrapper(size_t nslots) : nslots{nslots} {
        if ((filter = vqf_init(nslots)) == NULL) {
            fprintf(stderr, "Creation of VQF failed");
            exit(EXIT_FAILURE);
        }
        // range = filter->metadata.range;
        range = -1ull;
    }

    inline bool insert(std::uint64_t hash) {
        return vqf_insert(filter, hash);
    }

    inline bool query(std::uint64_t hash) {
        return vqf_is_present(filter, hash);
    }

    inline std::uint64_t sizeFilter() {
        //Copied from vqf_filter.c
        uint64_t total_blocks = (nslots + QUQU_SLOTS_PER_BLOCK) / QUQU_SLOTS_PER_BLOCK;
        uint64_t total_size_in_bytes = sizeof(vqf_block) * total_blocks;
        return total_size_in_bytes;
    }

    inline bool remove(std::uint64_t hash) {
        return vqf_remove(filter, hash);
    }

    ~VQFWrapper() {
        free(filter);
    }
};

class CQFWrapper {
    size_t nslots;
    QF filter;
    static constexpr size_t rbits = 8;

public:
    size_t range;
    // bool insertFailure;

    CQFWrapper(size_t nslots) : nslots{nslots} {
        // roughly rounding qbits = round(log (nslots)) by doing the multiplication by 7/5
        size_t qbits = 63 - _lzcnt_u64 (nslots * 7 / 5);
        size_t tot_bits = rbits + qbits;
        // filter = new QF;
        if (!qf_malloc(&filter, nslots, tot_bits, 0, QF_HASH_NONE, 0)) {
            fprintf(stderr, "Creation of CQF failed");
            exit(EXIT_FAILURE);
        }
        range = 1ull << tot_bits;
    }

    CQFWrapper(const CQFWrapper& a, const CQFWrapper& b){
        nslots = a.filter.metadata->nslots + b.filter.metadata->nslots;
        fprintf(stdout, "nslots %lu", nslots);
        size_t tot_bits = a.filter.metadata->key_bits;
        assert(tot_bits == b.filter.metadata->key_bits);
        if (!qf_malloc(&filter, nslots, tot_bits, 0, QF_HASH_NONE, 0)) {
            fprintf(stderr, "Creation of combined CQF failed");
            exit(EXIT_FAILURE);
        }
        range = 1ull << tot_bits;
        qf_merge(&a.filter, &b.filter, &filter);
    }

    inline bool insert(std::uint64_t hash) {
        return qf_insert(&filter, hash, 0, 1, QF_NO_LOCK) >= 0;
    }

    inline bool query(std::uint64_t hash) {
        return qf_count_key_value(&filter, hash, 0, 0) != 0;
    }

    inline std::uint64_t sizeFilter() {
        return sizeof(qfmetadata) + filter.metadata->total_size_in_bytes;
    }

    inline bool remove(std::uint64_t hash) {
        return qf_remove(&filter, hash, 0, 1, QF_NO_LOCK) == 1;
    }

    ~CQFWrapper() {
        free(filter.metadata);
        // free(filter);
    }
};

//! Wrapper class from blocked bloom filter. Code borrowed from Prefix-Filter.
//! This code was "taken" from Prefix-Filter/main-built.cpp
class BBFWrapper {
    size_t n_bits;
    SimdBlockFilterFixed<> filter;

public:
    size_t range;
    //BBFWrapper(size_t n_slots): n_bits(n_slots),
    //   filter(SimdBlockFilterFixed(n_bits))	{
    //    range = -1ull;
    //}
    //

    BBFWrapper(size_t n_slots): n_bits(static_cast<size_t>(-1 * (n_slots * std::log(0.595)) / (std::pow(std::log(2), 2)))),
	       filter(SimdBlockFilterFixed(n_bits))     {
		               range = -1ull;
			           }

    ~BBFWrapper() {
    }

    inline bool insert(std::uint64_t hash) {
        filter.Add(hash);
	return true;
    }

    inline bool query(std::uint64_t hash) {
        return filter.Find(hash);
    }

    inline std::uint64_t sizeFilter() {
        return filter.SizeInBytes();
    }

    inline bool remove(std::uint64_t hash) {
        return false;
    }
};

//Taken from the respective files in prefix filter codes

//From TC_shortcut
size_t sizeTC(size_t N) {
    constexpr float load = .935;
    const size_t single_pd_capacity = tc_sym::MAX_CAP;
    return 64 * TC_shortcut::TC_compute_number_of_PD(N, single_pd_capacity, load);
}

//From stable cuckoo filter and singletable.h
template<size_t bits_per_tag = 12>
size_t sizeCFF(size_t N) {
    static const size_t kTagsPerBucket = 4;
    static const size_t kBytesPerBucket = (bits_per_tag * kTagsPerBucket + 7) >> 3;
    static const size_t kPaddingBuckets = ((((kBytesPerBucket + 7) / 8) * 8) - 1) / kBytesPerBucket;
    size_t assoc = 4;
    // bucket count needs to be even
    constexpr double load = .94;
    size_t bucketCount = (10 + N / load / assoc) / 2 * 2;
    return kBytesPerBucket * (bucketCount + kPaddingBuckets); //I think this is right?
}

//BBF-Flex is SimdBlockFilterFixed?? Seems to be by the main-perf code, so I shall stick with it
size_t sizeBBFF(size_t N) {
    unsigned long long int bits = N; //I am very unsure about this but it appears that is how the code is structured??? Size matches up anyways
    size_t bucketCount = std::max(1ull, bits / 24);
    using Bucket = uint32_t[8];
    return bucketCount * sizeof(Bucket);
}

template<typename SpareType, size_t (*SpareSpaceCalculator)(size_t)>
size_t sizePF(size_t N) {
    constexpr float loads[2] = {.95, .95};
    // constexpr float loads[2] = {1.0, 1.0};
    double frontyardSize = 32 * std::ceil(1.0 * N / (min_pd::MAX_CAP0 * loads[0]));
    static double constexpr
    overflowing_items_ratio = 0.0586;
    size_t backyardSize = SpareSpaceCalculator(get_l2_slots<SpareType>(N, overflowing_items_ratio, loads));
    return backyardSize + frontyardSize;
}


double loadFactorMultiplierTC() {
    return 0.935;
}

double loadFactorMultiplierCFF() {
    return 0.94;
}

//BBF-Flex is SimdBlockFilterFixed?? Seems to be by the main-perf code, so I shall stick with it
double loadFactorMultiplierBBFF() {
    return 1.0; //difficult to speak of load factor with BBFF anyways and we won't be measuring it
}

template<typename SpareType>
double loadFactorMultiplierPF() {
    size_t max_items = 1'000'000'000ull;
    constexpr float loads[2] = {.95, .95}; //as in the code
    static double constexpr
    overflowing_items_ratio = 0.0586;
    size_t l2Slots = get_l2_slots<SpareType>(max_items, overflowing_items_ratio, loads);
    size_t l1Slots = std::ceil(1.0 * max_items / loads[0]);
    return max_items / ((double) (l1Slots + l2Slots));
}

template<typename FilterType, double (*LFMultiplier)(), bool CanRemove = false>
class PFFilterAPIWrapper {
    // using SpareType = TC_shortcut;
    // using PrefixFilterType = Prefix_Filter<SpareType>;

    size_t N;
    FilterType filter;

public:
    size_t range;
    // bool insertFailure;

    PFFilterAPIWrapper(size_t N) : N{N}, filter{FilterAPI<FilterType>::ConstructFromAddCount(
            static_cast<size_t>(LFMultiplier() * N))} {
        range = -1ull;
    }

    inline bool insert(std::uint64_t hash) {
        FilterAPI<FilterType>::Add(hash, &filter);
        // bool success = FilterAPI<FilterType>::Add_attempt(hash, &filter); //DOES NOT EXIST IN Prefix_Filter!!!!!
        // insertFailure = !success;
        // return success;
        return true;////terrible!
    }

    inline bool query(std::uint64_t hash) {
        return FilterAPI<FilterType>::Contain(hash, &filter);
    }

    inline std::uint64_t sizeFilter() {
        //Copied from wrappers.hpp and TC-Shortcut.hpp in Prefix-Filter
        //Size of frontyard
        // return SpaceCalculator(N);
        // return filter.get_byte_size();
        return FilterAPI<FilterType>::get_byte_size(&filter);
    }

    inline bool remove(std::uint64_t hash) {
        if (CanRemove) {
            FilterAPI<FilterType>::Remove(hash, &filter);
            return true; //No indication here at all
        } else
            return false;
    }
};

template<typename ItemType, size_t bits_per_item>
class CuckooWrapper {
    size_t N;
    using FT = cuckoofilter::CuckooFilter<ItemType, bits_per_item>;
    using ST = cuckoofilter::Status;
    FT filter;

public:
    size_t range;
    // bool insertFailure;

    CuckooWrapper(size_t N) : N{N}, filter{FT(N)} {
        range = -1ull;
    }

    inline bool insert(std::uint64_t hash) {
        ST status = filter.Add(hash);
        bool failure = status == cuckoofilter::NotEnoughSpace;
        // insertFailure = !FilterAPI<FilterType>::Add_attempt(hash, &filter);
        // insertFailure = failure;
        return !failure;
    }

    inline bool query(std::uint64_t hash) {
        ST status = filter.Contain(hash);
        return status == cuckoofilter::Ok;
    }

    inline std::uint64_t sizeFilter() {
        return filter.SizeInBytes();
    }

    inline bool remove(std::uint64_t hash) {
        filter.Delete(hash);
        return true;
    }
};

template<typename FT = CompressedCuckoo::Morton3_12>
class MortonWrapper {
    size_t N;
    // using FT = CompressedCuckoo::Morton3_12;
    FT filter;

public:
    size_t range;
    // bool insertFailure;

    MortonWrapper(size_t N) : N{N}, filter{FT(N)} {
        range = -1ull;
    }

    inline bool insert(std::uint64_t hash) {
        bool failure = filter.insert(hash);
        // insertFailure = failure;
        return failure;
    }

    inline bool query(std::uint64_t hash) {
        return filter.likely_contains(hash);
    }

    inline std::uint64_t sizeFilter() {
        return filter._total_blocks *
               sizeof(*(filter._storage)); //Not actually sure this is correct, as they have no getsize function. This seems to be the main storage hog?
    }

    inline bool remove(std::uint64_t hash) {
        return filter.delete_item(hash);
    }

    inline void insertBatch(const std::vector <keys_t> &keys, std::vector<bool> &status, const uint64_t num_keys) {
        filter.insert_many(keys, status, num_keys);
    }

    inline void queryBatch(const std::vector <keys_t> &keys, std::vector<bool> &status, const uint64_t num_keys) {
        filter.likely_contains_many(keys, status, num_keys);
    }

    inline void removeBatch(const std::vector <keys_t> &keys, std::vector<bool> &status, const uint64_t num_keys) {
        filter.delete_many(keys, status, num_keys);
    }
};

#endif
