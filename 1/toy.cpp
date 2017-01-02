#include <cstdio>
#include <cstdlib>
#include <string>


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
    char m_op;
    std::unique_ptr<ExprAST> m_lhs, m_rhs;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs)
    : m_op(op), m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {};
};


int main(int argc, char const *argv[]) {



    return 0;
}
