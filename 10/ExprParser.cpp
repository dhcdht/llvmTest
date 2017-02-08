//
// Created by 董宏昌 on 2017/2/8.
//

#include "ExprParser.h"
#include <iostream>
#include <sstream>


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

    // 用户自定义运算符的关键字
    token_binary = -11, // 二元
    token_unary = -12,  // 一元

    // 用户定义变量关键字
    token_var = -13,
};


ExprParser::ExprParser() {
    m_codeStream = new std::istringstream();
}

int ExprParser::getNextChar() {
    int c = m_codeStream->get();
    return c;
}

int ExprParser::getToken() {
    int lastChar = this->getNextChar();

    // 跳过空格
    while (isspace(lastChar)) {
        lastChar = this->getNextChar();
    }

    // [a-zA-Z]
    if (isalpha(lastChar)) {
        // 获取并记录当前这个字符串
        m_lastTokenIdentifierString = lastChar;
        // [a-zA-Z0-9]
        while (isalnum(lastChar = this->getNextChar())) {
            m_lastTokenIdentifierString += lastChar;
        }

        // 针对当前字符串的值进行判断，这部分都是一些语言的关键字
        if (m_lastTokenIdentifierString == "def") {
            return token_def;
        } else if (m_lastTokenIdentifierString == "extern") {
            return token_extern;
        } else if (m_lastTokenIdentifierString == "if") {
            return token_if;
        } else if (m_lastTokenIdentifierString == "then") {
            return token_then;
        } else if (m_lastTokenIdentifierString == "else") {
            return token_else;
        } else if (m_lastTokenIdentifierString == "for") {
            return token_for;
        } else if (m_lastTokenIdentifierString == "in") {
            return token_in;
        } else if (m_lastTokenIdentifierString == "binary") {
            return token_binary;
        } else if (m_lastTokenIdentifierString == "unary") {
            return token_unary;
        } else if (m_lastTokenIdentifierString == "var") {
            return token_var;
        }

        // 一个普通的字符串很可能是一个标识，比如变量名
        return token_identifier;
    }
    // [0-9\.]
    else if (isdigit(lastChar) || lastChar == '.') {
        std::string numberString;
        do {
            numberString += lastChar;
            lastChar = this->getNextChar();
        } while(isdigit(lastChar) || lastChar == '.');

        m_lastTokenNumberValue = strtod(numberString.c_str(), nullptr);

        return token_number;
    }
    // # 开头的一行为注释
    else if (lastChar == '#') {
        do {
            lastChar = this->getNextChar();
        } while(lastChar != EOF && lastChar != '\n' && lastChar != '\r');

        if (lastChar != EOF) {
            return this->getToken();
        } else {
            return token_eof;
        }
    }
    else {
        if (lastChar == EOF) {
            return token_eof;
        } else {
            return lastChar;
        }
    }
}

void ExprParser::getNextToken() {
    m_lastToken = getToken();
}

int ExprParser::getTokenPrecedence(int token) {
    // 不认识的字符
    if (!isascii(token)) {
        return -1;
    }

    int tokenPrecedence = kBinaryOPPrecedence[token];
    if (tokenPrecedence <= 0) {
        return -1;
    } else {
        return tokenPrecedence;
    }
}

void ExprParser::logError(const char *string) {
    fprintf(stderr, "LogError: %s\n", string);
}

std::unique_ptr<ExprAST> ExprParser::parseNumberExpr() {
    auto result = llvm::make_unique<NumberExprAST>(m_lastTokenNumberValue);

    return std::move(result);
}

std::unique_ptr<ExprAST> ExprParser::parseUnary() {
    // todo
//    // 如果当前字符不是一个合法的一元运算符，那么交给其他解析去处理
//    if (!isascii(m_lastToken) || m_lastToken == '(' || m_lastToken == ',') {
//        return this->parsePrimary();
//    }

    // 记录当前一元运算符的 ascii 码
    int operatorChar = m_lastToken;
    // 记录一元运算符的运算对象
    this->getNextToken();
    if (auto operand = this->parseUnary()) {
        return llvm::make_unique<UnaryExprAST>(operatorChar, std::move(operand));
    }
    return nullptr;
}

