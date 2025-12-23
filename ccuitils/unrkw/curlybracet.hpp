#ifndef __CURLYBRACET_HPP__
#define __CURLYBRACET_HPP__
#include <regex>
#include <string>
#include <iostream>
#include <algorithm>
#include <sstream>
#include "../fnc/fnc.hpp"
using namespace llvm;
static int ParseEndingCurlyBracet(std::string& line, LLVMContext &ctx)
{
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok)
    {
        bool isEndingBracet = tok != "}";
        if (isEndingBracet)
            continue;
        if (scopes.empty())
            throw CAndException("Cannot place a '}' if no function, if statement, ... is behind the brace", "SyntaxError");
        if (scopes.back() == _TYPE_NESTED_::FNC)
        {
            if (currentFunction == nullptr)
                throw CAndException("Function cannot be empty", "SyntaxError");
            raw_os_ostream out(std::cout);
            CheckFunction_isValid(*currentFunction, out);
            scopes.pop_back(); // Removes 'FNC'
            is_inside_function = false;
            return 0;
        }
        throw CAndException("Invalid closing brace", "SyntaxError");
    }
    return 1;
}
#endif