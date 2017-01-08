#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"


/// llvm 当前上下文对象
static llvm::LLVMContext kTheContext;
/// 用于构建各个 llvm IR 对象，比如函数等
static llvm::IRBuilder<> kBuilder(kTheContext);
/// 当前模块，当前所有函数都在同一模块中
static std::unique_ptr<llvm::Module> kTheModule;
/// 保存变量名与 llvm IR 对象的对应关系
static std::map<std::string, llvm::Value *> kNamedValue;


llvm::Value *logErrorV(const char *str);

/*
AST : abstract syntax tree 抽象语法树
ExprAST : 表达式的抽象语法树
所有表达式节点的基类
*/
class ExprAST {
public:
    virtual ~ExprAST() {};

    /*
    生成并取得 AST 节点对应的 llvm::Value 对象
    */
    virtual llvm::Value *codegen() = 0;
};

/*
数字常量节点
比如 "1.0"
*/
class NumberExprAST : public ExprAST {
    /// 常量值
    double m_val;

public:
    NumberExprAST(double val)
    : m_val(val) {};

    virtual llvm::Value *codegen() {
        return llvm::ConstantFP::get(kTheContext, llvm::APFloat(m_val));
    }
};

/*
变量节点
比如 "a"
*/
class VariableExprAST : public ExprAST {
    /// 变量名
    std::string m_name;

public:
    VariableExprAST(const std::string &name)
    : m_name(name) {};

    virtual llvm::Value *codegen() {
        llvm::Value *v = kNamedValue[m_name];
        if (!v) {
            logErrorV("Unknown variable name");
        }

        return v;
    }
};

/*
二元操作节点
比如 "+"
*/
class BinaryExprAST : public ExprAST {
    /// 操作符
    char m_op;
    /// 操作符左右的表达式
    std::unique_ptr<ExprAST> m_lhs, m_rhs;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
    : m_op(op), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {};

    virtual llvm::Value *codegen() {
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
};

/*
函数调用表达式节点
比如 "func()"
*/
class CallExprAST : public ExprAST {
    /// 被调用函数名
    std::string m_callee;
    /// 各个参数的表达式
    std::vector<std::unique_ptr<ExprAST>> m_args;

public:
    CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args)
    : m_callee(callee), m_args(std::move(args)) {};

    virtual llvm::Value *codegen() {
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
};

/*
函数定义节点
这不是一个表达式
保存了函数名和参数
*/
class PrototypeAST {
    /// 函数名
    std::string m_name;
    /// 各个参数名
    std::vector<std::string> m_args;

public:
    PrototypeAST(const std::string &name, std::vector<std::string> args)
    : m_name(name), m_args(std::move(args)) {};

    std::string getName() {
        return m_name;
    }

    llvm::Function *codegen() {
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
};

/*
函数实现节点
这不是一个表达式
保存了函数原型和函数实现内容的所有表达式节点
*/
class FunctionAST {
    /// 函数定义
    std::unique_ptr<PrototypeAST> m_prototype;
    /// 函数实现内容的各个表达式
    std::unique_ptr<ExprAST> m_body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> prototype, std::unique_ptr<ExprAST> body)
    : m_prototype(std::move(prototype)), m_body(std::move(body)) {};

