#ifndef __OTHER__ASSIGN_HPP__
#define __OTHER__ASSIGN_HPP__
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include <regex>
#include <string>
#include "../let/evaulate_expr.hpp"
using namespace llvm;
static int ParseAssignment(const std::string& line, LLVMContext& ctx, Module& module, IRBuilder<>& builder) {
	std::regex pattern(R"(\s*(\S+)\s*=\s*(.*))"); // Matches regex of type '<varname> = <expression>'
	std::smatch match;
	if (std::regex_match(line, match, pattern))
	{
		//std::cout << "Variable name: " << match[1] << "\n";
		//std::cout << "Right-hand side: " << match[2] << "\n";
		Function* fn = builder.GetInsertBlock()->getParent();
		// Find the variable in the symbol table
		// Is it an array ( checks syntax )?
		cand_eval::ParsedArrayType ty;
		bool isArray = cand_eval::parseArrayType(match[1].str(), ty);
		//std::cout << isArray << " " << ty.base << "\n";
		// Load the variable
		std::string variableName = isArray ? ty.base : match[1].str();
		AllocaInst* allocaInst = cand_eval::findVariableAlloca(fn, variableName);
		if (!allocaInst)
			throw CAndException("Variable not found: " + variableName, "NotFoundError");
		Type* typeToSearch = nullptr;
		if (!isArray) typeToSearch = allocaInst ? allocaInst->getAllocatedType() : nullptr;
		else {
			for (int i = 0; i < ty.dims.size(); i++) {
				if (i == 0) {
					typeToSearch = allocaInst ? allocaInst->getAllocatedType() : nullptr;
					continue;
				}
				if (!typeToSearch)
					throw CAndException("Invalid assignment target type.", "TypeError");
				if (typeToSearch->isArrayTy()) {
					typeToSearch = dyn_cast<ArrayType>(typeToSearch)->getElementType();
					continue;
				}
				throw CAndException("Cannot index non-array type.", "TypeError");
				return 1;
			}
		}
		Value* variableValue =
			cand_eval::evaluateExpression(match[2].str(), typeToSearch, fn, builder, ctx);
		if (!variableValue)
			throw CAndException("Invalid expression.", "ExpressionError");
		// Check if variable exists and type matches
		ArrayType* ArrayTy = dyn_cast<ArrayType>(allocaInst->getAllocatedType());
		if (ArrayTy) {
			if (ArrayTy->getArrayNumElements() > 1) {}
			else {
				throw CAndException("Variable is not an array: " + variableName, "TypeError");
				return 1;
			}
		}
		else throw CAndException("Variable not found: " + variableName, "NotFoundError");
		if (!allocaInst) {
			throw CAndException("Variable not found: " + variableName, "NotFoundError");
			return 1;
		}
		// Store the value into the variable
		if (!isArray) builder.CreateStore(variableValue, allocaInst);
		else {
			storeValueIntoArray(ctx, builder, ty, variableValue, dyn_cast<ArrayType>(allocaInst->getAllocatedType()), allocaInst);
		}
		return 0;
	}
	return 1;
}
#endif