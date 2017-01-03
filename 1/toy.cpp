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
