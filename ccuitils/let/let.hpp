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
        // std::cout << "First token: " << match[1] << "\n";
        // std::cout << "Second token: " << match[2] << "\n";
        // std::cout << "Right-hand side: " << match[3] << "\n";
        Type *type = checkForDataType_Declaration(match[1], ctx);
        if (!type)
            throw std::exception("Invalid type.");
        Function *fn = builder.GetInsertBlock()->getParent();
        AllocaInst *allocaInstance = allocate_variable(fn, type, match[2].str());
        Value *variableValue =
            cand_eval::evaluateExpression(match[3].str(), type, fn, builder, ctx);
        if (!variableValue)
            throw std::exception("Invalid expression.");
        builder.CreateStore(variableValue, allocaInstance);
        VariableInfo info;
        if (type->isPointerTy())
        {
            // full type is 'type'
            llvm::Type *baseType = nullptr;
            // manually extract the base type because opaque pointers can't:
            std::string t = match[1].str(); // "int32*""
            while (!t.empty() && t.back() == '*')
                t.pop_back();
            baseType = checkForDataType_Declaration(t, ctx);
            info = VariableInfo(type, baseType, allocaInstance);
        }
        else
        {
            info = VariableInfo(type, allocaInstance);
        }
        variables.back()[match[2].str()] = info;
        return 0; // didn't actually take me long
    }
    return 1;
}
#endif