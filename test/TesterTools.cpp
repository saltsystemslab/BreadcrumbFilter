#include "TesterTools.hpp"

std::vector <size_t> splitRange(size_t start, size_t end, size_t numSegs) {
    if (numSegs == 0) { //bad code lol
        if (start != end) {
            std::cerr << "0 segs and start is not end!!" << std::endl;
        }
        return std::vector < size_t > {start};
    }
    std::vector <size_t> ans(numSegs + 1);
    for (size_t i = 0; i <= numSegs; i++) {
        ans[i] = start + (end - start) * i / numSegs;
    }
    return ans;
}

std::mt19937_64 createGenerator() {
    return std::mt19937_64(std::random_device()());
}

std::vector<uint8_t> genBytes(size_t N, size_t NumThreads) {
    std::vector<uint64_t> llongs = generateKeys(FakeFilter{}, (N + 7) / 8, NumThreads);
    std::vector<uint8_t> bytes(llongs.size() * sizeof(uint64_t));
    std::memcpy(bytes.data(), llongs.data(), bytes.size());
    return bytes;
}