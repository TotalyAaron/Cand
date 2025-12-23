#ifndef __COMPILER_HPP__
#define __COMPILER_HPP__
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
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/Error.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <string>
#include <filesystem>
#include <vector>
#include "ccuitils/nkw/utils.hpp"
#include "caxml/caprojloader.hpp"
using namespace llvm;
int example()
{
    LLVMContext ctx;
    Module module("printf_example", ctx);
    IRBuilder<> builder(ctx);
    // Create the printf function declaration
    std::vector<Type *> printfArgs = {PointerType::get(Type::getInt8Ty(ctx), 0)};
    FunctionType *printfType = FunctionType::get(builder.getInt32Ty(), printfArgs, true);
    Function *printfFunc = Function::Create(printfType, Function::ExternalLinkage, "printf", module);
    // Create main function: int main()
    FunctionType *mainType = FunctionType::get(builder.getInt32Ty(), false);
    Function *mainFn = Function::Create(mainType, Function::ExternalLinkage, "main", module);
    BasicBlock *entry = BasicBlock::Create(ctx, "entry", mainFn);
    builder.SetInsertPoint(entry);
    // Create a global string: "Hello, LLVM!\n"
    Value *helloStr = builder.CreateGlobalStringPtr("Hello, LLVM!\n");
    // Call printf with the string
    builder.CreateCall(printfFunc, helloStr);
    // Return 0 from main
    builder.CreateRet(builder.getInt32(0));
    // Print generated LLVM IR
    module.print(llvm::outs(), nullptr);
    return 0;
}

/*
OPTIMIZATIONS
optimizeModule(llvm::Module &module, llvm::OptimizationLevel optimizationLevel) -> void
*/

void optimizeModule(llvm::Module &module, llvm::OptimizationLevel &oplevel)
{
    llvm::PassBuilder pb;
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    llvm::ModulePassManager mpm;
    mpm = pb.buildPerModuleDefaultPipeline(oplevel);
    mpm.run(module, mam);
}

/*
EMIT OBJECT FILE FOR MACHINE CODE
emitObjFile(llvm::Module &module, const std::string &filename) -> return bool
*/

bool emitObjFile(llvm::Module &module, const std::string &filename)
{
    using namespace llvm;
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    std::string err;
    auto triple = sys::getDefaultTargetTriple();
    module.setTargetTriple(Triple(triple));
    const Target *target = TargetRegistry::lookupTarget(triple, err);
    if (!target)
    {
        errs() << "Target lookup error: " << err << "\n";
        return false;
    }
    TargetOptions opt;
    auto cpu = sys::getHostCPUName();
    auto features = "";
    std::optional<Reloc::Model> relocModel;
    auto tm = target->createTargetMachine(triple, cpu, features, opt, relocModel);
    module.setDataLayout(tm->createDataLayout());
    std::error_code ec;
    raw_fd_ostream out(filename, ec, sys::fs::OF_None);
    if (ec)
    {
        errs() << "Cannot open output file: " << ec.message() << "\n";
        return false;
    }
    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, out, nullptr, CodeGenFileType::ObjectFile))
    {
        errs() << "TargetMachine cannot emit object files!\n";
        return false;
    }
    pm.run(module);
    out.flush();
    return true;
}

/*
EMIT ASSEMBLER
*/

bool emitAssembly(llvm::Module &module, const std::string &filename) {
    using namespace llvm;
    outs() << "code >> " << filename << "\n";
    //InitializeNativeTarget();
    //InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();
    std::string err;
    auto triple = sys::getDefaultTargetTriple();
    module.setTargetTriple(Triple(triple));
    const Target *target = TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        errs() << "Target lookup error: " << err << "\n";
        return false;
    }
    TargetOptions opt;
    auto cpu = sys::getHostCPUName();
    auto features = "";
    std::optional<Reloc::Model> relocModel;
    auto tm = target->createTargetMachine(triple, cpu, features, opt, relocModel);
    module.setDataLayout(tm->createDataLayout());
    std::error_code ec;
    raw_fd_ostream out(filename, ec, sys::fs::OF_None);
    if (ec) {
        errs() << "Cannot open output file: " << ec.message() << "\n";
        return false;
    }
    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, out, nullptr, CodeGenFileType::AssemblyFile)) {
        errs() << "TargetMachine cannot emit assembly!\n";
        return false;
    }
    pm.run(module);
    out.flush();
    return true;
}

