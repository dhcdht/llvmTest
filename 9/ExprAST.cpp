//
// Created by 董宏昌 on 2017/1/8.
//

#include <MacTypes.h>
#include "ExprAST.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DIBuilder.h"


/// 运算符优先级记录
std::map<char, int> kBinaryOPPrecedence;

/// llvm 当前上下文对象
static llvm::LLVMContext kTheContext;
/// 用于构建各个 llvm IR 对象，比如函数等
static llvm::IRBuilder<> kBuilder(kTheContext);
/// 当前模块，当前所有函数都在同一模块中
static std::unique_ptr<llvm::Module> kTheModule;
/// 保存变量名与 llvm IR 对象的对应关系，为了实现改变变量，我们修改为保存变量名与变量地址的对应关系
static std::map<std::string, llvm::AllocaInst *> kNamedValue;
/// 用来生成调试信息
static std::unique_ptr<llvm::DIBuilder> kDebugBuilder;


struct DebugInfo {
    llvm::DICompileUnit *compileUnit;
    llvm::DIType *debugInfoType;
    std::vector<llvm::DIScope *> lexicalBlocks;

    void emitLocation(ExprAST *ast);
    llvm::DIType *getDoubleTy();
} kDebugInfo;

void DebugInfo::emitLocation(ExprAST *ast) {
    if (!ast) {
        kBuilder.SetCurrentDebugLocation(llvm::DebugLoc());
        return;
    }

    llvm::DIScope *scope;
    if (this->lexicalBlocks.empty()) {
        scope = this->compileUnit;
    } else {
        scope = this->lexicalBlocks.back();
    }

    kBuilder.SetCurrentDebugLocation(llvm::DebugLoc::get(ast->getLine(), ast->getCol(), scope));
}

llvm::DIType* DebugInfo::getDoubleTy() {
    if (!this->debugInfoType) {
        this->debugInfoType = kDebugBuilder->createBasicType("double", 64, 64, llvm::dwarf::DW_ATE_float);
    }

    return this->debugInfoType;
}


static SourceLocation kCurLocation;
static SourceLocation kLexlocation = {1, 0};

static int advance() {
    int lastChar = getchar();

    if (lastChar == '\n' || lastChar == '\r') {
        kLexlocation.line++;
        kLexlocation.col = 0;
    } else {
        kLexlocation.col++;
    }

    return lastChar;
}

llvm::raw_ostream &indent(llvm::raw_ostream &out, int size) {
    return out << std::string(size, ' ');
}


static llvm::DISubroutineType *createFunctionType(unsigned numberArgs, llvm::DIFile *unit) {
    llvm::SmallVector<llvm::Metadata *, 8> eltTypes;
    llvm::DIType *Dbltype = kDebugInfo.getDoubleTy();

    eltTypes.push_back(Dbltype);

    for (int i = 0; i < numberArgs; ++i) {
        eltTypes.push_back(Dbltype);
    }

    llvm::ArrayRef<llvm::Metadata *> array(eltTypes);

    return kDebugBuilder->createSubroutineType(kDebugBuilder->getOrCreateTypeArray(array));
}


void initLLVMContext() {
    kTheModule = llvm::make_unique<llvm::Module>("My custom jit", kTheContext);

    kDebugBuilder = llvm::make_unique<llvm::DIBuilder>(*kTheModule);
    kDebugInfo.compileUnit = kDebugBuilder->createCompileUnit(llvm::dwarf::DW_LANG_C, "fib.ks", ".",
                                                              "Kaleidoscope Compiler", false, "", 0);
}

llvm::Module* dumpLLVMContext() {
    kTheModule->dump();

    kDebugBuilder->finalize();

    return kTheModule.get();
}


/**
 * 在函数作用域的栈中创建变量
 * 这里的函数含义比较广，不如一个循环结构，一个分支结构等，都可以有自己独立的栈
 * 最直接的体现就是这些作用域里边的变量是会覆盖外边的变量的
 */
static llvm::AllocaInst *createEntryBlockAlloca(llvm::Function *function, const std::string &varName) {
    llvm::IRBuilder<> tmpBuilder(&function->getEntryBlock(), function->getEntryBlock().begin());

    return tmpBuilder.CreateAlloca(llvm::Type::getDoubleTy(kTheContext), 0, varName.c_str());
}


extern std::unique_ptr<ExprAST> logError(const char *str);
llvm::Value *logErrorV(const char *str) {
    logError(str);

    return nullptr;
}


ExprAST::~ExprAST() {

}

unsigned ExprAST::getLine() {
    return m_sourceLocation.line;
}

