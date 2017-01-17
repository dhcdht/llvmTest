//
// Created by 董宏昌 on 2017/1/8.
//

#ifndef PROJECT_EXPRAST_H
#define PROJECT_EXPRAST_H

#include <memory>
#include <string>
#include <vector>
#include <map>

namespace llvm {
    class Value;
    class Function;
}


/// 储存了各个操作符的优先级
extern std::map<char, int> kBinaryOPPrecedence;


/// 初始化 llvm 上下文，之后才可以生成 llvm IR 代码
extern void initLLVMContext();
/// dump 出 llvm IR 中现在的代码
extern void dumpLLVMContext();


/*
AST : abstract syntax tree 抽象语法树
ExprAST : 表达式的抽象语法树
所有表达式节点的基类
*/
class ExprAST {
public:
    virtual ~ExprAST();

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
private:
    /// 常量值
    double m_val;

public:
    NumberExprAST(double val);

    virtual llvm::Value *codegen() override;
};


/*
变量节点
比如 "a"
*/
class VariableExprAST : public ExprAST {
private:
    /// 变量名
    std::string m_name;

public:
    VariableExprAST(const std::string &name);

    std::string getName();

    virtual llvm::Value *codegen() override;
};


/**
 * 一元运算符节点
 * 比如 "!"
 */
class UnaryExprAST : public ExprAST {
private:
    /// 自定义的运算符的字符
    char m_operatorCode;
    /// 自定义的运算符的运算对象
    std::unique_ptr<ExprAST> m_operand;

public:
    UnaryExprAST(char operatorCode, std::unique_ptr<ExprAST> operand);

    virtual llvm::Value *codegen() override;
};


/*
二元操作节点
比如 "+"
*/
class BinaryExprAST : public ExprAST {
private:
    /// 操作符
    char m_op;
    /// 操作符左右的表达式
    std::unique_ptr<ExprAST> m_lhs, m_rhs;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs);

    virtual llvm::Value *codegen() override;
};


/*
函数调用表达式节点
比如 "func()"
*/
class CallExprAST : public ExprAST {
private:
    /// 被调用函数名
    std::string m_callee;
    /// 各个参数的表达式
    std::vector<std::unique_ptr<ExprAST>> m_args;

public:
    CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args);

    virtual llvm::Value *codegen() override;
};


/*
 * 分支语句的表达式
 * if/then/else
 */
class IfExprAST : public ExprAST {
private:
    std::unique_ptr<ExprAST> m_condition;
    std::unique_ptr<ExprAST> m_then;
    std::unique_ptr<ExprAST> m_else;

public:
    IfExprAST(std::unique_ptr<ExprAST> condition, std::unique_ptr<ExprAST> then, std::unique_ptr<ExprAST> elseExpr);

    virtual llvm::Value *codegen() override;
};


/**
 * 循环语句的表达式
 * for in
 */
class ForExprAST : public ExprAST {
private:
    std::string m_varName;
    std::unique_ptr<ExprAST> m_start;
    std::unique_ptr<ExprAST> m_end;
    std::unique_ptr<ExprAST> m_step;
    std::unique_ptr<ExprAST> m_body;

public:
    ForExprAST(const std::string &varName, std::unique_ptr<ExprAST> start, std::unique_ptr<ExprAST> end,
               std::unique_ptr<ExprAST> step, std::unique_ptr<ExprAST> body);

    virtual llvm::Value *codegen() override;
};


/**
 * 变量定义节点
 * var a = 1.0 in ...
 */
class VarExprAST : public ExprAST {
private:
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> m_varNames;
    std::unique_ptr<ExprAST> m_body;

public:
    VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> varNames, std::unique_ptr<ExprAST> body);

    virtual llvm::Value *codegen() override;
};


/*
函数定义节点
这不是一个表达式
保存了函数名和参数
*/
class PrototypeAST {
private:
    /// 函数名
    std::string m_name;
    /// 各个参数名
    std::vector<std::string> m_args;
    /// 是否是一个运算符
    bool m_isOperator;
    /// 针对二元运算符的优先级
    unsigned m_precedence;

public:
    PrototypeAST(const std::string &name, std::vector<std::string> args, bool isOperator = false, unsigned precedence = 0);

    /// 返回函数名
    std::string getName();
    /// 取的运算符的 ascii 码
    char getOperatorName();
    /// 返回函数是否是一元运算符
    bool isUnaryOperator();
    /// 返回函数是否是二元运算符
    bool isBinaryOperator();
    /// 返回二元运算符的优先级
    unsigned getBinaryPrecedence();

//    /// 创建各个参数，并且记录地址到 kNamedValue，以便后边的访问和更改
//    void createArgumentAllocas(llvm::Function *function);

    llvm::Function *codegen();
};


/*
函数实现节点
这不是一个表达式
保存了函数原型和函数实现内容的所有表达式节点
*/
class FunctionAST {
private:
    /// 函数定义
    std::unique_ptr<PrototypeAST> m_prototype;
    /// 函数实现内容的各个表达式
    std::unique_ptr<ExprAST> m_body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> prototype, std::unique_ptr<ExprAST> body);

    llvm::Function *codegen();
};


#endif //PROJECT_EXPRAST_H
