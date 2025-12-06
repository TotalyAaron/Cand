#ifndef __RETURN_HPP_CA__
#define __RETURN_HPP_CA__
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
#include <string>
#include <regex>
#include <iostream>
#include "../let/evaulate_expr.hpp"
using namespace llvm;
static int ParseRet(const std::string &line, Type* retType, LLVMContext &ctx, Module &module, IRBuilder<> &builder, Function* function) {
    std::regex pattern("^ret\\s+(.*)$");
    std::smatch match;
    if (std::regex_match(line, match, pattern)) {
        //std::cout << "Return expression / value: " << match[1] << "\n";
        if (match[1] == "void" || std::string(match[1]).empty()) {
            builder.CreateRetVoid();
            return 0;
        }
        Value* value = cand_eval::evaluateExpression(std::string(match[1]), retType, function, builder, ctx);
        builder.CreateRet(value);
        return 0;
    }
    return 1;
}
#endif