std::unique_ptr<ExprAST> ExprParser::parseBinaryOPRHS(int exprPrecedence, std::unique_ptr<ExprAST> lhs) {
    while (1) {
        int tokenPrecedence = this->getTokenPrecedence(m_lastToken);

        // 后续表达式的优先级低于当前解析的部分，那么当前解析的部分可以作为整体返回了
        if (tokenPrecedence < exprPrecedence) {
            return lhs;
        }

        // 因为 exprPrecedence 初始为 0，所以如果上边判断条件不成立
        // 那么 token 必然是一个我们定义了优先级的二元操作符
        int binaryOP = m_lastToken;
        this->getNextToken();

        // 此处也优先查看右值是否是一个一元运算符的计算
        auto rhs = parseUnary();
        if (!rhs) {
            return nullptr;
        }

        // 下一个 token 的优先级如果大于当前部分
        // 那么它一定是另一个优先级更高的二元操作符
        // 那么我们将右边的所有表达式作为一个整体当做右值
        // 即优先计算他们，将他们的计算结果作为当前操作符的右值
        int nextPrecedence = this->getTokenPrecedence(m_lastToken);
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

std::unique_ptr<ExprAST> ExprParser::parseExpression() {
    // 优先查看其是否是一个一元运算符，parseUnary() 中会检查 parsePrimary
    auto lhs = this->parseUnary();
    if (!lhs) {
        return nullptr;
    }

    // 初始优先级设为 0 使得我们自定义的操作符优先于当前表达式，其余为 -1，
    return this->parseBinaryOPRHS(0, std::move(lhs));
}

std::unique_ptr<ExprAST> ExprParser::parseParentExpr() {
    this->getNextToken();
    auto v = this->parseExpression();
    if (!v) {
        return nullptr;
    }

    if (m_lastToken != ')') {
        this->logError("Expected ')'");

        return nullptr;
    }

    return v;
}

std::unique_ptr<ExprAST> ExprParser::parseIdentifierExpr() {
    std::string identifierName = m_lastTokenIdentifierString;

    getNextToken();

    if (m_lastToken != '(') {
        return llvm::make_unique<VariableExprAST>(identifierName);
    }

    getNextToken();
    std::vector<std::unique_ptr<ExprAST>> args;
    if (m_lastToken != ')') {
        while (1) {
            if (auto arg = parseExpression()) {
                args.push_back(std::move(arg));
            } else {
                return nullptr;
            }

            if (m_lastToken == ')') {
                break;
            }

            if (m_lastToken != ',') {
                logError("Expected ')' or ',' in argument list");

                return nullptr;
            }

            getNextToken();
        }
    }

    return llvm::make_unique<CallExprAST>(identifierName, std::move(args));
}

std::unique_ptr<ExprAST> ExprParser::parseIfExpr() {
    // 走过 if
    getNextToken();

    auto condition = parseExpression();
    if (!condition) {
        return nullptr;
    }

    if (m_lastToken != token_then) {
        logError("Expected then");

        return nullptr;
    }
    // 走过 then
    getNextToken();

    auto then = parseExpression();
    if (!then) {
        return nullptr;
    }

    if (m_lastToken != token_else) {
        logError("Expecte else");

        return nullptr;
    }
    // 走过 else
    getNextToken();

    auto elseExpression = parseExpression();
    if (!elseExpression) {
        return nullptr;
    }

    return llvm::make_unique<IfExprAST>(std::move(condition), std::move(then), std::move(elseExpression));
}

std::unique_ptr<ExprAST> ExprParser::parseForExpr() {
    getNextToken();
    if (m_lastToken != token_identifier) {
        logError("Expected identifier after for");

        return nullptr;
    }
    std::string idName = m_lastTokenIdentifierString;

    getNextToken();
    if (m_lastToken != '=') {
        logError("Expected '=' after for");

        return nullptr;
    }

    getNextToken();
    auto start = parseExpression();
    if (nullptr == start) {
        return nullptr;
    }
    if (m_lastToken != ',') {
        logError("Expected ',' after for start value");

        return nullptr;
    }

    getNextToken();
    auto end = parseExpression();
    if (!end) {
        return nullptr;
    }

    // step 是可选的
    std::unique_ptr<ExprAST> step;
    if (m_lastToken == ',') {
        getNextToken();
        step = parseExpression();
        if (!step) {
            return nullptr;
        }
    }

    if (m_lastToken != token_in) {
        logError("Expected 'in' after for");

        return nullptr;
    }

    getNextToken();
    auto body = parseExpression();
    if (!body) {
        return nullptr;
    }

    return llvm::make_unique<ForExprAST>(idName, std::move(start), std::move(end), std::move(step), std::move(body));
}

std::unique_ptr<ExprAST> ExprParser::parseVarExpr() {
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> varNames;

    getNextToken();
    if (m_lastToken != token_identifier) {
        logError("Expected identifier after var");

        return nullptr;
    }

    while (1) {
        // 变量名
        std::string name = m_lastTokenIdentifierString;
        // 记录右值表达式
        std::unique_ptr<ExprAST> init;

        getNextToken();
        if (m_lastToken == '=') {

            getNextToken();
            init = parseExpression();
            if (!init) {
                return nullptr;
            }
        }

        varNames.push_back(std::make_pair(name, std::move(init)));

        // var 用 ',' 来同时定义多个变量，如果找不到 ','，那么循环就可以结束了
        if (m_lastToken != ',') {
            break;
        }

        // 如果是 ',' 分割的多个变量定义，那么下一个 token 还应该是 token_identifier，一个变量名才对
        getNextToken();
        if (m_lastToken != token_identifier) {
            logError("Expected identifier list after var");

            return nullptr;
        }
    }

    // 后边跟着的应该是 in 关键字
    if (m_lastToken != token_in) {
        logError("Expected 'in' keywork after 'var'");

        return nullptr;
    }

    // 解析主体表达式
    getNextToken();
    auto body = parseExpression();
    if (!body) {
        return nullptr;
    }

    return llvm::make_unique<VarExprAST>(std::move(varNames), std::move(body));
}

std::unique_ptr<ExprAST> ExprParser::parsePrimary() {
    switch (m_lastToken) {
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

        case token_for: {
            return parseForExpr();
        } break;

        case token_var: {
            return parseVarExpr();
        } break;

        default: {
            logError("Unknown token when expecting an expression");

            return nullptr;
        } break;
    }
}

void ExprParser::startParse(std::string codeString) {
    m_codeStream->str(codeString);

    int c = this->getNextChar();
    while (c != EOF) {
        std::cout << c << std::endl;

        c = this->getNextChar();
    }
}
