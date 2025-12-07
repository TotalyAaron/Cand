#ifndef __FNC_HPP__
#define __FNC_HPP__
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
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include "../caccfg.h"
#include "../let/evaulate_expr.hpp"
#include "../let/insert_variable.hpp"
#include "../nkw/utils.hpp"
enum class _TYPE_NESTED_
{
    FNC,
    IF,
    FOR,
    WHILE
};
using namespace llvm;
static bool is_inside_function = false;
static Function *currentFunction = nullptr;
static std::vector<_TYPE_NESTED_> scopes = {};
void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                    { return !std::isspace(ch); }));
}
static bool isValidVariableName(const std::string &name)
{
    if (name.empty())
    {
        return false;
    }
    // Starts with a letter or underscore
    if (!std::isalpha(name[0]) && name[0] != '_')
    {
        return false;
    }
    // Subsequent characters are letters, digits, or underscores
    for (size_t i = 1; i < name.length(); ++i)
    {
        if (!std::isalnum(name[i]) && name[i] != '_')
        {
            return false;
        }
    }
    return true;
}
struct ParsedType
{
    llvm::Type *llvmType;    // the real pointer type
    llvm::Type *pointeeType; // nullptr if not a pointer
};
static Type *checkForDataType_Declaration(std::string dataType, LLVMContext &ctx)
{
    ltrim(dataType);
    rtrim(dataType);
    if (dataType.empty())
        return nullptr; // invalid like "***"
    // Handle pointer types (chain as many * as needed)
    if (!dataType.empty() && dataType.back() == '*')
    {
        std::string base = dataType.substr(0, dataType.size() - 1);
        Type *baseTy = checkForDataType_Declaration(base, ctx);
        if (!baseTy)
            return nullptr;
        return PointerType::getUnqual(baseTy);
    }
    // Primitive types
    if (dataType == "int1" || dataType == "bool")
        return Type::getInt1Ty(ctx);
    else if (dataType == "int8")
        return Type::getInt8Ty(ctx);
    else if (dataType == "int16")
        return Type::getInt16Ty(ctx);
    else if (dataType == "int32")
        return Type::getInt32Ty(ctx);
    else if (dataType == "int64")
        return Type::getInt64Ty(ctx);
    else if (dataType == "int128")
        return Type::getInt128Ty(ctx);
    else if (dataType == "float32")
        return Type::getFloatTy(ctx);
    else if (dataType == "float64")
        return Type::getDoubleTy(ctx);

    if (W > 1)
        std::cout << "Warning: Invalid data type '" << dataType
                  << "'. Defaulting to int1.\n";

    return Type::getInt1Ty(ctx);
}
class FNCPrivateUtils
{
public:
    static std::vector<std::string> split(const std::string &s, char delimiter)
    {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s); // Create a stringstream from the input string
        while (std::getline(tokenStream, token, delimiter))
        {
            tokens.push_back(token);
        }
        return tokens;
    }
    static std::vector<std::string> parseParameterLine(const std::string &input_string)
    {
        std::vector<std::string> result_vector;
        size_t first_space_pos = input_string.find(' '); // Finds the position of the first space
        if (first_space_pos != std::string::npos)
        {
            // Extract the first word (the data type blah blah)
            result_vector.push_back(input_string.substr(0, first_space_pos));
            // Extract the rest of the string after the first space (the argument name blah blah)
            result_vector.push_back(input_string.substr(first_space_pos + 1));
        }
        else
        {
            return std::vector<std::string>{"null", "arg"};
        }
        return result_vector;
    }
};
static int ParseFnc(const std::string &line, LLVMContext &ctx, Module &module, IRBuilder<> &builder)
{
    std::regex pattern(R"(fnc\s+(\w+)\s*->\s*\(\s*([^)]*)\s*\)\s*\(\s*([^)]*)\s*\)\s*\{)"); // Matches regex of type 'fnc .. -> (..)(..) {"
    std::smatch match;
    if (std::regex_match(line, match, pattern))
    {
        if (is_inside_function)
        {
            throw std::exception("Cannot declare a function inside of a function.");
        }
        is_inside_function = true;
        scopes.push_back(_TYPE_NESTED_::FNC);
        // std::cout << "Function name: " << match[1] << "\n";
        // std::cout << "Arguments: " << match[2] << "\n";
        // std::cout << "Return value: " << match[3] << "\n";
        /* Parsing the input */
        /// Arguments
        // --- parse args preserving order ---
        std::vector<std::string> rawArgs = FNCPrivateUtils::split(std::string(match[2]), ',');
        std::vector<llvm::Type *> paramTypes;                        // in-order LLVM types for FunctionType
        std::vector<std::string> paramNames;                         // in-order parameter names
        std::vector<std::pair<std::string, std::string>> parsedArgs; // (typeStr, nameStr) for later reuse
        for (auto &raw : rawArgs)
        {
            std::string arg = trim(raw); // keep your trim helper
            if (arg.empty())
                continue; // skip empty slots (e.g. no args)
            auto tokens = FNCPrivateUtils::parseParameterLine(arg);
            std::string typeStr = tokens[0];
            std::string nameStr = tokens[1];
            // validate name
            if (!isValidVariableName(nameStr))
            {
                throw std::exception("Variable name contains illegal characters.");
            }
            // Resolve the type (this returns the LLVM Type* for "int32" or "int32*" etc)
            Type *resolved = checkForDataType_Declaration(typeStr, ctx);
            if (!resolved)
            {
                std::string err = "Unknown type for parameter '" + nameStr + "' in function '" + std::string(match[1]) + "'";
                throw std::exception(err.c_str());
            }
            // push into ordered lists
            paramTypes.push_back(resolved);
            paramNames.push_back(nameStr);
            parsedArgs.push_back({typeStr, nameStr});
        }
        Type *returnType = !std::string(match[3]).empty()
                               ? checkForDataType_Declaration(std::string(match[3]), ctx)
                               : Type::getVoidTy(ctx);
        if (!returnType)
            throw std::exception("Invalid return type.");

        FunctionType *fnctype = FunctionType::get(returnType, paramTypes, false);
        Function *function = Function::Create(fnctype, Function::ExternalLinkage, Twine(match[1]), module);
        currentFunction = function;
        // Set up entry block and builder
        BasicBlock *entry = BasicBlock::Create(ctx, "entry", function);
        builder.SetInsertPoint(entry);
        // push a new scope for parameters
        variables.push_back({});
        // Assign arg names and create allocas *in the same order as paramNames*
        size_t idx = 0;
        for (Argument &arg : function->args())
        {
            arg.setName(paramNames[idx]);
            // allocate a stack slot for the parameter (use the declared LLVM type)
            AllocaInst *alloca = allocate_variable(function, arg.getType(), arg.getName().str());
            // store the incoming argument value into the alloca
            builder.CreateStore(&arg, alloca);
            VariableInfo info;
            Type *declType = arg.getType(); // full LLVM type (may be pointer, pointer-of-pointer, ...)
            if (declType->isPointerTy())
            {
                // derive pointee type from the parsed source text, not from LLVM pointer (opaque)
                std::string typeStr = parsedArgs[idx].first;

                // remove trailing '*' characters to get base type
                while (!typeStr.empty() && typeStr.back() == '*')
                    typeStr.pop_back();
                Type *pointee = checkForDataType_Declaration(typeStr, ctx);
                if (!pointee)
                {
                    throw std::exception("Cannot resolve pointee type for parameter.");
                }
                info = VariableInfo(declType, pointee, alloca);
            }
            else
            {
                // non-pointer
                info = VariableInfo(declType, alloca);
            }
            // register under the actual parameter name
            variables.back()[arg.getName().str()] = info;

            ++idx;
        }

        return 0; // yayyy... took 2 days
    }
    return 1;
}
static int CheckFunction_isValid(Function &function, raw_os_ostream &out)
{
    if (verifyFunction(function, &out))
    {
        throw std::exception("Function failed to build because of a syntax error or other.");
    }
    return 0;
}
#endif