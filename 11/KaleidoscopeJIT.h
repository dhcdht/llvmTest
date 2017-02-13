//
// Created by 董宏昌 on 2017/2/13.
//

#ifndef PROJECT_KALEIDOSCOPEJIT_H
#define PROJECT_KALEIDOSCOPEJIT_H


#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"


class KaleidoscopeJIT {
private:
    std::unique_ptr<llvm::TargetMachine> m_targetMachine;
    const llvm::DataLayout m_dataLayout;
    llvm::orc::ObjectLinkingLayer<> m_objectLayer;
    llvm::orc::IRCompileLayer<decltype(m_objectLayer)> m_compileLayer;

public:
    typedef decltype(m_compileLayer)::ModuleSetHandleT ModuleHandleT;
    KaleidoscopeJIT();
    ~KaleidoscopeJIT();

    llvm::TargetMachine &getTargetMachine();

    KaleidoscopeJIT::ModuleHandleT addModule(std::unique_ptr<llvm::Module> module);
    void removeModule(KaleidoscopeJIT::ModuleHandleT aModule);

    llvm::orc::JITSymbol findSymbol(const std::string aName);
};


#endif //PROJECT_KALEIDOSCOPEJIT_H
