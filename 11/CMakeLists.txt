cmake_minimum_required(VERSION 3.6)

project(llvmTest11)

set(CMAKE_CXX_STANDARD 11)

find_package(LLVM REQUIRED CONFIG)
#add_definitions(${LLVM_DEFINITIONS})
#include_directories(${LLVM_INCLUDE_DIRS})
#link_directories(${LLVM_LIBRARY_DIRS})
llvm_map_components_to_libnames(llvm_libs
        core
        nativecodegen
        executionengine)
link_libraries(${llvm_libs})

set(SOURCE_FILES
        toy.cpp
        ExprAST.cpp
        ExprAST.h
        ExprParser.cpp
        ExprParser.h
        KaleidoscopeJIT.cpp
        KaleidoscopeJIT.h)

add_executable(llvmTest11 ${SOURCE_FILES})
