//
// Created by 董宏昌 on 2017/2/13.
//

#include "KaleidoscopeJIT.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"


/// This will compile FnAST to IR, rename the function to add the given
/// suffix (needed to prevent a name-clash with the function's stub),
/// and then take ownership of the module that the function was compiled
/// into.
std::unique_ptr<llvm::Module>
irgenAndTakeOwnership(FunctionAST &FnAST, const std::string &Suffix);


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

std::string KaleidoscopeJIT::mangle(const std::string &name) {
    std::string mangledName;
    llvm::raw_string_ostream mangledNameStream(mangledName);
    llvm::Mangler::getNameWithPrefix(mangledNameStream, name, m_dataLayout);

    return mangledNameStream.str();
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

llvm::Error KaleidoscopeJIT::addFunctionAST(std::unique_ptr<FunctionAST> functionAST) {
    // Create a CompileCallback - this is the re-entry point into the compiler
    // for functions that haven't been compiled yet.
    auto CCInfo = m_compileCallbackMgr->getCompileCallback();

    // Create an indirect stub. This serves as the functions "canonical
    // definition" - an unchanging (constant address) entry point to the
    // function implementation.
    // Initially we point the stub's function-pointer at the compile callback
    // that we just created. In the compile action for the callback (see below)
    // we will update the stub's function pointer to point at the function
    // implementation that we just implemented.
    if (auto Err = m_indirectStubsMgr->createStub(this->mangle(functionAST->getName()),
                                                CCInfo.getAddress(),
                                                llvm::JITSymbolFlags::Exported))
        return Err;

    // Move ownership of FnAST to a shared pointer - C++11 lambdas don't support
    // capture-by-move, which is be required for unique_ptr.
    auto SharedFnAST = std::shared_ptr<FunctionAST>(std::move(functionAST));

    // Set the action to compile our AST. This lambda will be run if/when
    // execution hits the compile callback (via the stub).
    //
    // The steps to compile are:
    // (1) IRGen the function.
    // (2) Add the IR module to the JIT to make it executable like any other
    //     module.
    // (3) Use findSymbol to get the address of the compiled function.
    // (4) Update the stub pointer to point at the implementation so that
    ///    subsequent calls go directly to it and bypass the compiler.
    // (5) Return the address of the implementation: this lambda will actually
    //     be run inside an attempted call to the function, and we need to
    //     continue on to the implementation to complete the attempted call.
    //     The JIT runtime (the resolver block) will use the return address of
    //     this function as the address to continue at once it has reset the
    //     CPU state to what it was immediately before the call.
    CCInfo.setCompileAction(
            [this, SharedFnAST]() {
                auto M = irgenAndTakeOwnership(*SharedFnAST, "$impl");
                addModule(std::move(M));
                auto Sym = findSymbol(SharedFnAST->getName() + "$impl");
                assert(Sym && "Couldn't find compiled function?");
                llvm::orc::TargetAddress SymAddr = Sym.getAddress();
                if (auto Err =
                        m_indirectStubsMgr->updatePointer(mangle(SharedFnAST->getName()),
                                                        SymAddr)) {
                    logAllUnhandledErrors(std::move(Err), llvm::errs(),
                                          "Error updating function pointer: ");
                    exit(1);
                }

                return SymAddr;
            });

    return llvm::Error::success();
}
