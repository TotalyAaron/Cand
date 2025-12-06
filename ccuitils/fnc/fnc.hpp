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
static Type *checkForDataType_Declaration(std::string dataType, LLVMContext &ctx)
{
    ltrim(dataType);
    if (dataType.back() == '*')
    {
        std::string base = dataType.substr(0, dataType.size() - 1);
        Type *baseTy = checkForDataType_Declaration(base, ctx);
        if (baseTy == nullptr)
            return nullptr;
        return PointerType::getUnqual(baseTy);
    }
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
        return Type::getFloatTy(ctx); // (getFloat32Ty)
    else if (dataType == "float64")
        return Type::getDoubleTy(ctx); // (getFloat64Ty)
    if (W > 1)
    {
        std::cout << "Warning: Invalid data type '" << dataType << "'. Defaulting to int1 (boolean).\n";
    }
    return Type::getInt1Ty(ctx); // nothing, invalid type
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
        std::vector<std::string> args = FNCPrivateUtils::split(std::string(match[2]), ',');
        std::unordered_map<std::string, Type *> parameters = {};
        for (std::string &arg : args)
        {
            arg = trim(arg);
            std::vector<std::string> tokens = FNCPrivateUtils::parseParameterLine(arg);
            Type *data_type = checkForDataType_Declaration(tokens[0], ctx);
            std::cout << tokens[1] << std::endl;
            if (!isValidVariableName(tokens[1]))
            {
                throw std::exception("Variable name contains illegal characters.");
            }
            auto /* std::pair<std::iterator<..>, bool> */ success = parameters.insert({tokens[1], data_type});
            if (!success.second)
            {
                if (W > 1)
                {
                    if (WError) {
                        std::string err = "Repeated argument name at function '" + std::string(match[1]) + "', argument '" + arg + "'.";
                        throw std::exception(err.c_str());
                    }
                    std::cout << "Warning: Repeated argument found at function '" << match[2] << "', argument '" << arg << "', skipped insertion of argument.\n";
                }
            }
        }
        // Get all data types
        std::vector<Type *> allTypes = {};
        for (const std::pair<std::string, Type *> &pair : parameters)
        {
            allTypes.push_back(pair.second);
        }
        // Get all parameter names
        std::vector<std::string> allParameterNames = {};
        for (const std::pair<std::string, Type *> &pair : parameters)
        {
            allParameterNames.push_back(pair.first);
        }
        // Add the function
        Type *return_data_type = !std::string(match[3]).empty() ? checkForDataType_Declaration(std::string(match[3]), ctx) : Type::getVoidTy(ctx) /* returns nothing if well... no return value duh */;
        FunctionType *fnctype = FunctionType::get(return_data_type, allTypes, false);
        Function *function = Function::Create(fnctype, Function::ExternalLinkage, Twine(match[1]), module);
        currentFunction = function;
        // Set the builder's insert point so it actually knows where to go :skull:, spent hours tryin to figure out why the alloca is scatte/aring everywhere
        BasicBlock *entry = BasicBlock::Create(ctx, "entry", function);
        builder.SetInsertPoint(entry);
        int args_iterator_index = 0;
        for (Argument &arg : function->args())
        {
            arg.setName(allParameterNames[args_iterator_index]);
            // Create an alloca for the parameter
            AllocaInst *alloca = allocate_variable(function, arg.getType(), arg.getName().str()); //builder.CreateAlloca(arg.getType(), nullptr, arg.getName().str());
            // Store initial parameter value into the alloca
            builder.CreateStore(&arg, alloca);
            // Register this alloca inside variable table
            variables[arg.getName().str()] = alloca;
            args_iterator_index++;
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