    llvm::Function *codegen() {
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
};


/// 解析的 token 类型枚举，这里都是负数，token 如果不是这里的类型，会返回 0-255 返回的 ascii 码
enum Token {
    token_eof = -1,
    token_def = -2,
    token_extern = -3,
    token_identifier = -4,
    token_number = -5,
};

/// token 为 token_identifier 时，记下当前的 token 字符串
static std::string kIdentifierString;
/// token 为 token_number 时，记下当前的值
static double kNumberValue;
/*
词法解析器主函数
从 stdin 中返回下一个 token
enum Token 类型或 0-255 的 ascii 码
*/
static int getToken() {
    static int kLastChar = ' ';

    // 跳过空格
    while (isspace(kLastChar)) {
        kLastChar = getchar();
    }

    // [a-zA-Z]
    if (isalpha(kLastChar)) {
        // 获取并记录当前这个字符串
        kIdentifierString = kLastChar;
        // [a-zA-Z0-9]
        while (isalnum(kLastChar = getchar())) {
            kIdentifierString += kLastChar;
        }

        // 针对当前字符串的值进行判断
        if (kIdentifierString == "def") {
            return token_def;
        } else if (kIdentifierString == "extern") {
            return token_extern;
        }

        // 一个普通的字符串很可能是一个标识，比如变量名
        return token_identifier;
    }
    // [0-9\.]
    else if (isdigit(kLastChar) || kLastChar == '.') {
        std::string numberString;
        do {
            numberString += kLastChar;
            kLastChar = getchar();
        } while(isdigit(kLastChar) || kLastChar == '.');

        kNumberValue = strtod(numberString.c_str(), nullptr);

        return token_number;
    }
    // # 开头的一行为注释
    else if (kLastChar == '#') {
        do {
            kLastChar = getchar();
        } while(kLastChar != EOF && kLastChar != '\n' && kLastChar != '\r');

        if (kLastChar != EOF) {
            return getToken();
        } else {
            return token_eof;
        }
    }
    else {
        if (kLastChar == EOF) {
            return token_eof;
        }

        int thisChar = kLastChar;
        kLastChar = getchar();

        return thisChar;
    }
}

/// 当前解析器当前正在解析的 token
static int kCurToken;
/*
从词法分析器获取下一个 token 并更新 kCurToken 等变量
*/
static int getNextToken() {
    kCurToken = getToken();

    return kCurToken;
}


std::unique_ptr<ExprAST> logError(const char *str) {
    fprintf(stderr, "LogError: %s\n", str);

    return nullptr;
}

std::unique_ptr<PrototypeAST> logErrorP(const char *str) {
    logError(str);

    return nullptr;
}

llvm::Value *logErrorV(const char *str) {
    logError(str);

    return nullptr;
}


/*
解析数字
*/
static std::unique_ptr<ExprAST> parseNumberExpr() {
    auto result = llvm::make_unique<NumberExprAST>(kNumberValue);
    getNextToken();

    return std::move(result);
}

// 提前声明 parseExpression
static std::unique_ptr<ExprAST> parseExpression();

/*
解析类似 (expression) 的写法
*/
static std::unique_ptr<ExprAST> parseParentExpr() {
    getNextToken();
    auto v = parseExpression();
    if (!v) {
        return nullptr;
    }

    if (kCurToken != ')') {
        return logError("Expected ')'");
    }

    // 下次该解析 ')' 后边的东西了，这个 getNextToken 提前拿出 ‘)’
    getNextToken();

    return v;
}

/*
解析 a 或 a(‘expression’, 'expression') 这种写法
*/
static std::unique_ptr<ExprAST> parseIdentifierExpr() {
    std::string identifierName = kIdentifierString;

    getNextToken();

    if (kCurToken != '(') {
        return llvm::make_unique<VariableExprAST>(identifierName);
    }

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> args;
    if (kCurToken != ')') {
        while (1) {
            if (auto arg = parseExpression()) {
                args.push_back(std::move(arg));
            } else {
                return nullptr;
            }

            if (kCurToken == ')') {
                break;
            }

            if (kCurToken != ',') {
                return logError("Expected ')' or ',' in argument list");
            }

            getNextToken();
        }
    }

    // 下次该解析 ')' 后边的东西了，这个 getNextToken 提前拿出 ‘)’
    getNextToken();

    return llvm::make_unique<CallExprAST>(identifierName, std::move(args));
}

/// 储存了各个操作符的优先级
static std::map<char, int> kBinaryOPPrecedence;
/*
取的当前 token 的优先级
*/
static int getTokenPrecedence() {
    if (!isascii(kCurToken)) {
        return -1;
    }

    int tokenPrecedence = kBinaryOPPrecedence[kCurToken];
    if (tokenPrecedence <= 0) {
        return -1;
    } else {
        return tokenPrecedence;
    }
}

/*
解析 token 的主函数
*/
static std::unique_ptr<ExprAST> parsePrimary() {
    switch (kCurToken) {
        case token_identifier: {
            return parseIdentifierExpr();
        } break;

        case token_number: {
            return parseNumberExpr();
        } break;

        case '(': {
            return parseParentExpr();
        } break;

        default: {
            return logError("Unknown token when expecting an expression");
        } break;
    }
}

/*
解析表达式
递归的使用自身解析
输入是当前的表达式和其优先级，
默认输入 0，lhs，
这样当后接一个二元操作符，比如 + 时，后续的表达式优先级比当前输入大
*/
static std::unique_ptr<ExprAST> parseBinaryOPRHS(int exprPrecedence, std::unique_ptr<ExprAST> lhs) {
    while (1) {
        int tokenPrecedence = getTokenPrecedence();

        // 后续表达式的优先级低于当前解析的部分，那么当前解析的部分可以作为整体返回了
        if (tokenPrecedence < exprPrecedence) {
            return lhs;
        }

        // 因为 exprPrecedence 初始为 0，所以如果上边判断条件不成立
        // 那么 token 必然是一个我们定义了优先级的二元操作符
        int binaryOP = kCurToken;
        getNextToken();

        auto rhs = parsePrimary();
        if (!rhs) {
            return nullptr;
        }

        // 下一个 token 的优先级如果大于当前部分
        // 那么它一定是另一个优先级更高的二元操作符
        // 那么我们将右边的所有表达式作为一个整体当做右值
        // 即优先计算他们，将他们的计算结果作为当前操作符的右值
        int nextPrecedence = getTokenPrecedence();
        if (tokenPrecedence < nextPrecedence) {
            rhs = parseBinaryOPRHS(tokenPrecedence + 1, std::move(rhs));
            if (!rhs) {
                return nullptr;
            }
        }

        // 我们将当前的计算结果作为一个整体
        // 在下一次循环看是不是还继续是操作符
        // 比如 a+b+c 这种，我们在这不相当于做了 (a+b)
        // 后边我们将它作为整体计算 (a+b)+c
        lhs = llvm::make_unique<BinaryExprAST>(binaryOP, std::move(lhs), std::move(rhs));
    }
}

/*
解析普通的表达式
递归的使用 parseBinaryOPRHS 实现解析
*/
static std::unique_ptr<ExprAST> parseExpression() {
    auto lhs = parsePrimary();
    if (!lhs) {
        return nullptr;
    }

    // 初始优先级设为 0 使得我们自定义的操作符优先于当前表达式，其余为 -1，
    return parseBinaryOPRHS(0, std::move(lhs));
}

/*
解析 func(a, b, c) 这种写法，即函数定义
*/
static std::unique_ptr<PrototypeAST> parsePrototype() {
    if (kCurToken != token_identifier) {
        return logErrorP("Expected function name in prototype");
    }

    std::string functionName = kIdentifierString;
    getNextToken();

    if (kCurToken != '(') {
        return logErrorP("Expected '(' in prototype");
    }

    std::vector<std::string> argNames;
    while (getNextToken() == token_identifier) {
        argNames.push_back(kIdentifierString);
    }
    if (kCurToken != ')') {
        return logErrorP("Expected ')' in prototype");
    }

    // 下次该解析 ')' 后边的东西了，这个 getNextToken 提前拿出 ‘)’
    getNextToken();

    return llvm::make_unique<PrototypeAST>(functionName, std::move(argNames));
}

/*
解析 def func(a, b) 这种写法，即函数实现
*/
static std::unique_ptr<FunctionAST> parseDefinition() {
    // 将 def 走过去
    getNextToken();

    // 解析函数的定义
    auto prototype = parsePrototype();
    if (!prototype) {
        return nullptr;
    }

    // 解析函数的实现，并生成函数实现 AST 节点
    if (auto expression = parseExpression()) {
        return llvm::make_unique<FunctionAST>(std::move(prototype), std::move(expression));
    }

    return nullptr;
}

/*
解析 extern func 这种写法，即提前声明的函数定义
*/
static std::unique_ptr<PrototypeAST> parseExtern() {
    // 将 extern 走过去
    getNextToken();

    return parsePrototype();
}

/*
解析在全局作用范围内的表达式，即最顶层写的，不在任何函数中的表达式
*/
static std::unique_ptr<FunctionAST> parseTopLevelExpr() {
    if (auto expression = parseExpression()) {
        auto prototype = llvm::make_unique<PrototypeAST>("", std::vector<std::string>());

        return llvm::make_unique<FunctionAST>(std::move(prototype), std::move(expression));
    }

    return nullptr;
}

static void handleDefinition() {
    if (auto functionAST = parseDefinition()) {
        if (auto *functionIR = functionAST->codegen()) {
            fprintf(stderr, "Read function definition:");
            functionIR->dump();
        }
    } else {
        getNextToken();
    }
}

static void handleExtern() {
    if (auto protoAST = parseExtern()) {
        if (auto protoIR = protoAST->codegen()) {
            fprintf(stderr, "Read extern:");
            protoIR->dump();
        }
    } else {
        getNextToken();
    }
}

static void handleTopLevelExpression() {
    if (auto expressionAST = parseTopLevelExpr()) {
        if (auto expressionIR = expressionAST->codegen()) {
            fprintf(stderr, "Read top-level expr:");
            expressionIR->dump();
        }
    } else {
        getNextToken();
    }
}

/*
解析表达式的主循环
*/
static void mainLoop() {
    while (1) {
        fprintf(stderr, "ready> ");
        switch (kCurToken) {
            case token_eof: {
                return;
            } break;

            case ';': {
                getNextToken();
            } break;

            case token_def: {
                handleDefinition();
            } break;

            case token_extern: {
                handleExtern();
            } break;

            default: {
                handleTopLevelExpression();
            } break;
        }
    }
}


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
        kBinaryOPPrecedence['<'] = 10;
        kBinaryOPPrecedence['+'] = 20;
        kBinaryOPPrecedence['-'] = 30;
        kBinaryOPPrecedence['*'] = 40;
    }

    // 写上初始的提示文本
    fprintf(stderr, "ready> ");
    getNextToken();

    kTheModule = llvm::make_unique<llvm::Module>("My custom jit", kTheContext);

    // 开始解析主循环
    mainLoop();

    kTheModule->dump();

    return 0;
}
