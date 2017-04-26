// Copyright 2017 Alexander Gaivanuk

#include "include/arithmetic_parser.h"

#include <math.h>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <algorithm>
#include <iterator>

/*
    ArithmeticParser is a recursive descent parser that
    uses the folowing formal grammar:

    EXPR  -> EXPR2{[+ | -]EXPR2}
    EXPR2 -> EXPR3{[* | /]EXPR3}
    EXPR3 -> EXPR4 | EXPR4^EXPR4
    EXPR4 -> x | NUM x | NUM(EXPR) | -F | FUNC(EXPR) | (EXPR)
    FUNC  -> cos | sin | tg | ctg | arcsin | arccos | arctg | ln | lg | abs
    NUM   -> I | R
    I     -> 0|1|2...|9|0I|1I|...|9I
    R     -> I.I
*/

using std::string;
using std::vector;
using std::stack;

vector<char> ArithmeticParser::delims = {
    '^', '+', '-', '*', '/', '(', ')'
};

vector<string> ArithmeticParser::funcnames = {
    "cos", "sin", "tg", "ctg", "arcsin", "arccos", "arctg", "ln", "lg", "abs"
};

ArithmeticParser::func_t ArithmeticParser::functions[] = {
    cos, sin, tan, [](double x) -> double { return 1.0 / tan(x); },
    asin, acos, atan, log, log10, fabs
};

bool ArithmeticParser::parse(const string &s) {
    buf = vector<char>();
    rpn = vector<Token>();
    buf.reserve(80);
    rpn.reserve(50);

    data = s.c_str();
    try {
        nextToken();
        EXPR();
        expectToken(T_END);
        return true;
    }
    catch (Token) {
        rpn.clear();
        return false;
    }
    catch (const char *) {
        rpn.clear();
        return false;
    }
}

double ArithmeticParser::evaluate(double x) const {
    stack<double> nums;
    int s = rpn.size();
    if (s == 0) return 0.0;

    for (int i = 0; i < s; i++) {
        const Token &t = rpn[i];

        if (t.type == T_NUMBER) {
            nums.push(t.realValue);
        } else if (t.type == T_X) {
            nums.push(x);
        } else {
            double op1, op2;
            token_t tt = t.type;

            op2 = nums.top(); nums.pop();
            if (t.type != T_NEGATE && t.type != T_FUNC) {
                op1 = nums.top(); nums.pop();
            }

            if (tt == T_PLUS)
                nums.push(op1 + op2);
            else if (tt == T_MINUS)
                nums.push(op1 - op2);
            else if (tt == T_MUL)
                nums.push(op1 * op2);
            else if (tt == T_DIV)
                nums.push(op1 / op2);
            else if (tt == T_POW)
                nums.push(pow(op1, op2));
            else if (tt == T_NEGATE)
                nums.push(-op2);
            else if (tt == T_FUNC)
                nums.push(functions[t.intValue](op2));
        }
    }

    return nums.top();
}

ArithmeticParser::Token ArithmeticParser::getToken() {
    token_state state = TS_INITIAL;
    int n = 0;

    while (true) {
        char c = *data;
        switch (state) {
        case TS_INITIAL:
            if (isspace(c)) {
                data++;
            } else if (tolower(c) == 'x') {
                data++;
                return Token(T_X);
            } else if (isdigit(c)) {
                n = c - '0';
                data++;
                state = TS_NUMBER;
            } else if (isalpha(c)) {
                state = TS_FUNCNAME;
            } else if (c == 0) {
                return Token(T_END);
            } else {
                state = TS_DELIM;
            }
            break;
        case TS_NUMBER:
            while (isdigit(c)) {
                n = n * 10 + c - '0';
                c = *++data;
            }

            if (c == '.') {
                c = *++data;
                if (!isdigit(c)) throw data;

                buf.clear();
                buf.push_back('0');
                buf.push_back('.');
                do {
                    buf.push_back(c);
                    c = *++data;
                } while (isdigit(c));

                double value = n + atof(buf.data());
                return Token(T_NUMBER, value);
            } else {
                return Token(T_NUMBER, static_cast<double>(n));
            }
            break;
        case TS_FUNCNAME: {
            buf.clear();
            while (isalpha(c)) {
                buf.push_back(c);
                c = *++data;
            }
            buf.push_back('\0');
            const char *str = buf.data();

            auto it = std::find(funcnames.begin(), funcnames.end(), str);
            if (it != funcnames.end()) {
                int index = std::distance(funcnames.begin(), it);
                return Token(T_FUNC, index);
            }

            throw str;
        }
        case TS_DELIM:
            auto it = std::find(delims.begin(), delims.end(), c);
            if (it != delims.end()) {
                int index = std::distance(delims.begin(), it);
                data++;
                return Token((token_t)index);
            }
            throw data;
        }
    }
}

void ArithmeticParser::EXPR() {
    EXPR2();
    while (token.type == T_PLUS || token.type == T_MINUS) {
        token_t tt = token.type;
        nextToken();
        EXPR2();
        rpn.push_back(Token(tt));
    }
}

void ArithmeticParser::EXPR2() {
    EXPR3();
    while (token.type == T_MUL || token.type == T_DIV) {
        token_t tt = token.type;
        nextToken();
        EXPR3();
        rpn.push_back(Token(tt));
    }
}

void ArithmeticParser::EXPR3() {
    EXPR4();
    if (token.type == T_POW) {
        nextToken();
        EXPR4();
        rpn.push_back(Token(T_POW));
    }
}

void ArithmeticParser::EXPR4() {
    if (token.type == T_FUNC) {
        int functionIndex = token.intValue;
        nextToken();
        expectToken(T_LPAREN);
        nextToken();
        EXPR();
        expectToken(T_RPAREN);
        nextToken();
        rpn.push_back(Token(T_FUNC, functionIndex));
    } else if (token.type == T_X) {
        rpn.push_back(Token(T_X));
        nextToken();
    } else if (token.type == T_NUMBER) {
        rpn.push_back(Token(token));
        nextToken();
        if (token.type == T_X) {
            rpn.push_back(Token(T_X));
            rpn.push_back(Token(T_MUL));
            nextToken();
        } else if (token.type == T_LPAREN) {
            nextToken();
            EXPR();
            expectToken(T_RPAREN);
            nextToken();
            rpn.push_back(Token(T_MUL));
        }
    } else if (token.type == T_LPAREN) {
        nextToken();
        EXPR();
        expectToken(T_RPAREN);
        nextToken();
    } else if (token.type == T_MINUS) {
        nextToken();
        EXPR4();
        rpn.push_back(Token(T_NEGATE));
    } else {
        throw token;
    }
}