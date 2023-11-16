cmake_minimum_required(VERSION 3.10)

project(BenchFilter)

find_package(OpenSSL REQUIRED)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-march=native -pthread")

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)


set(DPF_FILES 
    src/DynamicPrefixFilter.cpp src/TestUtility include/Bucket.hpp include/DynamicPrefixFilter.hpp
    include/MiniFilter.hpp include/QRContainers.hpp include/RemainderStore include/TinyTable.hpp)

set(PF_FILES
    test/Prefix-Filter/Prefix-Filter/Shift_op.hpp test/Prefix-Filter/Prefix-Filter/Shift_op.cpp
    test/Prefix-Filter/Prefix-Filter/min_pd256.hpp test/Prefix-Filter/Prefix-Filter/min_pd256.cpp
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
    # test/cuckoofilter/src/hashutil.cc
    test/cuckoofilter/src/cuckoofilter.h
    # test/cuckoofilter/src/bitsutil.h
    # test/cuckoofilter/src/debug.h
    # test/cuckoofilter/src/packedtable.h
    # test/cuckoofilter/src/permencoding.h
    # test/cuckoofilter/src/printutil.h
    # test/cuckoofilter/src/simd-block.h
    # test/cuckoofilter/src/singletable.h
    )

set(MORTON_FILES
    test/morton_filter/morton_sample_configs.h
    # test/morton_filter/bf.h
    # test/morton_filter/block.h
    # test/morton_filter/compressed_cuckoo_config.h
    # test/morton_filter/compressed_cuckoo_filter.h
    # test/morton_filter/fixed_point.h
    # test/morton_filter/hash_util.h
    # test/morton_filter/morton_filter.h
    # test/morton_filter/morton_util.h
    # test/morton_filter/test_util.h
    # test/morton_filter/util.h
    # test/morton_filter/vector_types.h
    )

add_library(vqf OBJECT IMPORTED)
set_property(TARGET vqf PROPERTY IMPORTED_OBJECTS
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/vqf_filter.o
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/shuffle_matrix_512.o
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/shuffle_matrix_512_16.o
)
# set (VQF_FILES
#    test/vqf/src/shuffle_matrix_256.c test/vqf/src/shuffle_matrix_512_16.c test/vqf/src/shuffle_matrix_512.c test/vqf/src/vqf_filter.c
#    test/vqf/include/vqf_filter.h test/vqf/include/vqf_precompute.h test/vqf/include/vqf_wrapper.h)

add_executable(BenchFilter ${DPF_FILES} ${PF_FILES} ${TC_FILES} ${PF_TESTS_FILES} ${TC_SHORTCUT_FILES} ${CUCKOO_FILES} ${MORTON_FILES} test/BenchFilter.cpp test/TestWrappers.hpp)
add_executable(ParallelBenchFilter ${DPF_FILES} ${PF_FILES} ${TC_FILES} ${PF_TESTS_FILES} ${TC_SHORTCUT_FILES} ${CUCKOO_FILES} ${MORTON_FILES} test/BenchFilter.cpp test/TestWrappers.hpp)
add_executable(FailureAsymptoticsTest ${DPF_FILES} ${PF_FILES} ${TC_FILES} ${PF_TESTS_FILES} ${TC_SHORTCUT_FILES} ${CUCKOO_FILES} ${MORTON_FILES} test/FailureAsymptoticsTest.cpp test/TestWrappers.hpp)
add_executable(InsertDeleteTest ${DPF_FILES} ${PF_FILES} ${TC_FILES} ${PF_TESTS_FILES} ${TC_SHORTCUT_FILES} ${CUCKOO_FILES} ${MORTON_FILES} test/InsertDeleteTest.cpp test/TestWrappers.hpp)
add_executable(TestPQF ${DPF_FILES} ${PF_FILES} ${TC_FILES} ${PF_TESTS_FILES} ${TC_SHORTCUT_FILES} ${CUCKOO_FILES} ${MORTON_FILES} test/TestPQF.cpp test/TestWrappers.hpp)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET BenchFilter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET ParallelBenchFilter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET FailureAsymptoticsTest PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET InsertDeleteTest PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

target_compile_options(BenchFilter PRIVATE -O3 -DNDEBUG -Wall)
target_link_libraries(BenchFilter PRIVATE vqf)
target_include_directories(BenchFilter PUBLIC "test" "include" "test/vqf/include" "test/Prefix-Filter/Tests" "test/Prefix-Filter/Prefix-Filter" "test/Prefix-Filter" "test/Prefix-Filter/TC-Shortcut" "test/cuckoofilter/src" "test/morton_filter")
target_compile_options(ParallelBenchFilter PRIVATE -O3 -DNDEBUG -Wall -DENABLE_THREADS)
target_link_libraries(ParallelBenchFilter PRIVATE vqf)
target_include_directories(ParallelBenchFilter PUBLIC "test" "include" "test/vqf/include" "test/Prefix-Filter/Tests" "test/Prefix-Filter/Prefix-Filter" "test/Prefix-Filter" "test/Prefix-Filter/TC-Shortcut" "test/cuckoofilter/src" "test/morton_filter")
target_compile_options(FailureAsymptoticsTest PRIVATE -O3 -DNDEBUG -Wall)
target_link_libraries(FailureAsymptoticsTest PRIVATE vqf)
target_include_directories(FailureAsymptoticsTest PUBLIC "test" "include" "test/vqf/include" "test/Prefix-Filter/Tests" "test/Prefix-Filter/Prefix-Filter" "test/Prefix-Filter" "test/Prefix-Filter/TC-Shortcut" "test/cuckoofilter/src" "test/morton_filter")
target_compile_options(InsertDeleteTest PRIVATE -O3 -DNDEBUG -Wall)
target_link_libraries(InsertDeleteTest PRIVATE vqf)
target_include_directories(InsertDeleteTest PUBLIC "test" "include" "test/vqf/include" "test/Prefix-Filter/Tests" "test/Prefix-Filter/Prefix-Filter" "test/Prefix-Filter" "test/Prefix-Filter/TC-Shortcut" "test/cuckoofilter/src" "test/morton_filter")

target_compile_options(TestPQF PRIVATE -O3 -Wall)
target_include_directories(TestPQF PUBLIC "test" "include" "test/vqf/include" "test/Prefix-Filter/Tests" "test/Prefix-Filter/Prefix-Filter" "test/Prefix-Filter" "test/Prefix-Filter/TC-Shortcut" "test/cuckoofilter/src" "test/morton_filter")
