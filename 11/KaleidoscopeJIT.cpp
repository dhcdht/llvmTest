//
// Created by 董宏昌 on 2017/2/13.
//

#include "KaleidoscopeJIT.h"


KaleidoscopeJIT::KaleidoscopeJIT()
: m_targetMachine(llvm::EngineBuilder().selectTarget()),
  m_dataLayout(m_targetMachine->createDataLayout()),
  m_compileLayer(m_objectLayer, llvm::orc::SimpleCompiler(*m_targetMachine))
{
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
                if (auto sym = m_compileLayer.findSymbol(aName, false)) {
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
    return m_compileLayer.addModuleSet(std::move(modules), llvm::make_unique<llvm::SectionMemoryManager>(), std::move(Resolver));
}

void KaleidoscopeJIT::removeModule(KaleidoscopeJIT::ModuleHandleT aModule) {
    m_compileLayer.removeModuleSet(aModule);
}

llvm::orc::JITSymbol KaleidoscopeJIT::findSymbol(const std::string aName) {
    std::string mangledName;
    llvm::raw_string_ostream mangledNameStream(mangledName);
    llvm::Mangler::getNameWithPrefix(mangledNameStream, aName, m_dataLayout);
    return m_compileLayer.findSymbol(mangledNameStream.str(), true);
}
