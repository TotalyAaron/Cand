#ifndef CAND_EVAL_EXPR_HPP
#define CAND_EVAL_EXPR_HPP

// Expression parser / evaluator that generates LLVM IR.
// variables (loaded from existing AllocaInst in function), auto-promotion.

#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <memory>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <utility>
#include <optional>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>

#include "../../caexception.h"
struct VariableInfo
{
    llvm::Type *type;        // The type of the variable (i32, i32*, i32**, etc.)
    llvm::Type *pointeeType; // Only valid if type is a pointer (what *points to)
    llvm::ArrayType *arrayType;
    llvm::AllocaInst *allocaInst;
    bool isPointer;
    bool isArray;
    VariableInfo() : type(nullptr), pointeeType(nullptr),
                     allocaInst(nullptr), isPointer(false), arrayType(nullptr), isArray(false) {}

    // Non-pointer
    VariableInfo(llvm::Type *t, llvm::AllocaInst *al)
        : type(t),
          pointeeType(nullptr),
          allocaInst(al),
          isPointer(false),
          arrayType(nullptr), isArray(false) {}

    // Pointer
    VariableInfo(llvm::Type *fullPointerType, llvm::Type *elemType, llvm::AllocaInst *al)
        : type(fullPointerType),
          pointeeType(elemType),
          allocaInst(al),
          isPointer(true),
          arrayType(nullptr), isArray(false) {}

    VariableInfo(llvm::ArrayType *at, llvm::AllocaInst *al)
        : type(nullptr),
        pointeeType(nullptr),
        arrayType(at),
        allocaInst(al),
        isPointer(false),
        isArray(true) {}
};

std::vector<std::unordered_map<std::string, VariableInfo>> variables; // scopes
std::optional<VariableInfo> lookup(const std::string &name)
{
    for (int i = variables.size() - 1; i >= 0; --i)
    {
        auto &scope = variables[i];
        auto it = scope.find(name);
        if (it != scope.end())
        {
            return it->second;
        }
    }
    return std::nullopt; // not found
}

