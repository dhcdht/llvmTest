//
// Created by 董宏昌 on 2017/2/13.
//

#ifndef PROJECT_KALEIDOSCOPEJIT_H
#define PROJECT_KALEIDOSCOPEJIT_H


#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/LambdaResolver.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/DynamicLibrary.h"


class KaleidoscopeJIT {
private:
    std::unique_ptr<llvm::TargetMachine> m_targetMachine;
    const llvm::DataLayout m_dataLayout;
    llvm::orc::ObjectLinkingLayer<> m_objectLayer;
    llvm::orc::IRCompileLayer<decltype(m_objectLayer)> m_compileLayer;

    typedef std::function<std::unique_ptr<llvm::Module>(std::unique_ptr<llvm::Module>)>
            m_optimizeFunction;

    llvm::orc::IRTransformLayer<decltype(m_compileLayer), m_optimizeFunction> m_optimizeLayer;

    std::unique_ptr<llvm::orc::JITCompileCallbackManager> m_compileCallbackMgr;
    std::unique_ptr<llvm::orc::IndirectStubsManager> m_indirectStubsMgr;

public:
    typedef decltype(m_optimizeLayer)::ModuleSetHandleT ModuleHandleT;
    KaleidoscopeJIT();
    ~KaleidoscopeJIT();

    llvm::TargetMachine &getTargetMachine();

    KaleidoscopeJIT::ModuleHandleT addModule(std::unique_ptr<llvm::Module> module);
    void removeModule(KaleidoscopeJIT::ModuleHandleT aModule);

    llvm::orc::JITSymbol findSymbol(const std::string aName);

    std::unique_ptr<llvm::Module> optimizeModule(std::unique_ptr<llvm::Module> module);
};


#endif //PROJECT_KALEIDOSCOPEJIT_H
