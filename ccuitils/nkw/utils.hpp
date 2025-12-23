#ifndef __UTILS_HPP_CA__
#define __UTILS_HPP_CA__
#include <regex>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
std::string removeComments(const std::string& code);
// Function that accepts a vector<string>, for use
std::vector<std::string> removeComments(const std::vector<std::string>& lines) {
    std::vector<std::string> cleaned;
    cleaned.reserve(lines.size());
    for (const auto& line : lines) {
        cleaned.push_back(removeComments(line));
    }
    return cleaned;
}
std::vector<std::string> extractAllLinesFromIfstream(std::ifstream& file) {
    std::vector<std::string> allLines;
    std::string line;
    while (std::getline(file, line)) {
        allLines.push_back(line);
    }
    return allLines;
}
std::string ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r\f\v");
    return (start == std::string::npos) ? "" : s.substr(start);
}
std::string rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\n\r\f\v");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
std::string trim(const std::string& s) {
    return ltrim(rtrim(s));
}
#include "../let/evaulate_expr.hpp"
using namespace llvm;
void storeValueIntoArray(LLVMContext& ctx, IRBuilder<>& builder, const cand_eval::ParsedArrayType& parsedArrayType, Value* valueToStore, ArrayType* arrTy, AllocaInst* arrAlloca) {
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0); // ?????? why 0
    std::vector<llvm::Value*> allIndexValues = {zero};
    for (std::string index : parsedArrayType.dims) {
        long i = 0;
        try {
            i = std::stol(index);
        }
        catch (...) {
            throw CAndException("Cannot convert given element to 'int64'", "TypeError");
            return;
        }
        allIndexValues.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), i));
    }
    std::hash<std::string> hasher;
	size_t hashValue = hasher(parsedArrayType.base);
    llvm::Value* elementPtr = builder.CreateInBoundsGEP(
        arrTy,           // element type
        arrAlloca,       // pointer to the array
        allIndexValues, // indices
        "array.elem" + Twine(std::to_string(hashValue)) // some Twine name??
    );
    auto* gep = cast<GetElementPtrInst>(elementPtr);
    if (!gep)
		throw CAndException("Failed to create GEP instruction for array element.", "GEPError");
    Type* elemTy = gep->getResultElementType();
	valueToStore = cand_eval::promoteTo(builder, valueToStore, elemTy);
	builder.CreateStore(valueToStore, elementPtr);
    return;
}
#endif