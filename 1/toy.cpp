#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "llvm/ADT/STLExtras.h"


/*
AST : abstract syntax tree 抽象语法树
ExprAST : 表达式的抽象语法树
所有表达式节点的基类
*/
class ExprAST {
public:
    virtual ~ExprAST() {};
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
};

/*
函数调用表达式节点
比如 "func()"
*/
class CallExprAST : public ExprAST {
    /// 被调用函数名
    std::string m_callee;
    /// 各个参数的表达式
    std::vector<std::unique_ptr<ExprAST> > m_args;

public:
    CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST> > args)
    // TODO: m_args(std::move(args))
    : m_callee(callee), m_args() {};
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
    static int kLastChar = 0;

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
static int getNextTokent() {
    kCurToken = getToken();

    return kCurToken;
}


int main(int argc, char const *argv[]) {

    // 测试代码
    {
        // 如何表示 x + y
        {
            auto lhs = llvm::make_unique<VariableExprAST>("x");
            auto rhs = llvm::make_unique<VariableExprAST>("y");
            auto result = llvm::make_unique<BinaryExprAST>('+', std::move(lhs), std::move(rhs));
        }
    }

    return 0;
}