#include "../caccfg.h"
// #include "let.hpp"
#include "../fnc/fnc.hpp"
using namespace llvm;
bool usingBitCast = false;
namespace cand_eval
{
    // Default integer literal width (use i128 by default)
    static constexpr unsigned DEFAULT_LITERAL_INT_WIDTH = 128;
    // Allowed type name strings (map to LLVM types)
    inline llvm::Type *typeFromName(llvm::LLVMContext &ctx, const std::string &name)
    {
        if (name.empty())
        {
            throw CAndException("Cannot declare an empty type.", "TypeError");
            return nullptr;
        }
        if (name.back() == '*')
        {
            std::string base = name.substr(0, name.size() - 1);
            Type *baseTy = checkForDataType_Declaration(base, ctx);
            if (baseTy == nullptr)
                return nullptr;
            return PointerType::getUnqual(baseTy);
        }
        if (name == "int8" || name == "char")
            return llvm::IntegerType::get(ctx, 8);
        if (name == "int16")
            return llvm::IntegerType::get(ctx, 16);
        if (name == "int32")
            return llvm::IntegerType::get(ctx, 32);
        if (name == "int64")
            return llvm::IntegerType::get(ctx, 64);
        if (name == "int128")
            return llvm::IntegerType::get(ctx, 128);
        if (name == "float32")
            return llvm::Type::getFloatTy(ctx);
        if (name == "float64")
            return llvm::Type::getDoubleTy(ctx);
        throw CAndException("Unknown data type '" + name + "'.", "TypeError");
        return nullptr;
    }
    enum class TokenKind
    {
        Number,
        Ident,
        Ref,
        Deref,
        Plus,
        Minus,
        Mul,
        Div,
        LParen,
        RParen,
        End
    };
    struct Token
    {
        TokenKind kind = TokenKind::End;
        std::string text;     // raw text for numbers/idents
        bool isFloat = false; // for numeric literal detection
    };
    static std::vector<Token> tokenize(const std::string &s)
    {
        std::vector<Token> out;
        size_t i = 0, n = s.size();
        auto peek = [&](size_t off = 0)
        { return (i + off < n) ? s[i + off] : '\0'; };
        while (i < n)
        {
            char c = s[i];
            if (std::isspace((unsigned char)c))
            {
                ++i;
                continue;
            }
            // identifier
            if (std::isalpha((unsigned char)c) || c == '_')
            {
                std::string id;
                while (i < n && (std::isalnum((unsigned char)s[i]) || s[i] == '_'))
                    id.push_back(s[i++]);
                out.push_back({TokenKind::Ident, id, false});
                continue;
            }
            // number (integer or float)
            if (std::isdigit((unsigned char)c))
            {
                std::string num;
                bool seenDot = false;
                while (i < n && (std::isdigit((unsigned char)s[i]) || s[i] == '.'))
                {
                    if (s[i] == '.')
                    {
                        if (seenDot)
                            break; // second dot stops the number
                        seenDot = true;
                    }
                    num.push_back(s[i++]);
                }
                out.push_back({TokenKind::Number, num, seenDot});
                continue;
            }
            // operators/paren
            switch (c)
            {
            case '+':
                out.push_back({TokenKind::Plus, "+"});
                ++i;
                break;
            case '-':
                out.push_back({TokenKind::Minus, "-"});
                ++i;
                break;
            case '*':
                out.push_back({TokenKind::Mul, "*"});
                ++i;
                break;
            case '/':
                out.push_back({TokenKind::Div, "/"});
                ++i;
                break;
            case '&':
                out.push_back({TokenKind::Ref, "&"});
                ++i;
                break;
            case '~':
                out.push_back({TokenKind::Deref, "~"});
                ++i;
                break;
            case '(':
                out.push_back({TokenKind::LParen, "("});
                ++i;
                break;
            case ')':
                out.push_back({TokenKind::RParen, ")"});
                ++i;
                break;
            default:
                //std::cout << c << "\n";
                throw CAndException(std::string("Unexpected character '") + c + "'", "SyntaxError");
                return {}; // unreachable but satisfies compiler
            }
        }

        out.push_back({TokenKind::End, ""});
        return out;
    }
    struct AST
    {
        enum class Kind
        {
            Number,
            Ident,
            Op
        } kind;
        // number
        std::string numberText;
        bool isFloat = false;
        // ident
        std::string name;
        // op
        char oper = 0;
        std::unique_ptr<AST> left;
        std::unique_ptr<AST> right;
        static std::unique_ptr<AST> number(const std::string &txt, bool isFloat)
        {
            auto a = std::make_unique<AST>();
            a->kind = Kind::Number;
            a->numberText = txt;
            a->isFloat = isFloat;
            return a;
        }
        static std::unique_ptr<AST> ident(const std::string &n)
        {
            auto a = std::make_unique<AST>();
            a->kind = Kind::Ident;
            a->name = n;
            return a;
        }
        static std::unique_ptr<AST> op(char oper, std::unique_ptr<AST> L, std::unique_ptr<AST> R)
        {
            auto a = std::make_unique<AST>();
            a->kind = Kind::Op;
            a->oper = oper;
            a->left = std::move(L);
            a->right = std::move(R);
            return a;
        }
    };
    class Parser
    {
    public:
        Parser(const std::vector<Token> &t) : tokens(t), pos(0) {}
        std::unique_ptr<AST> parseExpression() { return parseAddSub(); }

