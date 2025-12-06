#ifndef __GLOBAL_HPP_CA__
#define __GLOBAL_HPP_CA__
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
#include "../let/evaulate_expr.hpp"
#include "../let/let.hpp"
using namespace llvm;
std::vector<std::string> split(const std::string& s, const char delimiter) {
    std::vector<std::string> tokens;
    size_t pos_start = 0, pos_end;
    std::string token;
    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + 1;
        if (!token.empty()) tokens.push_back(token);
    }
    tokens.push_back(s.substr(pos_start)); // Add the last token
    return tokens;
}
static int ParseGlobal(const std::string &line, LLVMContext &ctx, Module &module, IRBuilder<> &builder)
{
    std::regex pattern(R"(^\s*global\s+([A-Za-z_]\w*(?:\s*[\*\&])*)\s+(\w+)\s*$)");
    std::smatch match;
    if (!std::regex_match(line, match, pattern))
        return 1;
    std::string typeStr = match[1];
    std::string nameStr = match[2];
    typeStr = trim(typeStr);
    nameStr = trim(nameStr);
    Type* type = checkForDataType_Declaration(typeStr, ctx);
    if (!type) {
        std::string err = "Invalid data type '" + typeStr + "'.";
        throw std::exception(err.c_str());
    }

    // If the type string ends with '*', create a pointer type
    if (!typeStr.empty() && typeStr.back() == '*')
        type = type->getPointerTo();

    // Create a matching initializer
    Constant* init = nullptr;
    if (type->isPointerTy()) {
        init = ConstantPointerNull::get(cast<PointerType>(type));
    } else if (type->isIntegerTy()) {
        init = ConstantInt::get(type, 0);
    } else if (type->isFloatingPointTy()) {
        init = ConstantFP::get(type, 0.0);
    } else {
        std::string err = "Unsupported type for global variable '" + nameStr + "'.";
        throw std::exception(err.c_str());
    }
    auto* GV = new GlobalVariable(
        module,
        type,
        false,
        GlobalValue::ExternalLinkage, // linkage
        init,
        nameStr
    );
    return 0;
}
#endif