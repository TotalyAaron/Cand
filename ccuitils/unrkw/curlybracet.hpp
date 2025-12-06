#ifndef __CURLYBRACET_HPP__
#define __CURLYBRACET_HPP__
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/IR/PassManager.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IR/Verifier.h>
#include <regex>
#include <string>
#include <iostream>
#include <algorithm>
#include <sstream>
#include "../fnc/fnc.hpp"
using namespace llvm;
static int ParseEndingCurlyBracet(std::string line, LLVMContext &ctx)
{
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok)
    {
        bool isEndingBracet = tok != "}";
        if (isEndingBracet)
            continue;
        if (scopes.empty())
            throw std::exception("Cannot place a '}' if no function, if statement, ... is behind the brace.");
        if (scopes.back() == _TYPE_NESTED_::FNC)
        {
            if (currentFunction == nullptr)
                throw std::exception("Invalid 'currentFunction' because it has value 'nullptr'");
            raw_os_ostream out(std::cout);
            CheckFunction_isValid(*currentFunction, out);
            scopes.pop_back(); // Removes 'FNC'
            is_inside_function = false;
            return 0;
        }
        throw std::exception("Invalid closing brace.");
    }
    return 1;
}
#endif