cmake_minimum_required(VERSION 3.6)

project(llvmTest4)

set(CMAKE_CXX_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
#add_definitions(${LLVM_DEFINITIONS})
#include_directories(${LLVM_INCLUDE_DIRS})
#link_directories(${LLVM_LIBRARY_DIRS})
llvm_map_components_to_libnames(llvm_libs core)
link_libraries(${llvm_libs})

set(SOURCE_FILES
        toy.cpp
        ExprAST.cpp
        ExprAST.h)

add_executable(llvmTest4 ${SOURCE_FILES})
