#include "TestUtility.hpp"

void printBinaryUInt64(uint64_t x, bool newline, int divider) {
    for(int i=0; i < 64; i++) {
        if(i>0 && i%divider == 0) std::cout << ' ';
        std::cout << (x%2);
        x/=2;
    }
    if(newline) std::cout << std::endl;
}

// void print_vec(__m512i X, bool binary, int divider /*=64*/) { //either prints binary or hex in little endian fashion because of weirdness
//     if(binary) std::cout << "0b ";
//     else std::cout << "0x ";
//     for(int i=0; i < 8; i++) {
//         if(binary)
//             printBinaryUInt64(X[i], false, divider);
//         else 
//             std::cout << X[i];
//         std::cout << ' ';
//     }
//     std::cout << std::endl;
// }