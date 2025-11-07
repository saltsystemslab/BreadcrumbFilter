#ifndef WRAPPERS_HPP
#define WRAPPERS_HPP


#include "gqf.h"
#include "gqf_int.h"

#include "vqf_filter.h"

#ifdef __AVX512BW__
#include "min_pd256.hpp"
#include "tc-sym.hpp"
#include "TC-shortcut.hpp"
#include "wrappers.hpp"
#include "simd-block-fixed-fpp.h"
#endif

#include "cuckoofilter.h"
#include "morton_sample_configs.h"
#include <immintrin.h>
#include <algorithm>

//The methods in this function were copied from main.cc in VQF
class VQFWrapper {
    size_t nslots;
    vqf_filter *filter;
    // static constexpr size_t
    // QUQU_SLOTS_PER_BLOCK = 48; //Defined in vqf_filter.cpp so just copied from here. However there its defined based on remainder size, but here we just assume 8

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
        // //Copied from vqf_filter.c
        // uint64_t total_blocks = (nslots + QUQU_SLOTS_PER_BLOCK) / QUQU_SLOTS_PER_BLOCK;
        // uint64_t total_size_in_bytes = sizeof(vqf_block) * total_blocks;
        // return total_size_in_bytes;
        return filter->metadata.total_size_in_bytes;
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
        std::cout << "nslots " << nslots << std::endl;
        // filter = new QF;
        if (!qf_malloc(&filter, nslots, tot_bits, 0, QF_HASH_NONE, 0)) {
            fprintf(stderr, "Creation of CQF failed");
            exit(EXIT_FAILURE);
        }
        range = 1ull << tot_bits;
    }

    CQFWrapper(const CQFWrapper& a, const CQFWrapper& b){
        nslots = a.filter.metadata->nslots + b.filter.metadata->nslots;
        // fprintf(stdout, "nslots %lu", nslots);
        std::cout << "nslots " << nslots << std::endl;
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

#ifdef __AVX512BW__

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

#endif

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









//PQF Types
template<typename FT, const char* n>
struct PQF_Wrapper_SingleT {
    using type = FT;
    static constexpr std::string_view name{n};
    static constexpr bool threaded = false;
    static constexpr bool canBatch = true;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = true;
};

#ifdef __AVX512BW__

static const char PQF_8_22_Wrapper_str[] = "PQF_8_22";
using PQF_8_22_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22, PQF_8_22_Wrapper_str>;
static const char PQF_8_3_Wrapper_str[] = "PQF_8_3";
using PQF_8_3_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_3, PQF_8_3_Wrapper_str>;
static const char PQF_8_22_FRQ_Wrapper_str[] = "PQF_8_22_FRQ";
using PQF_8_22_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22_FRQ, PQF_8_22_FRQ_Wrapper_str>;
static const char PQF_8_22BB_Wrapper_str[] = "PQF_8_22BB";
using PQF_8_22BB_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22BB, PQF_8_22BB_Wrapper_str>;
static const char PQF_8_22BB_FRQ_Wrapper_str[] = "PQF_8_22BB_FRQ";
using PQF_8_22BB_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22BB_FRQ, PQF_8_22BB_FRQ_Wrapper_str>;

static const char PQF_8_31_Wrapper_str[] = "PQF_8_31";
using PQF_8_31_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_31, PQF_8_31_Wrapper_str>;
static const char PQF_8_31_FRQ_Wrapper_str[] = "PQF_8_31_FRQ";
using PQF_8_31_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_31_FRQ, PQF_8_31_FRQ_Wrapper_str>;

static const char PQF_8_62_Wrapper_str[] = "PQF_8_62";
using PQF_8_62_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_62, PQF_8_62_Wrapper_str>;
static const char PQF_8_62_FRQ_Wrapper_str[] = "PQF_8_62_FRQ";
using PQF_8_62_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_62_FRQ, PQF_8_62_FRQ_Wrapper_str>;

static const char PQF_8_53_Wrapper_str[] = "PQF_8_53";
using PQF_8_53_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_53, PQF_8_53_Wrapper_str>;
static const char PQF_8_53_FRQ_Wrapper_str[] = "PQF_8_53_FRQ";
using PQF_8_53_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_53_FRQ, PQF_8_53_FRQ_Wrapper_str>;

static const char PQF_16_36_Wrapper_str[] = "PQF_16_36";
using PQF_16_36_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_16_36, PQF_16_36_Wrapper_str>;
static const char PQF_16_36_FRQ_Wrapper_str[] = "PQF_16_36_FRQ";
using PQF_16_36_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_16_36_FRQ, PQF_16_36_FRQ_Wrapper_str>;

#else
static const char PQF_8_22_Wrapper_str[] = "PQF_8_22_AVX2";
using PQF_8_22_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22, PQF_8_22_Wrapper_str>;
static const char PQF_8_3_Wrapper_str[] = "PQF_8_3_AVX2";
using PQF_8_3_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_3, PQF_8_3_Wrapper_str>;
static const char PQF_8_22_FRQ_Wrapper_str[] = "PQF_8_22_FRQ_AVX2";
using PQF_8_22_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22_FRQ, PQF_8_22_FRQ_Wrapper_str>;
static const char PQF_8_22BB_Wrapper_str[] = "PQF_8_22BB_AVX2";
using PQF_8_22BB_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22BB, PQF_8_22BB_Wrapper_str>;
static const char PQF_8_22BB_FRQ_Wrapper_str[] = "PQF_8_22BB_FRQ_AVX2";
using PQF_8_22BB_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_22BB_FRQ, PQF_8_22BB_FRQ_Wrapper_str>;

static const char PQF_8_31_Wrapper_str[] = "PQF_8_31_AVX2";
using PQF_8_31_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_31, PQF_8_31_Wrapper_str>;
static const char PQF_8_31_FRQ_Wrapper_str[] = "PQF_8_31_FRQ_AVX2";
using PQF_8_31_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_31_FRQ, PQF_8_31_FRQ_Wrapper_str>;

static const char PQF_8_62_Wrapper_str[] = "PQF_8_62_AVX2";
using PQF_8_62_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_62, PQF_8_62_Wrapper_str>;
static const char PQF_8_62_FRQ_Wrapper_str[] = "PQF_8_62_FRQ_AVX2";
using PQF_8_62_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_62_FRQ, PQF_8_62_FRQ_Wrapper_str>;

static const char PQF_8_53_Wrapper_str[] = "PQF_8_53_AVX2";
using PQF_8_53_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_53, PQF_8_53_Wrapper_str>;
static const char PQF_8_53_FRQ_Wrapper_str[] = "PQF_8_53_FRQ_AVX2";
using PQF_8_53_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_8_53_FRQ, PQF_8_53_FRQ_Wrapper_str>;

static const char PQF_16_36_Wrapper_str[] = "PQF_16_36_AVX2";
using PQF_16_36_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_16_36, PQF_16_36_Wrapper_str>;
static const char PQF_16_36_FRQ_Wrapper_str[] = "PQF_16_36_FRQ_AVX2";
using PQF_16_36_FRQ_Wrapper = PQF_Wrapper_SingleT<PQF::PQF_16_36_FRQ, PQF_16_36_FRQ_Wrapper_str>;
#endif


template<typename FT, const char* n>
struct PQF_Wrapper_MultiT {
    using type = FT;
    static constexpr std::string_view name{n};
    static constexpr bool threaded = true;
    static constexpr bool onlyInsertsThreaded = false;
    static constexpr bool canBatch = true;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false; //set to true if implement multithreaded merging
};

#ifdef __AVX512BW__
static const char PQF_8_21_T_Wrapper_str[] = "PQF_8_21_T";
using PQF_8_21_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_21_T, PQF_8_21_T_Wrapper_str>;
static const char PQF_8_21_FRQ_T_Wrapper_str[] = "PQF_8_21_FRQ_T";
using PQF_8_21_FRQ_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_21_FRQ_T, PQF_8_21_FRQ_T_Wrapper_str>;

static const char PQF_8_52_T_Wrapper_str[] = "PQF_8_52_T";
using PQF_8_52_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_52_T, PQF_8_52_T_Wrapper_str>;
static const char PQF_8_52_FRQ_T_Wrapper_str[] = "PQF_8_52_FRQ_T";
using PQF_8_52_FRQ_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_52_FRQ_T, PQF_8_52_FRQ_T_Wrapper_str>;

static const char PQF_16_35_T_Wrapper_str[] = "PQF_16_35_T";
using PQF_16_35_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_16_35_T, PQF_16_35_T_Wrapper_str>;
static const char PQF_16_35_FRQ_T_Wrapper_str[] = "PQF_16_35_FRQ_T";
using PQF_16_35_FRQ_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_16_35_FRQ_T, PQF_16_35_FRQ_T_Wrapper_str>;

#else

static const char PQF_8_21_T_Wrapper_str[] = "PQF_8_21_T_AVX2";
using PQF_8_21_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_21_T, PQF_8_21_T_Wrapper_str>;
static const char PQF_8_21_FRQ_T_Wrapper_str[] = "PQF_8_21_FRQ_T_AVX2";
using PQF_8_21_FRQ_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_21_FRQ_T, PQF_8_21_FRQ_T_Wrapper_str>;

static const char PQF_8_52_T_Wrapper_str[] = "PQF_8_52_T_AVX2";
using PQF_8_52_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_52_T, PQF_8_52_T_Wrapper_str>;
static const char PQF_8_52_FRQ_T_Wrapper_str[] = "PQF_8_52_FRQ_T_AVX2";
using PQF_8_52_FRQ_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_8_52_FRQ_T, PQF_8_52_FRQ_T_Wrapper_str>;

static const char PQF_16_35_T_Wrapper_str[] = "PQF_16_35_T_AVX2";
using PQF_16_35_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_16_35_T, PQF_16_35_T_Wrapper_str>;
static const char PQF_16_35_FRQ_T_Wrapper_str[] = "PQF_16_35_FRQ_T_AVX2";
using PQF_16_35_FRQ_T_Wrapper = PQF_Wrapper_MultiT<PQF::PQF_16_35_FRQ_T, PQF_16_35_FRQ_T_Wrapper_str>;

#endif


#ifdef __AVX512BW__
//PF Types
struct PF_TC_Wrapper {
    // using type = PFFilterAPIWrapper<Prefix_Filter<TC_shortcut>, sizePF<TC_shortcut, sizeTC>, false>;
    using type = PFFilterAPIWrapper<Prefix_Filter<TC_shortcut>, loadFactorMultiplierPF<TC_shortcut>, false>;
    static constexpr std::string_view name = "PF_TC";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = false;
    static constexpr bool canMerge = false;
};

using CF12_Flex = cuckoofilter::CuckooFilterStable<uint64_t, 12>;

struct CF12_Flex_Wrapper {
    using type = cuckoofilter::CuckooFilterStable<uint64_t, 12>;
    static constexpr std::string_view name = "CF12_Flex";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

struct PF_CFF12_Wrapper {
    // using type = PFFilterAPIWrapper<Prefix_Filter<CF12_Flex>, sizePF<CF12_Flex_Wrapper::type, sizeCFF>>;
    using type = PFFilterAPIWrapper<Prefix_Filter<CF12_Flex>, loadFactorMultiplierPF<CF12_Flex>>;
    static constexpr std::string_view name = "PF_CFF12";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = false;
    static constexpr bool canMerge = false;
};

struct PF_BBFF_Wrapper {
    // using type = PFFilterAPIWrapper<Prefix_Filter<SimdBlockFilterFixed<>>, sizePF<SimdBlockFilterFixed<>, sizeBBFF>>;
    using type = PFFilterAPIWrapper<Prefix_Filter<SimdBlockFilterFixed<>>, loadFactorMultiplierPF<SimdBlockFilterFixed<>>>;
    static constexpr std::string_view name = "PF_BBFF";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = false;
    static constexpr bool canMerge = false;
};

struct TC_Wrapper {
    // using type = PFFilterAPIWrapper<TC_shortcut, sizeTC, true>;
    using type = PFFilterAPIWrapper<TC_shortcut, loadFactorMultiplierTC, true>;
    static constexpr std::string_view name = "TC";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

struct CFF12_Wrapper {
    // using type = PFFilterAPIWrapper<CF12_Flex, sizeCFF, true>;
    using type = PFFilterAPIWrapper<CF12_Flex, loadFactorMultiplierCFF, true>;
    static constexpr std::string_view name = "CFF12";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

struct BBFF_Wrapper {
    // using type = PFFilterAPIWrapper<SimdBlockFilterFixed<>, sizeBBFF>;
    using type = PFFilterAPIWrapper<SimdBlockFilterFixed<>, loadFactorMultiplierBBFF>;
    static constexpr std::string_view name = "BBFF";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = false;
    static constexpr bool canMerge = false;
};

#endif


//Original cuckoo filter
struct OriginalCF8_Wrapper {
    using type = CuckooWrapper<size_t, 8>;
    static constexpr std::string_view name = "OriginalCF8";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

struct OriginalCF12_Wrapper {
    using type = CuckooWrapper<size_t, 12>;
    static constexpr std::string_view name = "OriginalCF12";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

struct OriginalCF16_Wrapper {
    using type = CuckooWrapper<size_t, 16>;
    static constexpr std::string_view name = "OriginalCF16";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};


//Morton
struct Morton3_12_Wrapper {
    using type = MortonWrapper<CompressedCuckoo::Morton3_12>;
    static constexpr std::string_view name = "Morton3_12";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = true;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};
struct Morton3_18_Wrapper {
    using type = MortonWrapper<CompressedCuckoo::Morton3_18>;
    static constexpr std::string_view name = "Morton3_18";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = true;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};


#ifdef __AVX512BW__
//VQF
struct VQF_Wrapper {
    using type = VQFWrapper; //kinda bad
    static constexpr std::string_view name = "VQF";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

//not a real different one from VQF, but just for the output distinction. VQF needs macro change, so we just recompile the VQF for a threaded test
struct VQFT_Wrapper {
    using type = VQFWrapper; //kinda bad
    static constexpr std::string_view name = "VQFT";
    static constexpr bool threaded = true;
    static constexpr bool onlyInsertsThreaded = true;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

//not a real different one from VQF, but just for the output distinction. Again, for 16 bits VQF needs macro change, so this requires being careful and making sure you're compiling the right version
struct VQF16_Wrapper {
    using type = VQFWrapper; //kinda bad
    static constexpr std::string_view name = "VQF16";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = false;
    static constexpr bool canMerge = false;
};

#else

//VQF
struct VQF_Wrapper {
    using type = VQFWrapper; //kinda bad
    static constexpr std::string_view name = "VQF_AVX2";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

//not a real different one from VQF, but just for the output distinction. VQF needs macro change, so we just recompile the VQF for a threaded test
struct VQFT_Wrapper {
    using type = VQFWrapper; //kinda bad
    static constexpr std::string_view name = "VQFT_AVX2";
    static constexpr bool threaded = true;
    static constexpr bool onlyInsertsThreaded = true;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = false;
};

#endif

#ifdef __AVX512BW__

struct BBF_Wrapper {
    using type = BBFWrapper;
    static constexpr std::string_view name = "BBFT";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = false;
    static constexpr bool canMerge = false;
};

#endif

struct CQF_Wrapper {
    using type = CQFWrapper;
    static constexpr std::string_view name = "CQF";
    static constexpr bool threaded = false;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = true;
};

//not a real different one from CQF, but just for the output distinction. CQF needs macro change, so we just recompile the CQF for a threaded test
struct CQFT_Wrapper {
    using type = CQFWrapper;
    static constexpr std::string_view name = "CQFT";
    static constexpr bool threaded = true;
    static constexpr bool onlyInsertsThreaded = true;
    static constexpr bool canBatch = false;
    static constexpr bool canDelete = true;
    static constexpr bool canMerge = true;
};

#endif
