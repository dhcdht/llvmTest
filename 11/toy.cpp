#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include "ExprAST.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/TargetSelect.h"
#include "ExprParser.h"


int main(int argc, char const *argv[]) {

    // // 测试代码
    // {
    //     // 如何表示 x + y
    //     {
    //         auto lhs = llvm::make_unique<VariableExprAST>("x");
    //         auto rhs = llvm::make_unique<VariableExprAST>("y");
    //         auto result = llvm::make_unique<BinaryExprAST>('+', std::move(lhs), std::move(rhs));
    //     }
    // }

    // 规定各个操作符的优先级
    {
        kBinaryOPPrecedence['='] = 2;
        kBinaryOPPrecedence['<'] = 10;
        kBinaryOPPrecedence['+'] = 20;
        kBinaryOPPrecedence['-'] = 30;
        kBinaryOPPrecedence['*'] = 40;
    }

    // 初始化 llvm 环境
    initLLVMContext();
    auto parser = ExprParser();

    std::string inputString;
    // 写上初始的提示文本
    fprintf(stderr, "ready> ");
    std::getline(std::cin, inputString);
    while (inputString != "~") {
        parser.startParse(inputString);

        fprintf(stderr, "ready> ");
        std::getline(std::cin, inputString);
    }

    // dump 出当前 llvm IR 中已经生成的所有代码
    llvm::Module *module = dumpLLVMContext();

    // 初始化
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    // 查看 llvm 是否支持编译当前机器架构
    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        llvm::errs() << error;
        return 1;
    }
    module->setTargetTriple(targetTriple);

    // 设置输出机器码的格式
    auto CPU = "generic";
    auto features = "";
    llvm::TargetOptions options;
    auto rm = llvm::Optional<llvm::Reloc::Model>();
    auto targetMachine = target->createTargetMachine(targetTriple, CPU, features, options, rm);
    module->setDataLayout(targetMachine->createDataLayout());

    // 打开要输出的文件
    auto fileName = "output.o";
    std::error_code error_code;
    llvm::raw_fd_ostream dest(fileName, error_code, llvm::sys::fs::F_None);
    if (error_code) {
        llvm::errs() << "Could not open file: " << error_code.message();
        return 1;
    }

    // 设置 pass 输出到文件
    llvm::legacy::PassManager passManager;
    auto fileType = llvm::TargetMachine::CGFT_ObjectFile;
    // 这里没有写错，这个函数在成功的时候返回 false，它的注释里边有写
    if (targetMachine->addPassesToEmitFile(passManager, dest, fileType)) {
        llvm::errs() << "The target machine can't emit a file of this type";
        return 1;
    }

    // 运行 pass，写入文件
    passManager.run(*module);
    dest.flush();

    return 0;
}
