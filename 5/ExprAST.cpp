//
// Created by 董宏昌 on 2017/1/8.
//

#include "ExprAST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"


/// llvm 当前上下文对象
static llvm::LLVMContext kTheContext;
/// 用于构建各个 llvm IR 对象，比如函数等
static llvm::IRBuilder<> kBuilder(kTheContext);
/// 当前模块，当前所有函数都在同一模块中
static std::unique_ptr<llvm::Module> kTheModule;
/// 保存变量名与 llvm IR 对象的对应关系
static std::map<std::string, llvm::Value *> kNamedValue;


void initLLVMContext() {
    kTheModule = llvm::make_unique<llvm::Module>("My custom jit", kTheContext);
}

void dumpLLVMContext() {
    kTheModule->dump();
}


extern std::unique_ptr<ExprAST> logError(const char *str);
llvm::Value *logErrorV(const char *str) {
    logError(str);

    return nullptr;
}


ExprAST::~ExprAST() {

}


NumberExprAST::NumberExprAST(double val)
        : m_val(val) {

}

llvm::Value *NumberExprAST::codegen() {
    return llvm::ConstantFP::get(kTheContext, llvm::APFloat(m_val));
}


VariableExprAST::VariableExprAST(const std::string &name)
        : m_name(name) {

}

llvm::Value* VariableExprAST::codegen() {
    llvm::Value *v = kNamedValue[m_name];
    if (!v) {
        logErrorV("Unknown variable name");
    }

    return v;
}


BinaryExprAST::BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : m_op(op), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {

}

llvm::Value* BinaryExprAST::codegen() {
    llvm::Value *lhsValue = m_lhs->codegen();
    llvm::Value *rhsValue = m_rhs->codegen();
    if (!lhsValue || !rhsValue) {
        return nullptr;
    }

    switch (m_op) {
        case '+': {
            return kBuilder.CreateFAdd(lhsValue, rhsValue, "addtmp");

            break;
        }

        case '-': {
            return kBuilder.CreateFSub(lhsValue, rhsValue, "subtmp");

            break;
        }

        case '*': {
            return kBuilder.CreateFMul(lhsValue, rhsValue, "multmp");

            break;
        }

        case '<': {
            lhsValue = kBuilder.CreateFCmpULT(lhsValue, rhsValue, "cmptmp");
            // 这一步将 bool 对象 0/1 转换为 double 型的 0.0/1.0
            return kBuilder.CreateUIToFP(lhsValue, llvm::Type::getDoubleTy(kTheContext), "booltmp");

            break;
        }

        default: {
            return logErrorV("Invalid binary operator");

            break;
        }
    }
}


CallExprAST::CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args)
        : m_callee(callee), m_args(std::move(args)) {

}

llvm::Value* CallExprAST::codegen() {
    // 取的要调用的函数
    llvm::Function *calleeFunc = kTheModule->getFunction(m_callee);
    if (!calleeFunc) {
        return logErrorV("Unknown function referenced");
    }

    if (calleeFunc->arg_size() != m_args.size()) {
        return logErrorV("Incorrect # arguments passed");
    }

    // 将各个参数对应设置上
    std::vector<llvm::Value *> argsValue;
    for (unsigned long i = 0; i < m_args.size(); ++i) {
        argsValue.push_back(m_args[i]->codegen());
        if (!argsValue.back()) {
            return nullptr;
        }
    }

    // 创建函数调用的 llvm IR 代码
    return kBuilder.CreateCall(calleeFunc, argsValue, "calltmp");
}


PrototypeAST::PrototypeAST(const std::string &name, std::vector<std::string> args)
        : m_name(name), m_args(std::move(args)) {

}

std::string PrototypeAST::getName() {
    return m_name;
}

llvm::Function* PrototypeAST::codegen() {
    std::vector<llvm::Type *> doubles(m_args.size(), llvm::Type::getDoubleTy(kTheContext));
    llvm::FunctionType *functionType = llvm::FunctionType::get(llvm::Type::getDoubleTy(kTheContext), doubles,
                                                               false);
    // 在当前模块中创建函数
    llvm::Function *function = llvm::Function::Create(functionType, llvm::Function::ExternalLinkage, m_name,
                                                      kTheModule.get());

    // 设置各个参数名
    unsigned long index = 0;
    for (auto &arg : function->args()) {
        arg.setName(m_args[index++]);
    }

    return function;
}


FunctionAST::FunctionAST(std::unique_ptr<PrototypeAST> prototype, std::unique_ptr<ExprAST> body)
        : m_prototype(std::move(prototype)), m_body(std::move(body)) {

}

llvm::Function* FunctionAST::codegen() {
    // 看函数是否已经创建过，如果没有的话，创建一个
    llvm::Function *theFunction = kTheModule->getFunction(m_prototype->getName());
    if (!theFunction) {
        theFunction = m_prototype->codegen();
    }

    if (!theFunction) {
        return nullptr;
    }

    // 函数不应该被重复实现，如果原来实现过，说明有问题
    if (!theFunction->empty()) {
        return (llvm::Function*)logErrorV("Function cannot be redefined");
    }

    // 创建函数实现
    llvm::BasicBlock *basicBlock = llvm::BasicBlock::Create(kTheContext, "entry", theFunction);
    kBuilder.SetInsertPoint(basicBlock);

    // 记录参数名与其对应的 llvm::Value 对象
    kNamedValue.clear();
    for (auto &arg : theFunction->args()) {
        kNamedValue[arg.getName()] = &arg;
    }

    // 函数内部实现对应的 llvm IR 代码和返回值设定
    if (llvm::Value *retVal = m_body->codegen()) {
        kBuilder.CreateRet(retVal);

        // 使用 llvm 自带的函数验证当前的函数是否有问题
        llvm::verifyFunction(*theFunction);

        return theFunction;
    }

    // 函数实现创建有问题的时候，去掉模块中对应的函数定义
    theFunction->eraseFromParent();
    return nullptr;
}
