cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER /usr/bin/gcc-11)
set(CMAKE_CXX_COMPILER /usr/bin/g++-11)
project(BenchFilter)

find_package(OpenSSL REQUIRED)

# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-march=native -pthread")

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

set(DPF_FILES 
    src/TestUtility.cpp include/Bucket.hpp include/PartitionQuotientFilter.hpp
    include/MiniFilter.hpp include/QRContainers.hpp include/RemainderStore.hpp)

set(PF_FILES
    test/Prefix-Filter/Prefix-Filter/Shift_op.hpp test/Prefix-Filter/Prefix-Filter/Shift_op.cpp
    test/Prefix-Filter/Prefix-Filter/min_pd256.hpp test/Prefix-Filter/Prefix-Filter/min_pd256.cpp
    test/Prefix-Filter/Bloom_Filter/simd-block-fixed-fpp.h
)

set(TC_FILES
    test/Prefix-Filter/TC-Shortcut/tc-sym.hpp
    test/Prefix-Filter/TC-Shortcut/tc-sym.cpp
    test/Prefix-Filter/TC-Shortcut/TC-shortcut.hpp)

set(PF_TESTS_FILES 
    test/Prefix-Filter/Tests/wrappers.hpp)

set(TC_SHORTCUT_FILES
    test/Prefix-Filter/TC-Shortcut/tc-sym.hpp
    test/Prefix-Filter/TC-Shortcut/tc-sym.cpp
    test/Prefix-Filter/TC-Shortcut/TC-shortcut.hpp)

set(CUCKOO_FILES
    test/cuckoofilter/src/cuckoofilter.h
    )

set(MORTON_FILES
    test/morton_filter/morton_sample_configs.h
    )

add_library(vqf OBJECT IMPORTED)
add_library(PQF ${DPF_FILES})
target_include_directories(PQF PRIVATE ${PROJECT_SOURCE_DIR}/include, test/cqf/include)


set_property(TARGET vqf PROPERTY IMPORTED_OBJECTS
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/vqf_filter.o
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/shuffle_matrix_512.o
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/shuffle_matrix_512_16.o
)

add_executable(Tester ${DPF_FILES} ${PF_FILES} ${TC_FILES} ${PF_TESTS_FILES} ${TC_SHORTCUT_FILES} ${CUCKOO_FILES} ${MORTON_FILES} test/Tester.cpp test/Tester.hpp test/Config.hpp test/Config.cpp test/TestWrappers.hpp)
target_include_directories(Tester PRIVATE ${PROJECT_SOURCE_DIR}/include)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET Tester PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

target_compile_options(Tester PRIVATE -g -Wall)
target_link_libraries(Tester PRIVATE vqf)
target_include_directories(Tester PUBLIC "test" "include" "test/vqf/include" "test/Prefix-Filter/Tests" "test/Prefix-Filter/Prefix-Filter" "test/Prefix-Filter" "test/Prefix-Filter/TC-Shortcut" "test/cuckoofilter/src" "test/morton_filter", "test/Prefix-Filter/Bloom_Filter", "test/cqf/include")
