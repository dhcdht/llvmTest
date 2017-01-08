cmake_minimum_required(VERSION 3.6)

project(llvmTest)

set(CMAKE_CXX_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
#add_definitions(${LLVM_DEFINITIONS})
#include_directories(${LLVM_INCLUDE_DIRS})
#link_directories(${LLVM_LIBRARY_DIRS})
llvm_map_components_to_libnames(llvm_libs core)

set(SOURCE_FILES
        3/toy.cpp)

add_executable(llvmTest ${SOURCE_FILES})
target_link_libraries(llvmTest ${llvm_libs})
