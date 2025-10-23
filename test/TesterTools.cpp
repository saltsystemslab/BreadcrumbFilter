#include "TesterTools.hpp"

double runTest(std::function<void(void)> t) {
    std::chrono::time_point <std::chrono::system_clock> startTime, endTime;
    startTime = std::chrono::system_clock::now();
    asm volatile ("":: : "memory");

    t();

    asm volatile ("":: : "memory");
    endTime = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    return duration_cast<std::chrono::microseconds>(elapsed).count();
}

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