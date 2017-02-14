//
// Created by 董宏昌 on 2017/2/13.
//

#include "KaleidoscopeJIT.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"


KaleidoscopeJIT::KaleidoscopeJIT()
: m_targetMachine(llvm::EngineBuilder().selectTarget()),
  m_dataLayout(m_targetMachine->createDataLayout()),
  m_compileLayer(m_objectLayer, llvm::orc::SimpleCompiler(*m_targetMachine)),
  m_optimizeLayer(m_compileLayer, [this](std::unique_ptr<llvm::Module> M) {
      return this->optimizeModule(std::move(M));
  }),
  m_compileCallbackMgr(llvm::orc::createLocalCompileCallbackManager(m_targetMachine->getTargetTriple(), 0))
{
    auto indirectStubsMgrBuilder = llvm::orc::createLocalIndirectStubsManagerBuilder(m_targetMachine->getTargetTriple());
    m_indirectStubsMgr = indirectStubsMgrBuilder();
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

KaleidoscopeJIT::~KaleidoscopeJIT() {

}

llvm::TargetMachine& KaleidoscopeJIT::getTargetMachine() {
    return *m_targetMachine;
}

KaleidoscopeJIT::ModuleHandleT KaleidoscopeJIT::addModule(std::unique_ptr<llvm::Module> module) {
    // Build our symbol resolver:
    // Lambda 1: Look back into the JIT itself to find symbols that are part of
    //           the same "logical dylib".
    // Lambda 2: Search for external symbols in the host process.
    auto Resolver = llvm::orc::createLambdaResolver(
            [&](const std::string &aName) {
                if (auto sym = m_optimizeLayer.findSymbol(aName, false)) {
                    return llvm::RuntimeDyld::SymbolInfo(sym.getAddress(), sym.getFlags());
                }

                return llvm::RuntimeDyld::SymbolInfo(nullptr);
            },
            [](const std::string &aName) {
                if (auto symAddr = llvm::RTDyldMemoryManager::getSymbolAddressInProcess(aName)) {
                    return llvm::RuntimeDyld::SymbolInfo(symAddr, llvm::JITSymbolFlags::Exported);
                }

                return llvm::RuntimeDyld::SymbolInfo(nullptr);
            });

    // Build a singlton module set to hold our module.
    std::vector<std::unique_ptr<llvm::Module>> modules;
    modules.push_back(std::move(module));

    // Add the set to the JIT with the resolver we created above and a newly
    // created SectionMemoryManager.
    return m_optimizeLayer.addModuleSet(std::move(modules), llvm::make_unique<llvm::SectionMemoryManager>(), std::move(Resolver));
}

void KaleidoscopeJIT::removeModule(KaleidoscopeJIT::ModuleHandleT aModule) {
    m_optimizeLayer.removeModuleSet(aModule);
}

llvm::orc::JITSymbol KaleidoscopeJIT::findSymbol(const std::string aName) {
    std::string mangledName;
    llvm::raw_string_ostream mangledNameStream(mangledName);
    llvm::Mangler::getNameWithPrefix(mangledNameStream, aName, m_dataLayout);
    return m_optimizeLayer.findSymbol(mangledNameStream.str(), true);
}

std::unique_ptr<llvm::Module> KaleidoscopeJIT::optimizeModule(std::unique_ptr<llvm::Module> module) {
    // Create a function pass manager.
    auto FPM = llvm::make_unique<llvm::legacy::FunctionPassManager>(module.get());

    // Add some optimizations.
    FPM->add(llvm::createInstructionCombiningPass());
    FPM->add(llvm::createReassociatePass());
    FPM->add(llvm::createGVNPass());
    FPM->add(llvm::createCFGSimplificationPass());
    FPM->doInitialization();

    // Run the optimizations over all functions in the module being added to
    // the JIT.
    for (auto &F : *module) {
        FPM->run(F);
    }

    return module;
}
