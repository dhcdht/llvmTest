//
// Created by 董宏昌 on 2017/1/8.
//

#ifndef PROJECT_EXPRAST_H
#define PROJECT_EXPRAST_H

#include <memory>
#include <string>
#include <vector>

namespace llvm {
    class Value;
    class Function;
}


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

public:
    PrototypeAST(const std::string &name, std::vector<std::string> args);

    std::string getName();

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
