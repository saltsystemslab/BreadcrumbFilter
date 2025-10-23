#pragma once

#include <functional>
#include <string_view>
#include <string>
#include <utility>
#include <random>
#include <thread>
#include <iostream>

//returns microseconds
double runTest(std::function<void(void)> t);

std::vector <size_t> splitRange(size_t start, size_t end, size_t numSegs);

std::mt19937_64 createGenerator();

template<typename FT>
size_t generateKey(const FT &filter, std::mt19937_64 &generator) {
    std::uniform_int_distribution dist(0ull, filter.range - 1ull);
    return dist(generator);
}

template<typename FT>
std::vector <size_t> generateKeys(const FT &filter, size_t N, size_t NumThreads = 32) {
    if (NumThreads > 1) {
        std::vector <size_t> keys(N);
        std::vector <size_t> threadKeys = splitRange(0, N, NumThreads);
        std::vector <std::thread> threads;
        for (size_t i = 0; i < NumThreads; i++) {
            threads.push_back(std::thread([&, i] {
                auto generator = createGenerator();
                for (size_t j = threadKeys[i]; j < threadKeys[i + 1]; j++) {
                    keys[j] = generateKey<FT>(filter, generator);
                }
            }));
        }
        for (auto &th: threads) {
            th.join();
        }
        return keys;
    } else {
        std::vector <size_t> keys;
        auto generator = createGenerator();
        for (size_t i = 0; i < N; i++) {
            keys.push_back(generateKey<FT>(filter, generator));
        }
        return keys;
    }
}

template<typename FT>
bool insertItems(FT &filter, const std::vector <size_t> &keys, size_t start, size_t end, std::string name = "") {
    for (size_t i{start}; i < end; i++) {
        // if (name == "CQF") {
        //     std::cout << keys[i] << " " << i << std::endl;
        // }
        if (!filter.insert(keys[i])) {
            std::cerr << "Tried to insert key " << keys[i] << std::endl;
            return false;
        }
        //std::cerr << "Inserted key " << keys[i];
    }
    return true;
}