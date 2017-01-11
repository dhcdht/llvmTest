#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include "ExprAST.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/Function.h"


/// 解析的 token 类型枚举，这里都是负数，token 如果不是这里的类型，会返回 0-255 返回的 ascii 码
enum Token {
    token_eof = -1,
    token_def = -2,
    token_extern = -3,
    token_identifier = -4,
    token_number = -5,

    // 控制流关键字
    token_if = -6,
    token_then = -7,
    token_else = -8,

    // 循环控制关键字
    token_for = -9,
    token_in = -10,
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

        // 针对当前字符串的值进行判断，这部分都是一些语言的关键字
        if (kIdentifierString == "def") {
            return token_def;
        } else if (kIdentifierString == "extern") {
            return token_extern;
        } else if (kIdentifierString == "if") {
            return token_if;
        } else if (kIdentifierString == "then") {
            return token_then;
        } else if (kIdentifierString == "else") {
            return token_else;
        } else if (kIdentifierString == "for") {
            return token_for;
        } else if (kIdentifierString == "in") {
            return token_in;
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

/**
 * 解析 if then else 这样的写法
 */
static std::unique_ptr<ExprAST> parseIfExpr() {
    // 走过 if
    getNextToken();

    auto condition = parseExpression();
    if (!condition) {
        return nullptr;
    }

    if (kCurToken != token_then) {
        return logError("Expected then");
    }
    // 走过 then
    getNextToken();

    auto then = parseExpression();
    if (!then) {
        return nullptr;
    }

    if (kCurToken != token_else) {
        return logError("Expecte else");
    }
    // 走过 else
    getNextToken();

    auto elseExpression = parseExpression();
    if (!elseExpression) {
        return nullptr;
    }

    return llvm::make_unique<IfExprAST>(std::move(condition), std::move(then), std::move(elseExpression));
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

        case token_if: {
            return parseIfExpr();
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

    // 初始化 llvm 环境
    initLLVMContext();

    // 写上初始的提示文本
    fprintf(stderr, "ready> ");
    getNextToken();

    // 开始解析主循环
    mainLoop();

    // dump 出当前 llvm IR 中已经生成的所有代码
    dumpLLVMContext();

    return 0;
}
