#ifndef __INSERT_VARIABLE_HPP_CA__
#define __INSERT_VARIABLE_HPP_CA__
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
#include <string>
using namespace llvm;
AllocaInst* allocate_variable(Function* fn, Type* ty, std::string varName) {
    BasicBlock &entryBlock = fn->getEntryBlock();
    IRBuilder<> entryBuilder(&entryBlock, entryBlock.begin());
    AllocaInst* alloc = entryBuilder.CreateAlloca(ty, nullptr, varName);
    return alloc;
}
#endif