bool isWhitespace(const std::string &str)
{
    return std::all_of(str.begin(), str.end(), [](char c)
                       { return std::isspace(static_cast<unsigned char>(c)); });
}

/*
INIT MODULE DATA
*/

void SetupModule(Module &module) {
    if (currentProject == nullptr) return;
    module.setSourceFileName(currentProject->program.SourceFilename);
    module.setModuleIdentifier(currentProject->program.ModuleID);
}
/*
C& COMPILER
compile(filePath) -> return CACompileStatus
*/

#include "ccuitils/let/let.hpp"
#include "ccuitils/fnc/fnc.hpp"
#include "ccuitils/return/return.hpp"
#include "ccuitils/unrkw/curlybracet.hpp"
#include "ccuitils/importextrn/importextrn.hpp"
#include "ccuitils/global/global.hpp"
#include "ccuitils/other/assign.hpp"
/*
PARSING
*/

// COMPILE

int compile(std::filesystem::path filePath, llvm::OptimizationLevel &oplevel, std::string outputPath, std::string mod)
{
    if (filePath.extension().string() == ".ca")
    {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::string err = "File '" + filePath.string() + "' does not exist.";
            throw std::exception(err.c_str());
        }
        std::vector<std::string> allLines = removeComments(extractAllLinesFromIfstream(file));
        // Setup LLVM structs
        LLVMContext ctx;
        Module module("main", ctx);
        IRBuilder<> builder(ctx);
        for (int i = 0; i < allLines.size(); i++)
        {
            allLines[i] = trim(allLines[i]);
            if (isWhitespace(allLines[i]))
                continue;
            //std::cout << allLines[i] << "\n";
            if (ParseImportExtern(allLines[i], ctx, module, builder) == 0) continue;
            if (ParseGlobal(allLines[i], ctx, module, builder) == 0) continue;
            if (ParseLet(allLines[i], ctx, module, builder) == 0) continue;
            if (ParseFnc(allLines[i], ctx, module, builder) == 0) continue;
            if (currentFunction != nullptr)
            {
                if (ParseRet(allLines[i], currentFunction->getFunctionType()->getReturnType(), ctx, module, builder, currentFunction) == 0) continue;
            }
            if (ParseEndingCurlyBracet(allLines[i], ctx) == 0) continue;
			if (ParseAssignment(allLines[i], ctx, module, builder) == 0) continue;
            llvm::Value *exprres = cand_eval::evaluateExpression(allLines[i], builder.getInt32Ty(), currentFunction, builder, ctx);
            if (exprres != nullptr)
                continue;
            std::string errmsg = "Invalid line '" + std::string(allLines[i]) + "'";
            throw CAndException(errmsg, "InvalidLineError");
        }
        if (llvm::verifyModule(module, &llvm::errs()))
        {
            llvm::errs() << "LINK FATAL ERROR: \n";
            module.print(llvm::errs(), nullptr);
            throw std::exception("Fatal error when linking: failed to verify module.");
        }
        optimizeModule(module, oplevel);
        //module.print(llvm::outs(), nullptr);
        /*if (mod == "-o" || mod == "-obj")
        {
            //module.print(llvm::errs(), nullptr);
            bool success = emitObjFile(module, outputPath);
            if (!success)
                return CACompileStatus(1, "Fatal error when emiiting binary.");
        }*/
        if (mod == "-ll" || mod == "-bc")
        {
            std::error_code EC;
            llvm::raw_fd_ostream out(outputPath, EC, llvm::sys::fs::OF_None);
            if (EC)
            {
                llvm::errs() << "Cannot open file: " << EC.message() << "\n";
            }
            else
            {
                llvm::WriteBitcodeToFile(module, out);
            }
        }
        else if (mod == "-s") {
            bool success = emitAssembly(module, outputPath);
            if (!success)
                throw std::exception("Fatal error when emitting assembler.");
        }
        return 0;
    }
    return 0;
}
#endif