#ifndef __IMPORT_EXTRN_HPP__
#define __IMPORT_EXTRN_HPP__
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
#include <regex>
#include <string>
#include <iostream>
#include "../let/let.hpp"
#include "../fnc/fnc.hpp"
#include "../nkw/utils.hpp"
using namespace llvm;
class IMPEXTERN
{
public:
    static inline Function *ParseFNCImport(std::smatch &match, LLVMContext &ctx, Module &module)
    {
        Type *ty = std::string(match[4]) == "void" ? Type::getVoidTy(ctx) : checkForDataType_Declaration(std::string(match[4]), ctx);
        std::vector<std::string> params = FNCPrivateUtils::split(match[3], ',');
        std::vector<Type *> paramTypes = {};
        for (std::string tok : params)
        {
            if (tok.empty())
                continue;
            paramTypes.push_back(checkForDataType_Declaration(tok, ctx));
        }
        FunctionType *fty = FunctionType::get(ty, paramTypes, false);
        Function *f = Function::Create(fty, Function::ExternalLinkage, std::string(match[2]), module);
        return f;
    }
};
static inline int ParseImportExtern(std::string &line, LLVMContext &ctx, Module &module, IRBuilder<> &builder)
{
    static const std::regex pattern(R"(^\s*importextrn\s+(\w+)\s+(\w+)\s*\(\s*([^\)]*)\s*\)\s*->\s*([A-Za-z0-9_\*\s&]+)\s*$)",
                                        std::regex::ECMAScript);
    std::smatch match;
    if (std::regex_match(line, match, pattern))
    {
        //std::cout << "First: " << match[1] << "\n";
        //std::cout << "Second: " << match[2] << "\n";
        //std::cout << "Inside (): " << match[3] << "\n";
        //std::cout << "After ->: " << match[4] << "\n";
        if (match[1] == "fnc")
        {
            Function *f = IMPEXTERN::ParseFNCImport(match, ctx, module);
            return 0;
        }
        else
        {
            std::string err = "No extern type called '" + std::string(match[1]) + "'.";
            throw CAndException(err);
        }
    }
    return 1;
}
#endif