#include "compiler.hpp"
#include "caxml/caprojloader.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
using namespace llvm;
OptimizationLevel oplevel = OptimizationLevel::O3;
llvm::Value *cand_eval::evaluateExpression(std::string expr, llvm::Type *type, llvm::Function *F, llvm::IRBuilder<> &builder, llvm::LLVMContext &ctx)
{
    expr = trim(expr);
    std::pair<bool, double> isSingleVal = cand_eval::IsSingleValue(expr);
    if (isSingleVal.first)
    {
        if (type->isIntegerTy())
        {
            return llvm::ConstantInt::get(ctx, llvm::APInt(type->getIntegerBitWidth(), int(isSingleVal.second)));
        }
        else
        {
            return llvm::ConstantFP::get(ctx, llvm::APFloat(isSingleVal.second));
        }
    }
    if (expr == "nullptr")
    {
        return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(type));
    }
    llvm::Value *str = cand_eval::evaluateString(expr, builder, ctx);
    if (str != nullptr)
        return str;
    llvm::Value *fcres = cand_eval::evaulateFunctionCall(expr, type, builder, ctx);
    if (fcres != nullptr)
        return fcres;
    auto tokens = cand_eval::tokenize(expr);
    cand_eval::Parser p(tokens);
    auto root = p.parseExpression();
    llvm::Value *val = generateIR(root.get(), F, builder);
    return val;
}
std::string removeComments(const std::string &code)
{
    static const std::regex singleLine(R"(//[^\n]*)");
    static const std::regex multiLine(R"(/\*[\s\S]*?\*/)");
    std::string result = std::regex_replace(code, singleLine, "");
    result = std::regex_replace(result, multiLine, "");
    return result;
}
std::vector<std::string> splitStringBySpace(const std::string &inputString)
{
    std::vector<std::string> words;
    std::stringstream ss(inputString);
    std::string word;
    // Extract words separated by whitespace
    while (ss >> word)
    {
        words.push_back(word);
    }
    return words;
}
int ProjectMode(std::string path)
{
    while (1)
    {
        std::cout << path << " >> ";
        std::string input;
        std::getline(std::cin, input);
        std::vector<std::string> parts = splitStringBySpace(input);
        if (parts.size() < 1)
        {
            std::cout << input << "\n";
            continue;
        }
        if (parts[0] == "build")
        {
            // std::cout << "Build not avalible at the moment.\n";
            if (currentProject == nullptr)
            {
                std::cout << "Cannot load project!\n";
                continue;
            }
            int errc;
            try
            {
                errc = compile(currentProject->program.SourceFilename, currentProject->program.optimizationLevel, currentProject->program.outputPath, currentProject->program.mode);
            }
            catch (const std::exception &e)
            {
                std::cout << "Error when compiling: " << e.what() << "\n";
                errc = 1;
            }
            std::cout << "[STATUS] Compile exited with error code " << errc << ".\n";
            continue;
        }
        else if (parts[0] == "exitproj")
        {
            if (currentProject != nullptr)
                delete currentProject;
            break;
        }
        printf("\n");
    }
    return 0;
}
int MainLoop()
{
    printf(">> ");
    std::string input;
    std::getline(std::cin, input);
    std::vector<std::string> parts = splitStringBySpace(input);
    if (parts.size() < 1)
    {
        std::cout << input << "\n";
        return 0;
    }
    if (parts[0] == "example")
    {
        example();
        return 0;
    }
    else if (parts[0] == "exit")
    {
        return 2;
    }
    else if (parts[0] == "setopt")
    {
        if (parts.size() < 2)
        {
            std::cout << "Sets the optimization level for compiling.\n";
            return 0;
        }
        oplevel = checkOptimizationLevel(parts[1]);
        return 0;
    }
    else if (parts[0] == "loadproject")
    {
        if (parts.size() < 2)
        {
            std::cout << "Usage: Loads a project.\n"
                      << "Loads a project with the .caxproj file extension that is created by 'newproj'.\n";
            return 0;
        }
        if (!std::filesystem::exists(parts[1]))
        {
            std::cout << "No path named '" << parts[1] << "'.\n";
            return 0;
        }
        currentProject = LoadProject(parts[1]);
        if (currentProject->program.success == false)
        {
            delete currentProject;
            return 0;
        }
        ProjectMode(parts[1]);
    }
    else if (parts[0] == "compile")
    {
        if (parts.size() < 4)
        {
            std::cout << "Compiles a single source code file, usually with the file format '.ca'.\n";
            return 0;
        }
        if (!std::filesystem::exists(parts[1]))
        {
            std::cout << "No file named '" << parts[1] << "'.\n";
            return 0;
        }
        int errc;
        try
        {
            errc = compile(parts[1], oplevel, parts[2], parts[3]);
        }
        catch (const std::exception &e)
        {
            std::cout << "Error when compiling: " << e.what() << "\n";
            errc = 1;
        }
        std::cout << "[STATUS] Compile exited with error code " << errc << ".\n";
        return 0;
    }
    else if (parts[0] == "allVars")
    {
        for (auto &scope : variables)
        {
            for (std::pair<std::string, VariableInfo> alloca : scope)
            {
                std::cout << alloca.second.allocaInst->getName().str() << "\n";
            }
        }
        return 0;
    }
    else
    {
        std::cout << "No command named '" << input << "'.\n";
        return 0;
    }
    return 0;
}
int main()
{
    while (1)
    {
        int err = MainLoop();
        if (err == 1)
        {
            std::cout << "CAND EXIT\n";
            break;
        }
        else if (err == 2)
            break;
        printf("\n");
    }
    return 0;
}