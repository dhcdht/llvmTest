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

IfExprAST::IfExprAST(std::unique_ptr<ExprAST> condition, std::unique_ptr<ExprAST> then,
                     std::unique_ptr<ExprAST> elseExpr)
        : m_condition(std::move(condition)), m_then(std::move(then)), m_else(std::move(elseExpr)) {

}

/*
 * 分支语句 llvm IR 代码结构如下
 *       True ->    then ->
 * entry                    ifcont
 *       False ->   else ->
 * llvm 会使用 SSA 机制，即静态单赋值，意思是所有变量只能被复制一次，便于后期代码优化
 * 这样，就需要 ifcont 来根据上一步运行的是 then 还是 else 来分别处理后续的赋值步骤
 */
llvm::Value *IfExprAST::codegen() {
    llvm::Value *conditionValue = m_condition->codegen();
    if (!conditionValue) {
        return nullptr;
    }

    // 作为条件表达式，我们需要把它的值转换为 bool 类型，这里使用 0.0 来进行比较
    conditionValue = kBuilder.CreateFCmpONE(conditionValue, llvm::ConstantFP::get(kTheContext, llvm::APFloat(0.0)), "ifcondition");

    // 当前分支语句的总函数
    llvm::Function *function = kBuilder.GetInsertBlock()->getParent();

    // 分别创建 the else ifcont 的代码块，此时还没有向其中添加真是的 IR 代码
    // 第一句创建 thenBlock 时顺便将 thenBlock 挂到了 function 上
    llvm::BasicBlock *thenBlock = llvm::BasicBlock::Create(kTheContext, "then", function);
    llvm::BasicBlock *elseBlock = llvm::BasicBlock::Create(kTheContext, "else");
    llvm::BasicBlock *mergeBlock = llvm::BasicBlock::Create(kTheContext, "ifcont");

    // 当前函数的插入点上，插入执行分支语句的代码
    kBuilder.CreateCondBr(conditionValue, thenBlock, elseBlock);

    // 给 then 代码块添加 IR 代码
    kBuilder.SetInsertPoint(thenBlock);
    llvm::Value *theValue = m_then->codegen();
    if (!theValue) {
        return nullptr;
    }

    // then 代码块完成后运行 mergeBlock
    kBuilder.CreateBr(mergeBlock);
    // 此时 then 代码块的内容已经改变了，后边 phi 还要用到它，所以更新一下
    thenBlock = kBuilder.GetInsertBlock();

    // 将 elseBlock 挂到 function 上
    function->getBasicBlockList().push_back(elseBlock);
    // 给 else 代码块添加 IR 代码
    kBuilder.SetInsertPoint(elseBlock);
    llvm::Value *elseValue = m_else->codegen();
    if (!elseValue) {
        return nullptr;
    }

    // else 代码块完成后运行 mergeBlock
    kBuilder.CreateBr(mergeBlock);
    // 此时 else 代码块的内容已经改变了，后边 phi 还要用到它，所以更新一下
    elseBlock = kBuilder.GetInsertBlock();

    // 给 mergeBlock 添加 IR 代码
    function->getBasicBlockList().push_back(mergeBlock);
    kBuilder.SetInsertPoint(mergeBlock);
    // 使用 PHI 创建分支语句最后的 IR 代码
    llvm::PHINode *phiNode = kBuilder.CreatePHI(llvm::Type::getDoubleTy(kTheContext), 2, "iftmp");
    phiNode->addIncoming(theValue, thenBlock);
    phiNode->addIncoming(elseValue, elseBlock);

    return phiNode;
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


ForExprAST::ForExprAST(const std::string &varName, std::unique_ptr<ExprAST> start, std::unique_ptr<ExprAST> end,
                       std::unique_ptr<ExprAST> step, std::unique_ptr<ExprAST> body)
        : m_varName(varName), m_start(std::move(start)), m_end(std::move(end)), m_step(std::move(step)),
          m_body(std::move(body)) {
}

llvm::Value* ForExprAST::codegen() {
    llvm::Value *startValue = m_start->codegen();
    if (nullptr == startValue) {
        return nullptr;
    }

    // 在当前函数后边加上我们将要生成的 for 循环 block
    llvm::Function *function = kBuilder.GetInsertBlock()->getParent();
    llvm::BasicBlock *preheaderBlock = kBuilder.GetInsertBlock();
    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(kTheContext, "loop", function);

    // 当前函数插入点上，插入运行当前分支语句的代码
    kBuilder.CreateBr(loopBlock);
    // 开始往循环中插入代码
    kBuilder.SetInsertPoint(loopBlock);

    // 分支进入与否同样涉及 SSA (静态单赋值) 问题，所以这里用 phi node 确定进入与不进入循环不同的两个路径
    // 这里添加的是不进入循环的走法
    llvm::PHINode *variable = kBuilder.CreatePHI(llvm::Type::getDoubleTy(kTheContext), 2, m_varName.c_str());
    variable->addIncoming(startValue, preheaderBlock);

    // 如果在循环中循环变量覆盖了当前作用域的变量，我们需要在循环结束后恢复那个变量，所以现在先记住它旧的值
    llvm::Value *oldValue = kNamedValue[m_varName];
    kNamedValue[m_varName] = variable;

    if (nullptr == m_body->codegen()) {
        return nullptr;
    }

    // 生成循环变量步进的代码
    llvm::Value *stepValue = nullptr;
    if (m_step) {
        stepValue = m_step->codegen();
        if (nullptr == stepValue) {
            return nullptr;
        }
    } else {
        // 如果没有步进，那么步进默认为 1.0
        stepValue = llvm::ConstantFP::get(kTheContext, llvm::APFloat(1.0));
    }

    // 对循环变量执行步进
    llvm::Value *nextValue = kBuilder.CreateFAdd(variable, stepValue, "nextvar");

    // 循环结束条件
    llvm::Value *endCondition = m_end->codegen();
    if (nullptr == endCondition) {
        return nullptr;
    }

    // 循环结束条件与 true(1.0) 判断
    endCondition = kBuilder.CreateFCmpONE(endCondition, llvm::ConstantFP::get(kTheContext, llvm::APFloat(1.0)),
                                          "loopcond");

    // 为 phi 记一下循环结束的地方
    llvm::BasicBlock *loopEndBlock = kBuilder.GetInsertBlock();
    // 创建分支语句，符合结束条件走 after block 不符合继续走 loop block
    llvm::BasicBlock *afterBlock = llvm::BasicBlock::Create(kTheContext, "afterloop", function);
    kBuilder.CreateCondBr(endCondition, loopBlock, afterBlock);

    // 将后边的代码添加地点放到循环结束后
    kBuilder.SetInsertPoint(afterBlock);

    // 如果进入循环的话，最后会从 loopEndBlock 结束，这也是 phi node 的一个路径，用它完成 phi node
    variable->addIncoming(nextValue, loopEndBlock);

    if (oldValue) {
        kNamedValue[m_varName] = oldValue;
    } else {
        kNamedValue.erase(m_varName);
    }

    // for 循环作为表达式，整体对外的值永远是 0.0
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(kTheContext));
}
