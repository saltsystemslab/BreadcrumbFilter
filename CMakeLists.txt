cmake_minimum_required(VERSION 3.10)

project(BenchFilter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-march=native -pthread")

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)


set(DPF_FILES 
    src/DynamicPrefixFilter.cpp src/TestUtility include/Bucket.hpp include/DynamicPrefixFilter.hpp
    include/MiniFilter.hpp include/QRContainers.hpp include/RemainderStore)

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

add_library(vqf OBJECT IMPORTED)
set_property(TARGET vqf PROPERTY IMPORTED_OBJECTS
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/vqf_filter.o
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/shuffle_matrix_512.o
  ${PROJECT_SOURCE_DIR}/test/vqf/obj/shuffle_matrix_512_16.o
)
# set (VQF_FILES
#    test/vqf/src/shuffle_matrix_256.c test/vqf/src/shuffle_matrix_512_16.c test/vqf/src/shuffle_matrix_512.c test/vqf/src/vqf_filter.c
#    test/vqf/include/vqf_filter.h test/vqf/include/vqf_precompute.h test/vqf/include/vqf_wrapper.h)

add_executable(BenchFilter ${DPF_FILES} ${PF_FILES} ${TC_FILES} ${PF_TESTS_FILES} ${TC_SHORTCUT_FILES} test/BenchFilter.cpp)

if( supported )
    message(STATUS "IPO / LTO enabled")
    set_property(TARGET BenchFilter PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO / LTO not supported: <${error}>")
endif()

# target_compile_options(BenchFilter PRIVATE -O3 -DNDEBUG -Wall)
target_compile_options(BenchFilter PRIVATE -O3 -Wall)
target_link_libraries(BenchFilter PRIVATE vqf)
target_include_directories(BenchFilter PUBLIC "include" "test/vqf/include" "test/Prefix-Filter/Tests" "test/Prefix-Filter/Prefix-Filter" "test/Prefix-Filter" "test/Prefix-Filter/TC-Shortcut")