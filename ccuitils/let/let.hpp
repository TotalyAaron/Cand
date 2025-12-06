#ifndef __LET_HPP__
#define __LET_HPP__
#include <string>
#include <iostream>
#include <regex>
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
#include "evaulate_expr.hpp"
#include "insert_variable.hpp"
#include "../caccfg.h"
#include "../fnc/fnc.hpp"
using namespace llvm;
static int ParseLet(const std::string &line, LLVMContext &ctx, Module &module, IRBuilder<> &builder)
{
    std::regex pattern(R"(let\s+(\S+)\s+(\S+)\s*=\s*(.*))"); // Matches regex of type 'let .. .. = <anything>'
    std::smatch match;
    if (std::regex_match(line, match, pattern))
    {
        //std::cout << "First token: " << match[1] << "\n";
        //std::cout << "Second token: " << match[2] << "\n";
        //std::cout << "Right-hand side: " << match[3] << "\n";
        Type *type = checkForDataType_Declaration(match[1], ctx);
        if (!builder.GetInsertBlock())
        {
            throw std::exception("Cannot declare a variable in global scope.");
        }
        if (!isValidVariableName(match[2]))
        {
            throw std::exception("Variable name contains illegal characters.");
        }
        Function *fn = builder.GetInsertBlock()->getParent();
        AllocaInst *allocaInstance = allocate_variable(fn, type, match[2].str());
        Value *variableValue = cand_eval::evaluateExpression(std::string(match[3]), type, fn, builder, ctx);
        if (!variableValue) throw std::exception("Invalid expression.");
        builder.CreateStore(variableValue, allocaInstance);
        variables[std::string(match[2])] = allocaInstance;
        return 0; // didn't actually take me long
    }
    return 1;
}
#endif