    private:
        const std::vector<Token> &tokens;
        size_t pos;
        const Token &peek() const { return tokens[pos]; }
        const Token &consume() { return tokens[pos++]; }
        bool match(TokenKind k)
        {
            if (tokens[pos].kind == k)
            {
                ++pos;
                return true;
            }
            return false;
        }
        std::unique_ptr<AST> parsePrimary()
        {
            if (match(TokenKind::Plus))
            {
                return parsePrimary();
            }
            if (match(TokenKind::Minus))
            {
                // unary minus
                auto operand = parsePrimary();
                // if operand is a numeric literal, fold into a single negative literal
                if (operand && operand->kind == AST::Kind::Number && !operand->isFloat)
                {
                    // prepend '-' to number text
                    operand->numberText = std::string("-") + operand->numberText;
                    return operand;
                }
                return AST::op('-', nullptr, std::move(operand)); // unary '-' node
            }

            const Token &t = peek();
            if (t.kind == TokenKind::Number)
            {
                consume();
                return AST::number(t.text, t.isFloat);
            }
            if (t.kind == TokenKind::Ident)
            {
                consume();
                return AST::ident(t.text);
            }
            if (t.kind == TokenKind::Ref) // ref
            {
                consume();
                auto operand = parsePrimary();
                return AST::op('&', nullptr, std::move(operand));
            }
            if (t.kind == TokenKind::Deref) // deref
            {
                consume();
                auto operand = parsePrimary();
                return AST::op('~', nullptr, std::move(operand));
            }
            if (match(TokenKind::LParen))
            {
                auto e = parseExpression();
                if (!match(TokenKind::RParen))
                    throw CAndException("Expected ')'", "SyntaxError");
                return e;
            }
            throw CAndException("Invalid primary expression", "SyntaxError");
            return nullptr;
        }

        std::unique_ptr<AST> parseMulDiv()
        {
            auto node = parsePrimary();
            while (true)
            {
                if (match(TokenKind::Mul))
                {
                    node = AST::op('*', std::move(node), parsePrimary());
                }
                else if (match(TokenKind::Div))
                {
                    node = AST::op('/', std::move(node), parsePrimary());
                }
                else
                    break;
            }
            return node;
        }
        std::unique_ptr<AST> parseAddSub()
        {
            auto node = parseMulDiv();
            while (true)
            {
                // std::cout << "match\n";
                if (match(TokenKind::Plus))
                {
                    node = AST::op('+', std::move(node), parseMulDiv());
                }
                else if (match(TokenKind::Minus))
                {
                    node = AST::op('-', std::move(node), parseMulDiv());
                }
                else
                    break;
            }
            return node;
        }
    };
    // Return the wider integer type between two integer LLVM types
    static llvm::Type *widerIntType(llvm::Type *a, llvm::Type *b)
    {
        if (!a->isIntegerTy() || !b->isIntegerTy())
            throw CAndException("Cannot cast on non-integers", "CastError");
        unsigned wa = llvm::cast<llvm::IntegerType>(a)->getBitWidth();
        unsigned wb = llvm::cast<llvm::IntegerType>(b)->getBitWidth();
        unsigned w = (wa > wb) ? wa : wb;
        return llvm::IntegerType::get(a->getContext(), w);
    }
    // Make integer APInt constant with the given default width (DEFAULT_LITERAL_INT_WIDTH)
    static inline llvm::Value *makeIntegerLiteral(const std::string &text, llvm::IRBuilder<> &builder)
    {
        llvm::LLVMContext &ctx = builder.getContext();
        bool negative = false;
        std::string t = text;
        if (!t.empty() && t[0] == '-')
        {
            negative = true;
            t = t.substr(1);
        }
        llvm::APInt api(DEFAULT_LITERAL_INT_WIDTH, llvm::StringRef(t), 10);
        if (negative)
        {
            api = -api; // two's complement negation
        }
        return llvm::ConstantInt::get(llvm::IntegerType::get(ctx, DEFAULT_LITERAL_INT_WIDTH), api);
    }

