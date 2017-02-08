//
// Created by 董宏昌 on 2017/2/8.
//

#ifndef PROJECT_EXPRPARSER_H
#define PROJECT_EXPRPARSER_H


#include <string>
#include "ExprAST.h"


class ExprParser {
private:
    /// 输入的代码字符串流
    std::istringstream *m_codeStream;

    /// 当前解析器当前正在解析的 token
    int m_lastToken;
    /// m_lastToken 为 token_identifier 时，记下当前的 token 字符串
    std::string m_lastTokenIdentifierString;
    /// m_lastToken 为 token_number 时，记下当前的值
    double m_lastTokenNumberValue;

private:
    /*
     * 返回下一个字符
     */
    int getNextChar();
    /*
     * 返回下一个 token
     */
    int getToken();
    /*
     * 设置 m_lastToken 为下一个 token
     */
    void getNextToken();

    /*
     * 取的 token 的优先级
     */
    int getTokenPrecedence(int token);

    /*
     * 打印错误
     */
    void logError(const char *string);

    /*
     * 解析数字
     */
    std::unique_ptr<ExprAST> parseNumberExpr();
    /*
     * 解析一元运算符表达式
     */
    std::unique_ptr<ExprAST> parseUnary();
    /*
     * 解析表达式
     * 递归的使用自身解析
     * 输入是当前的表达式和其优先级，
     * 默认输入 0，lhs，
     * 这样当后接一个二元操作符，比如 + 时，后续的表达式优先级比当前输入大
     */
    std::unique_ptr<ExprAST> parseBinaryOPRHS(int exprPrecedence, std::unique_ptr<ExprAST> lhs);
    /*
     * 解析普通的表达式
     * 递归的使用 parseBinaryOPRHS 实现解析
     */
    std::unique_ptr<ExprAST> parseExpression();
    /*
     * 解析类似 (expression) 的写法
     */
    std::unique_ptr<ExprAST> parseParentExpr();
    /*
     * 解析 a 或 a(‘expression’, 'expression') 这种写法
     */
    std::unique_ptr<ExprAST> parseIdentifierExpr();
    /*
     * 解析 if then else 这样的写法
     */
    std::unique_ptr<ExprAST> parseIfExpr();
    /*
     * 解析 for in 写法
     */
    std::unique_ptr<ExprAST> parseForExpr();
    /*
     * 解析 var a = 1.0 in ... 写法
     */
    std::unique_ptr<ExprAST> parseVarExpr();

    /*
     * 解析 token 的主函数
     */
    std::unique_ptr<ExprAST> parsePrimary();

public:
    ExprParser();

    void startParse(std::string codeString);

};


#endif //PROJECT_EXPRPARSER_H
