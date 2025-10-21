#ifndef TESTER_HPP
#define TESTER_HPP

#include <functional>
#include <string_view>
#include <string>
#include <utility>
#include <random>
#include "PartitionQuotientFilter.hpp"
#include "TestWrappers.hpp"

double runTest(std::function<void(void)> t);

std::vector<size_t> splitRange(size_t start, size_t end, size_t numSegs);
std::mt19937_64 createGenerator();

template<typename FT>
size_t generateKey(const FT& filter, std::mt19937_64& generator);

// template<typename FT>
// bool insertItems(FT& filter, const std::vector<size_t>& keys, size_t start, size_t end, std::string name = "");

template<typename FT>
bool checkQuery(FT& filter, const std::vector<size_t>& keys, size_t start, size_t end);

template<typename FT>
size_t getNumFalsePositives(FT& filter, const std::vector<size_t>& FPRkeys, size_t start, size_t end);

template<typename FT>
bool removeItems(FT& filter, const std::vector<size_t>& keys, size_t start, size_t end);

template<typename FT>
size_t streamingInsertDeleteTest(FT& filter, std::vector<size_t>& keysInFilter, std::mt19937_64& generator, size_t maxKeyCount);

template<typename FT>
size_t randomInsertDeleteTest(FT& filter, std::vector<size_t>& keysInFilter, std::mt19937_64& generator, size_t maxKeyCount);

template<typename ...FTWrappers>
void runTests(std::string configFile, std::string );




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
