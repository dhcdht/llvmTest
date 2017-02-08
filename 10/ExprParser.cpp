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
    getNextToken();
    if (auto operand = this->parseUnary()) {
        return llvm::make_unique<UnaryExprAST>(operatorChar, std::move(operand));
    }
    return nullptr;
}

void ExprParser::startParse(std::string codeString) {
    m_codeStream->str(codeString);

    int c = this->getNextChar();
    while (c != EOF) {
        std::cout << c << std::endl;

        c = this->getNextChar();
    }
}
