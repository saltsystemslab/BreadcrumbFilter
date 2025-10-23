#ifndef TESTER_HPP
#define TESTER_HPP

#include <functional>
#include <string_view>
#include <string>
#include <utility>
#include <random>
#include "PartitionQuotientFilter.hpp"
#include "TestWrappers.hpp"

// double runTest(std::function<void(void)> t);

// std::vector<size_t> splitRange(size_t start, size_t end, size_t numSegs);
// std::mt19937_64 createGenerator();

// template<typename FT>
// size_t generateKey(const FT& filter, std::mt19937_64& generator);

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




#endif
