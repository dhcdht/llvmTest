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
     * 解析数字
     */
    std::unique_ptr<ExprAST> parseNumberExpr();
    /*
     * 解析一元运算符表达式
     */
    std::unique_ptr<ExprAST> parseUnary();

public:
    ExprParser();

    void startParse(std::string codeString);

};


#endif //PROJECT_EXPRPARSER_H