unsigned ExprAST::getCol() {
    return m_sourceLocation.col;
}

llvm::raw_ostream& ExprAST::dump(llvm::raw_ostream &out, int index) {
    return out << ':' << this->getLine() << ':' << this->getCol() << '\n';
}


NumberExprAST::NumberExprAST(double val)
        : m_val(val) {

}

llvm::Value *NumberExprAST::codegen() {
    // 记录调试信息
    kDebugInfo.emitLocation(this);
    return llvm::ConstantFP::get(kTheContext, llvm::APFloat(m_val));
}

llvm::raw_ostream& NumberExprAST::dump(llvm::raw_ostream &out, int index) {
    return ExprAST::dump(out << m_val, index);
}


VariableExprAST::VariableExprAST(const std::string &name)
        : m_name(name) {

}

std::string VariableExprAST::getName() {
    return m_name;
}

llvm::Value* VariableExprAST::codegen() {
    llvm::Value *value = kNamedValue[m_name];
    if (!value) {
        return logErrorV("Unknown variable name");
    }

    // 记录调试信息
    kDebugInfo.emitLocation(this);

    return kBuilder.CreateLoad(value, m_name.c_str());
}

llvm::raw_ostream& VariableExprAST::dump(llvm::raw_ostream &out, int index) {
    return ExprAST::dump(out << m_name, index);
}


UnaryExprAST::UnaryExprAST(char operatorCode, std::unique_ptr<ExprAST> operand)
        : m_operatorCode(operatorCode), m_operand(std::move(operand)) {

}

llvm::Value* UnaryExprAST::codegen() {
    llvm::Value *operandValue = m_operand->codegen();
    if (!operandValue) {
        return nullptr;
    }

    // 根据一元运算符的名字查找对应的函数
    std::string functionName = std::string("unary") + m_operatorCode;
    llvm::Function *function = kTheModule->getFunction(functionName);
    if (!function) {
        return logErrorV("Unknown unary operator");
    }

    // 记录调试信息
    kDebugInfo.emitLocation(this);

    return kBuilder.CreateCall(function, operandValue, "unop");
}

llvm::raw_ostream& UnaryExprAST::dump(llvm::raw_ostream &out, int index) {
    ExprAST::dump(out << "unary" << m_operatorCode, index);
    m_operand->dump(out, index + 1);

    return out;
}


BinaryExprAST::BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
        : m_op(op), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {

}

llvm::Value* BinaryExprAST::codegen() {
    // 记录调试信息
    kDebugInfo.emitLocation(this);

    llvm::Value *lhsValue = m_lhs->codegen();
    llvm::Value *rhsValue = m_rhs->codegen();
    if (!lhsValue || !rhsValue) {
        return nullptr;
    }

    // 赋值是一个特殊的二元运算符，因其左值是不需要计算的
    if (m_op == '=') {
        // 赋值的左值应该是一个变量，而不是需要计算的表达式
        VariableExprAST *lhs = dynamic_cast<VariableExprAST *>(m_lhs.get());
        if (!lhs) {
            return logErrorV("Destination of '=' must be a variable");
        }

        // 获取左值的那个变量
        llvm::Value *variable = kNamedValue[lhs->getName()];
        if (!variable) {
            return logErrorV("Unknown variable name");
        }

        // 设置左值的变量为右值的计算结果
        kBuilder.CreateStore(rhsValue, variable);

        // 整体运算符的值是右值的结算结果
        return rhsValue;
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
            // 如果进入这里，说明这很可能是一个重写的二元运算符

            // 运算符函数名
            std::string functionName = std::string("binary") + m_op;
            llvm::Function *function = kTheModule->getFunction(functionName);
            if (nullptr == function) {
                return logErrorV("Binary operator not found!");
            }

            // 创建函数调用 IR 代码
            llvm::Value *operators[] = {lhsValue, rhsValue};
            return kBuilder.CreateCall(function, llvm::makeArrayRef(operators), "binop");

            break;
        }
    }
}

llvm::raw_ostream& BinaryExprAST::dump(llvm::raw_ostream &out, int index) {
    ExprAST::dump(out << "binary" << m_op, index);
    m_lhs->dump(indent(out, index) << "lhs:", index + 1);
    m_rhs->dump(indent(out, index) << "rhs:", index + 1);

    return out;
}


CallExprAST::CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args)
        : m_callee(callee), m_args(std::move(args)) {

}

