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
static int ParseLet(const std::string& line, LLVMContext& ctx, Module& module, IRBuilder<>& builder)
{
    std::regex pattern(R"(let\s+(\S+)\s+(\S+)\s*=\s*(.*))");
    std::smatch match;
    if (!std::regex_match(line, match, pattern)) return 1;
    //std::cout << match[1].str() << "\n";
    //std::cout << match[2].str() << "\n";
    //std::cout << match[3].str() << "\n";
    Type* declType = checkForDataType_Declaration(match[1].str(), ctx);
    if (!declType) throw CAndException("Invalid type.", "TypeError");
    Function* fn = builder.GetInsertBlock()->getParent();
    if (ArrayType* arrTy = cand_eval::evaulateArrayType(match[3].str(), ctx))
    {
        // allocate array ONCE (took me a million years to figure out what was wrong with my array :skull:)
        AllocaInst* allocaInstance =
            builder.CreateAlloca(arrTy, nullptr, match[2].str());
        VariableInfo info(arrTy, allocaInstance);
        info.isArray = true;
        variables.back()[match[2].str()] = info;
        return 0;
    }
    AllocaInst* allocaInstance = allocate_variable(fn, declType, match[2].str());
    Value* rhsValue = cand_eval::evaluateExpression(match[3].str(), declType, fn, builder, ctx);
    if (!rhsValue) throw CAndException("Invalid expression.", "ExpressionError");
    builder.CreateStore(rhsValue, allocaInstance);
    VariableInfo info;
    if (declType->isPointerTy())
    {
        std::string t = match[1].str();
        while (!t.empty() && t.back() == '*')
            t.pop_back();

        Type* baseType = checkForDataType_Declaration(t, ctx);
        info = VariableInfo(declType, baseType, allocaInstance);
    }
    else
    {
        info = VariableInfo(declType, allocaInstance);
    }
    variables.back()[match[2].str()] = info;
    return 0;
}

#endif