    // Make floating literal (double)
    static llvm::Value *makeFloatingLiteral(const std::string &text, llvm::IRBuilder<> &builder)
    {
        llvm::LLVMContext &ctx = builder.getContext();
        double v = 0.0;
        try
        {
            v = std::stod(text);
        }
        catch (...)
        {
            throw CAndException("Invalid float literal: " + text, "CastError");
        }
        return llvm::ConstantFP::get(builder.getDoubleTy(), v);
    }
    // References the value something something blah blah
    static llvm::AllocaInst *findVariableAlloca(llvm::Function *F, const std::string &name)
    {
        for (auto &BB : *F)
        {
            for (auto &I : BB)
            {
                if (auto *AI = llvm::dyn_cast<llvm::AllocaInst>(&I))
                {
                    if (AI->hasName() && AI->getName() == name)
                        return AI; // pointer, not the loaded value
                }
            }
        }
        throw CAndException("Unknown variable: " + name, "NotFoundError");
        return nullptr;
    }
    // Find alloca by name inside function and return a loaded value
    llvm::Value *loadVariableByName(
        llvm::Function *function,
        const std::string &name,
        llvm::IRBuilder<> &builder,
        llvm::Module &module)
    {
        std::optional<VariableInfo> info = lookup(name);
        if (info == std::nullopt)
            return nullptr;
        llvm::Value *alloc = info->allocaInst;
        if (llvm::AllocaInst *alloca_inst = llvm::dyn_cast<llvm::AllocaInst>(alloc))
        {
            llvm::Type *loaded_type = info->type;
            return builder.CreateLoad(
                loaded_type,
                alloc,
                name + ".load");
        }
        else
        {
            return nullptr;
        }
        // fallback: global variable
        if (llvm::GlobalVariable *GV = module.getGlobalVariable(name))
        {
            return builder.CreateLoad(
                GV->getValueType(),
                GV,
                name + ".load");
        }
        throw CAndException("Unknown variable: " + name, "NotFoundError");
        return nullptr;
    }
    // Promote value v to desired type. Handles integer widen/trunc, int<->float conversions.
    // Emits llvm instructions using builder.
    static llvm::Value *promoteTo(llvm::IRBuilder<> &builder, llvm::Value *v, llvm::Type *desired)
    {
        llvm::Type *from = v->getType();
        llvm::LLVMContext &ctx = builder.getContext();
        if (from == desired)
            return v;
        // int -> int widen/trunc
        if (from->isIntegerTy() && desired->isIntegerTy())
        {
            unsigned fw = llvm::cast<llvm::IntegerType>(from)->getBitWidth();
            unsigned dw = llvm::cast<llvm::IntegerType>(desired)->getBitWidth();
            if (fw < dw)
                return builder.CreateSExt(v, desired, "sexttmp");
            if (fw > dw)
                return builder.CreateTrunc(v, desired, "trunctmp");
        }
        // int -> float
        if (from->isIntegerTy() && desired->isFloatingPointTy())
        {
            return builder.CreateSIToFP(v, desired, "sitofptmp");
        }
        // float -> int
        if (from->isFloatingPointTy() && desired->isIntegerTy())
        {
            // we allow FP->INT conversion but it's explicit: we still do it here (use FPToSI)
            return builder.CreateFPToSI(v, desired, "fptositmp");
        }
        // float -> float (cast)
        if (from->isFloatingPointTy() && desired->isFloatingPointTy())
        {
            return builder.CreateFPCast(v, desired, "fpcasttmp");
        }
        throw CAndException("Unsupported type promotion.", "CastError");
        return nullptr;
    }
    // Determine common type between two values for binary operations
    // rules:
    //  - if either is floating -> use double (f64)
    //  - else -> use wider integer
    static llvm::Type *commonBinaryType(llvm::IRBuilder<> &builder, llvm::Value *L, llvm::Value *R)
    {
        llvm::Type *Lt = L->getType();
        llvm::Type *Rt = R->getType();

        if (Lt->isFloatingPointTy() || Rt->isFloatingPointTy())
        {
            // we choose double (float64) as the float type for operations
            return builder.getDoubleTy();
        }
        // both integers: choose wider integer
        llvm::Type *wider = widerIntType(Lt, Rt);
        // warn on implicit widening between integers if W > 4
        if (W > 4)
        {
            unsigned lw = llvm::cast<llvm::IntegerType>(Lt)->getBitWidth();
            unsigned rw = llvm::cast<llvm::IntegerType>(Rt)->getBitWidth();
            if (lw != rw)
            {
                std::cout << "Warning: implicit integer promotion from int" << lw << " and int" << rw
                          << " to int" << llvm::cast<llvm::IntegerType>(wider)->getBitWidth() << "\n";
            }
        }
        return wider;
    }
    // IR generation from AST
    static llvm::Value *generateIR(AST *node, llvm::Function *F, llvm::IRBuilder<> &builder)
    {
        if (!node)
            throw CAndException("null AST node", "ASTError");
        switch (node->kind)
        {
        case AST::Kind::Number:
        {
            if (node->isFloat)
            {
                return makeFloatingLiteral(node->numberText, builder);
            }
            else
            {
                // integer literal -> default width (i128) constant
                return makeIntegerLiteral(node->numberText, builder);
            }
        }
        case AST::Kind::Ident:
        {
            return loadVariableByName(F, node->name, builder, *builder.GetInsertBlock()->getParent()->getParent());
        }
        case AST::Kind::Op:
        {
            char op = node->oper;
            if (node->left == nullptr)
            {
                if (op == '&')
                {
                    std::string varName = node->right->name;
                    return findVariableAlloca(currentFunction, varName);
                }
                if (op == '~')
                {
                    std::string varName = node->right->name;
                    std::optional<VariableInfo> info = lookup(varName);
                    if (info == std::nullopt)
                    {
                        throw CAndException("Cannot dereference unknown variable/pointer.", "NotFoundError");
                        return nullptr;
                    }
                    if (!info->isPointer)
                    {
                        throw CAndException("Cannot dereference non-pointer value.", "PointerError");
                        return nullptr;
                    }
                    // Load the pointer stored inside the alloca
                    llvm::Value *loadedPtr =
                        builder.CreateLoad(info->type, info->allocaInst, varName + ".ptr");
                    // Use the manually-stored pointeeType
                    return builder.CreateLoad(info->pointeeType, loadedPtr, varName + ".deref");
                }
                if (op == '-')
                { // unary negation
                    llvm::Value *v = generateIR(node->right.get(), F, builder);
                    if (!v)
                    {
                        throw CAndException("Cannot negate non-value.", "NumberError");
                        return nullptr;
                    }
                    llvm::Type *ty = v->getType();
                    if (ty->isFloatingPointTy())
                    {
                        // floating negation
                        return builder.CreateFNeg(v, "fnegtmp");
                    }
                    else if (ty->isIntegerTy())
                    {
                        return builder.CreateNeg(v, "negtemp");
                    }
                    else
                    {
                        throw CAndException("Unary - on unsupported type", "NumberError");
                        return nullptr;
                    }
                }
                if (op == '+')
                {
                    // unary plus: just return the operand value
                    return generateIR(node->right.get(), F, builder);
                }
                throw CAndException("Unknown unary operator", "NumberError");
            }
            llvm::Value *L = generateIR(node->left.get(), F, builder);
            llvm::Value *R = generateIR(node->right.get(), F, builder);
            if (!L || !R)
            {
                throw CAndException("Cannot express a non-value as a part of an expression.", "NumberError");
                return nullptr;
            }
            llvm::Type *finalTy = commonBinaryType(builder, L, R);
            L = promoteTo(builder, L, finalTy);
            R = promoteTo(builder, R, finalTy);
            if (finalTy->isFloatingPointTy())
            {
                switch (node->oper)
                {
                case '+':
                    return builder.CreateFAdd(L, R, "faddtmp");
                case '-':
                    return builder.CreateFSub(L, R, "fsubtmp");
                case '*':
                    return builder.CreateFMul(L, R, "fmultmp");
                case '/':
                    return builder.CreateFDiv(L, R, "fdivtmp");
                default:
                    break;
                }
            }
            else if (finalTy->isIntegerTy())
            {
                switch (node->oper)
                {
                case '+':
                    return builder.CreateAdd(L, R, "addtmp");
                case '-':
                    return builder.CreateSub(L, R, "subtmp");
                case '*':
                    return builder.CreateMul(L, R, "multmp");
                case '/':
                    return builder.CreateSDiv(L, R, "divtmp");
                default:
                    break;
                }
            }
            throw CAndException("Unsupported operator.", "NumberError");
        }
        }
        throw CAndException("Invalid AST node kind", "ASTError");
        return nullptr;
    }
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
    llvm::Value *evaluateExpression(std::string expr, llvm::Type *type, llvm::Function *F, llvm::IRBuilder<> &builder, llvm::LLVMContext &ctx);
    // String literals
    static std::unordered_map<std::string, llvm::GlobalVariable *> StringPool;
    static llvm::Value *evaluateString(
        const std::string &expr,
        llvm::IRBuilder<> &builder, llvm::LLVMContext &ctx)
    {
        if (expr.size() < 2 || expr.front() != '"' || expr.back() != '"')
            return nullptr;
        llvm::Module *mod = builder.GetInsertBlock()->getModule();
        std::string inner = expr.substr(1, expr.size() - 2);
        if (StringPool.count(inner))
        {
            auto *var = StringPool[inner];
            llvm::Value *zero = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
            return builder.CreateInBoundsGEP(
                var->getValueType(),
                var,
                {zero, zero},
                ".strptr");
        }
        // create constant
        llvm::Constant *strConst =
            llvm::ConstantDataArray::getString(ctx, inner, true);
        std::string name = ".str_" + std::to_string(std::hash<std::string>{}(inner));
        llvm::GlobalVariable *var =
            new llvm::GlobalVariable(
                *mod,
                strConst->getType(),
                true,
                llvm::GlobalValue::PrivateLinkage,
                strConst,
                name);
        var->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        var->setAlignment(llvm::Align(1));
        StringPool[inner] = var;
        llvm::Value *zero = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
        return builder.CreateInBoundsGEP(
            var->getValueType(),
            var,
            {zero, zero},
            name + ".ptr");
    }
    static unsigned getNumberOfArgumentsInFunction(llvm::Function *Function)
    {
        unsigned count = 0;
        for (auto &arg : Function->args())
        {
            count++;
        }
        return count;
    }
    static llvm::Value *evaulateFunctionCall(const std::string &expr, llvm::Type *type, llvm::IRBuilder<> &builder, llvm::LLVMContext &ctx)
    {
        // std::cout << "Evaulating function call '" << expr << "'.\n";
        llvm::Module *mod = builder.GetInsertBlock()->getModule();
        std::regex pattern(R"(^\s*(.*?)\s*\(\s*(.*?)\s*\)\s*$)");
        std::smatch match;
        if (std::regex_match(expr, match, pattern))
        {
            llvm::Function *callee = mod->getFunction(std::string(match[1]));
            if (!callee)
            {
                throw CAndException("No function called '" + std::string(match[1]) + "'.", "NotFoundError");
                return nullptr;
            }
            // Parse the parameters
            llvm::Function *currentFn = builder.GetInsertBlock()->getParent();
            std::vector<std::string> params = split(std::string(match[2]), ',');
            std::vector<llvm::Value *> paramValues = {};
            for (std::string &s : params)
            {
                paramValues.push_back(evaluateExpression(s, type, currentFn, builder, ctx));
            }
            for (auto *v : paramValues)
                if (!v)
                    throw CAndException("Invalid argument for function call", "ArgumentError");
            llvm::Function::arg_iterator ai = callee->arg_begin();
            for (int i = 0; i < getNumberOfArgumentsInFunction(callee); i++)
            {
                llvm::Argument *arg = &(*ai);
                if (paramValues[i]->getType()->isPointerTy() && arg->getType()->isPointerTy())
                {
                    paramValues[i] = builder.CreateBitCast(paramValues[i], arg->getType(), "argcast");
                }
                paramValues[i] = promoteTo(builder, paramValues[i], arg->getType());
                ++ai;
            }
            llvm::Value *result = builder.CreateCall(callee, paramValues, "calltmp_" + std::to_string(std::hash<std::string>{}(std::string(match[1]))));
            return result;
        }
        else
            return nullptr;
    }
    struct ParsedArrayType
    {
        std::string base;
        std::vector<std::string> dims;
        ParsedArrayType() = default;
    };
    // returns true if ok, false if malformed
    static bool parseArrayType(const std::string &input, ParsedArrayType &out)
    {
        std::string s = input;
        ltrim(s);
        rtrim(s);
        out = ParsedArrayType{};
        std::string base;
        std::vector<std::string> dims;
        size_t i = 0;
        size_t n = s.size();
        // 1. Extract base type everything before first '['
        while (i < n && s[i] != '[')
        {
            base.push_back(s[i]);
            i++;
        }
        if (base.empty())
        {
            throw CAndException("Missing base type before array brackets.", "SyntaxError");
            return false;
        }
        ltrim(base);
        rtrim(base);
        out.base = base;
        // Repeatedly parse [number]
        while (i < n)
        {
            if (s[i] != '[')
            {
                throw CAndException("Unexpected character '" + std::string(1, s[i]) +
                      "' after base type '" + base + "'.", "SyntaxError");
                return false;
            }
            i++; // skip [
            std::string num;
            while (i < n && s[i] != ']')
            {
                if (!std::isspace((unsigned char)s[i]))
                {
                    if (!std::isdigit((unsigned char)s[i]))
                    {
                        throw CAndException("Array size must be numeric. Offending character: '" +
                              std::string(1, s[i]) + "'", "SyntaxError");
                        return false;
                    }
                    num.push_back(s[i]);
                }
                i++;
            }
            if (i >= n)
            {
                throw CAndException("Unclosed '[', missing ']'.", "SyntaxError");
                return false;
            }
            if (num.empty())
            {
                throw CAndException("Empty array size '[]' is not allowed.", "SyntaxError");
                return false;
            }
            dims.push_back(num);
            i++; // skip ]
        }
        out.dims = dims;
        return true;
    }
    uint64_t getIntValueFromValue(llvm::Value *value)
    {
        if (llvm::ConstantInt *CI = llvm::dyn_cast<llvm::ConstantInt>(value))
        {
            return CI->getZExtValue();
        }
        else if (llvm::ConstantFP *CFP = llvm::dyn_cast<llvm::ConstantFP>(value))
        {
            if (W > 2) { std::cout << "Conversion from float to integer, truncating decimal part.\n"; }
            return static_cast<uint64_t>(CFP->getValueAPF().convertToDouble());
        }
        else
        {
            throw CAndException("Value is not a numeric constant.", "NumberError"); return 0;
        }
    }
    llvm::ArrayType* evaulateArrayType(const std::string& expr, llvm::LLVMContext& ctx) {
        if (expr.empty() || expr.back() != ']')
            return nullptr;
        ParsedArrayType pat;
        parseArrayType(expr, pat);
        llvm::Type* baseType = checkForDataType_Declaration(pat.base, ctx);
        llvm::ArrayType* arrTy = nullptr;
        for (int i = pat.dims.size() - 1; i >= 0; --i) {
            int dim = std::stoi(pat.dims[i]);
            arrTy = arrTy
                ? llvm::ArrayType::get(arrTy, dim)
                : llvm::ArrayType::get(baseType, dim);
        }
        return arrTy;
    }
    static llvm::Value *evaulateIndexArray(const std::string &expr, llvm::IRBuilder<> &builder, llvm::LLVMContext &ctx) {
        if (expr.empty() || expr.back() != ']')
            return nullptr;
        ParsedArrayType pat = ParsedArrayType();
        parseArrayType(expr, pat);
        std::optional<VariableInfo> arrayInfo = lookup(pat.base);
        if (arrayInfo == std::nullopt) throw CAndException("Unknown variable '" + pat.base + "'.", "NotFoundError");
        if (!arrayInfo->isArray) throw CAndException(R"(No overloaded operator '[]' can match the argument lists:
            operator []: extern "C++" | extern "C++.stdlib" internal llvm-private compiled evaluateIndexArray(...)
        )", "SyntaxError");
        llvm::Type* type = arrayInfo->arrayType->getElementType();
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0);
        std::vector<llvm::Value*> allIndexValues = {zero};
        for (std::string index : pat.dims) {
            int i = 0;
            try {
                i = std::stoi(index);
            }
            catch (...) {
                throw CAndException("Cannot convert given element to 'int32'", "TypeError");
                return nullptr;
            }
            allIndexValues.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), i));
        }
        llvm::Value* val = builder.CreateGEP(
            arrayInfo->arrayType,
            arrayInfo->allocaInst,
            allIndexValues
        );
        return builder.CreateLoad(arrayInfo->arrayType->getElementType(), val);
    }
    static std::pair<bool, double> IsSingleValue(const std::string &expr)
    {
        std::istringstream iss(expr);
        double d; // Can be int, float, etc. depending on expected type
        iss >> d;
        bool success = iss.eof() && !iss.fail();
        return std::pair<bool, double>(success, d);
    }
}

#endif // CAND_EVAL_EXPR_HPP