llvm::Value* CallExprAST::codegen() {
    // 记录调试信息
    kDebugInfo.emitLocation(this);

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

llvm::raw_ostream& CallExprAST::dump(llvm::raw_ostream &out, int index) {
    ExprAST::dump(out << "call " << m_callee, index);
    for (const auto &arg : m_args) {
        arg->dump(indent(out, index + 1), index + 1);
    }

    return out;
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
    // 记录调试信息
    kDebugInfo.emitLocation(this);

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

llvm::raw_ostream& IfExprAST::dump(llvm::raw_ostream &out, int index) {
    ExprAST::dump(out << "if", index);
    m_condition->dump(indent(out, index) << "condition:", index + 1);
    m_then->dump(indent(out, index) << "then:", index + 1);
    m_else->dump(indent(out, index) << "else:", index + 1);

    return out;
}


VarExprAST::VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> varNames,
                       std::unique_ptr<ExprAST> body)
        : m_varNames(std::move(varNames)), m_body(std::move(body)) {

}

llvm::Value* VarExprAST::codegen() {
    // 当前函数
    llvm::Function *function = kBuilder.GetInsertBlock()->getParent();

    // 用来记录被当前作用域覆盖的外层同名变量的值
    std::vector<llvm::AllocaInst *> oldBindings;
    for (int i = 0; i < m_varNames.size(); ++i) {
        const std::string &varName = m_varNames[i].first;
        ExprAST *init = m_varNames[i].second.get();

        // 先插入右值代码，再在函数作用域中生成左值变量
        // 比如 var a = a 这种情况，
        // 不先执行右值计算，a 值会使用错误，并且再也无法访问外边的 a 值
        llvm::Value *initValue;
        if (init) {
            initValue = init->codegen();
            if (!initValue) {
                return nullptr;
            }
        } else {
            // 如果没有写右值，那么默认是 0.0
            initValue = llvm::ConstantFP::get(kTheContext, llvm::APFloat(0.0));
        }

        // 生成变量
        llvm::AllocaInst *alloca = createEntryBlockAlloca(function, varName);
        // 赋初值
        kBuilder.CreateStore(initValue, alloca);

        // 记住被当前作用域覆盖的外层同名变量的值
        oldBindings.push_back(kNamedValue[varName]);

        // 记录在当前作用域中，varName 名字的变量的内存
        kNamedValue[varName] = alloca;
    }

    // 记录调试信息
    kDebugInfo.emitLocation(this);

    // 现在，所有 m_body 用到的变量都有了，可以开始生成 m_body 的 IR 代码了
    llvm::Value *bodyValue = m_body->codegen();
    if (!bodyValue) {
        return nullptr;
    }

    // 当前作用域结束之后，恢复旧的同名变量的各个值
    for (int j = 0; j < m_varNames.size(); ++j) {
        kNamedValue[m_varNames[j].first] = oldBindings[j];
    }

    return bodyValue;
}

llvm::raw_ostream& VarExprAST::dump(llvm::raw_ostream &out, int index) {
    ExprAST::dump(out << "var", index);
    for (const auto &varName : m_varNames) {
        varName.second->dump(indent(out, index) << varName.first << ':', index + 1);
    }
    m_body->dump(indent(out, index) << "body:", index + 1);

    return out;
}


PrototypeAST::PrototypeAST(const std::string &name, std::vector<std::string> args, bool isOperator,
                           unsigned int precedence)
        : m_name(name), m_args(std::move(args)), m_isOperator(isOperator), m_precedence(precedence) {

}

std::string PrototypeAST::getName() {
    return m_name;
}

char PrototypeAST::getOperatorName() {
    return m_name[m_name.size() - 1];
}

bool PrototypeAST::isUnaryOperator() {
    return m_isOperator && m_args.size() == 1;
}

bool PrototypeAST::isBinaryOperator() {
    return m_isOperator && m_args.size() == 2;
}

unsigned PrototypeAST::getBinaryPrecedence() {
    return m_precedence;
}

//void PrototypeAST::createArgumentAllocas(llvm::Function *function) {
//    llvm::Function::arg_iterator allocaIterator = function->arg_begin();
//    for (int index = 0; index < m_args.size(); ++index, ++allocaIterator) {
//        // 在函数作用域的栈中创建变量
//        llvm::AllocaInst *alloca = createEntryBlockAlloca(function, m_args[index]);
//
//        // 将参数值传给变量
//        kBuilder.CreateStore(allocaIterator, alloca);
//
//        // 记录一下以便后边访问和更改
//        kNamedValue[m_args[index]] = alloca;
//    }
//}

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

unsigned PrototypeAST::getLine() {
    return m_line;
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

    // 为函数创建单独的调试信息记录
    llvm::DIFile *debugUnit = kDebugBuilder->createFile(kDebugInfo.compileUnit->getFilename(),
                                                        kDebugInfo.compileUnit->getDirectory());
    llvm::DIScope *functionContext = debugUnit;
    unsigned lineNumber = m_prototype->getLine();
    unsigned scopeLine = lineNumber;
    llvm::DISubprogram *subprogram = kDebugBuilder->createFunction(
            functionContext, m_prototype->getName(), llvm::StringRef(), debugUnit, lineNumber,
            createFunctionType(theFunction->arg_size(), debugUnit),
            false, true, scopeLine,
            llvm::DINode::FlagPrototyped, false);
    theFunction->setSubprogram(subprogram);
    kDebugInfo.lexicalBlocks.push_back(subprogram);
    kDebugInfo.emitLocation(nullptr);


    // 记录参数名与其对应的 llvm::Value 对象
    kNamedValue.clear();
    for (auto &arg : theFunction->args()) {
        // 创建
        llvm::AllocaInst *alloca = createEntryBlockAlloca(theFunction, arg.getName());

        // 赋值
        kBuilder.CreateStore(&arg, alloca);

        // 记录
        kNamedValue[arg.getName()] = alloca;
    }

    kDebugInfo.emitLocation(m_body.get());

    // 对于二元运算符，我们要存储它的优先级
    if (m_prototype->isBinaryOperator()) {
        kBinaryOPPrecedence[m_prototype->getOperatorName()] = m_prototype->getBinaryPrecedence();
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

llvm::raw_ostream& FunctionAST::dump(llvm::raw_ostream &out, int index) {
    indent(out, index) << "FunctionAST\n";
    ++index;
    indent(out, index) << "body:";

    return m_body ? m_body->dump(out, index) : out << "null\n";
}


ForExprAST::ForExprAST(const std::string &varName, std::unique_ptr<ExprAST> start, std::unique_ptr<ExprAST> end,
                       std::unique_ptr<ExprAST> step, std::unique_ptr<ExprAST> body)
        : m_varName(varName), m_start(std::move(start)), m_end(std::move(end)), m_step(std::move(step)),
          m_body(std::move(body)) {
}

llvm::Value* ForExprAST::codegen() {
    // 当前函数
    llvm::Function *function = kBuilder.GetInsertBlock()->getParent();
    // 循环的变量
    llvm::AllocaInst *alloca = createEntryBlockAlloca(function, m_varName);

    // 记录调试信息
    kDebugInfo.emitLocation(this);

    llvm::Value *startValue = m_start->codegen();
    if (nullptr == startValue) {
        return nullptr;
    }

    // 存储循环变量的值
    kBuilder.CreateStore(startValue, alloca);

    llvm::BasicBlock *loopBlock = llvm::BasicBlock::Create(kTheContext, "loop", function);

    // 当前函数插入点上，插入运行当前分支语句的代码
    kBuilder.CreateBr(loopBlock);
    // 开始往循环中插入代码
    kBuilder.SetInsertPoint(loopBlock);

    // 如果在循环中循环变量覆盖了当前作用域的变量，我们需要在循环结束后恢复那个变量，所以现在先记住它旧的值
    llvm::AllocaInst *oldValue = kNamedValue[m_varName];
    kNamedValue[m_varName] = alloca;

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

    // 循环结束条件
    llvm::Value *endCondition = m_end->codegen();
    if (nullptr == endCondition) {
        return nullptr;
    }

    // 将循环变量的值设置为最新的
    // 重新读取一下循环变量的值，因为 body 中可能改变了循环变量的值
    llvm::Value *curValue = kBuilder.CreateLoad(alloca);
    llvm::Value *nextValue = kBuilder.CreateFAdd(curValue, stepValue, "nextVar");
    kBuilder.CreateStore(nextValue, alloca);

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

    if (oldValue) {
        kNamedValue[m_varName] = oldValue;
    } else {
        kNamedValue.erase(m_varName);
    }

    // for 循环作为表达式，整体对外的值永远是 0.0
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(kTheContext));
}

llvm::raw_ostream& ForExprAST::dump(llvm::raw_ostream &out, int index) {
    ExprAST::dump(out << "for", index);
    m_start->dump(indent(out, index) << "condition:", index + 1);
    m_end->dump(indent(out, index) << "end:", index + 1);
    m_step->dump(indent(out, index) << "step:", index + 1);
    m_body->dump(indent(out, index) << "body:", index + 1);

    return out;
}
