#ifndef CAND_EVAL_EXPR_HPP
#define CAND_EVAL_EXPR_HPP

// Expression parser / evaluator that generates LLVM IR.
// variables (loaded from existing AllocaInst in function), auto-promotion.
// Uses throw_ca_err(std::string) for errors and std::cout for warnings

#include <string>
#include <vector>
#include <cctype>
#include <algorithm>
#include <memory>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <utility>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/ADT/APInt.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
struct VariableInfo
{
    llvm::Type *type;        // The type of the variable (i32, i32*, i32**, etc.)
    llvm::Type *pointeeType; // Only valid if type is a pointer (what *points to)
    llvm::AllocaInst *allocaInst;
    bool isPointer;
    VariableInfo() : type(nullptr), pointeeType(nullptr),
                     allocaInst(nullptr), isPointer(false) {}

    // Non-pointer
    VariableInfo(llvm::Type *t, llvm::AllocaInst *al)
        : type(t),
          pointeeType(nullptr),
          allocaInst(al),
          isPointer(false) {}

    // Pointer
    VariableInfo(llvm::Type *fullPointerType, llvm::Type *elemType, llvm::AllocaInst *al)
        : type(fullPointerType),
          pointeeType(elemType),
          allocaInst(al),
          isPointer(true) {}
};

std::vector<std::unordered_map<std::string, VariableInfo>> variables; // scopes
VariableInfo *lookup(const std::string &name)
{
    for (int i = variables.size() - 1; i >= 0; --i)
    {
        auto &scope = variables[i];
        auto it = scope.find(name);
        if (it != scope.end())
        {
            return &it->second;
        }
    }
    return nullptr; // not found
}

#include "../caccfg.h"
// #include "let.hpp"
#include "../fnc/fnc.hpp"
using namespace llvm;
bool usingBitCast = false;
void throw_ca_err(std::string msg)
{
    throw std::exception(msg.c_str());
}
namespace cand_eval
{
    // Default integer literal width (use i128 by default)
    static constexpr unsigned DEFAULT_LITERAL_INT_WIDTH = 128;
    // Allowed type name strings (map to LLVM types)
    inline llvm::Type *typeFromName(llvm::LLVMContext &ctx, const std::string &name)
    {
        if (name.empty())
        {
            throw_ca_err("Cannot declare an empty type.");
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
        throw_ca_err("Unknown data type '" + name + "'.");
        return nullptr;
    }
    /*inline llvm::Type *getTypeFromVariable(const std::string& name, llvm::LLVMContext &ctx)
    {
        if (variables.count(name) == 0)
        {
            throw_ca_err("Invalid variable '" + name + "'.");
            return llvm::IntegerType::get(ctx, 1);
        }
        return variables[name];
    }*/
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
                throw_ca_err(std::string("Tokenizer: unexpected character '") + c + "'");
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
                    throw_ca_err("Expected ')'");
                return e;
            }
            throw_ca_err("Invalid primary expression");
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
            throw_ca_err("widerIntType: non-integer");
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
            throw_ca_err("Invalid float literal: " + text);
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
        throw_ca_err("Unknown variable: " + name);
        return nullptr;
    }
    // Find alloca by name inside function and return a loaded value
    llvm::Value *loadVariableByName(
        llvm::Function *function,
        const std::string &name,
        llvm::IRBuilder<> &builder,
        llvm::Module &module)
    {
        VariableInfo *info = lookup(name);
        if (!info)
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
        throw_ca_err("Unknown variable: " + name);
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
        throw_ca_err("Unsupported type promotion.");
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
        if (err)
        {
            std::cout << "Error before IR generation: " << errMsg << "\n";
            return nullptr;
        }
        if (!node)
            throw_ca_err("generateIR: null AST node");
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
                    /*llvm::Value *ptr = generateIR(node->right.get(), F, builder);

                    llvm::AllocaInst *__alloca = dyn_cast<llvm::AllocaInst>(ptr);
                    if (!__alloca)
                    {
                        throw_ca_err("Cannot dereference non-alloca value");
                        return nullptr;
                    }
                    llvm::Type *elemTy = __alloca->getAllocatedType();
                    return builder.CreateLoad(elemTy, ptr, "deref");*/
                    std::string varName = node->right->name;
                    VariableInfo *info = lookup(varName);
                    if (!info)
                    {
                        throw_ca_err("Cannot dereference unknown variable/pointer.");
                        return nullptr;
                    }
                    if (!info->isPointer)
                    {
                        throw_ca_err("Cannot dereference non-pointer value.");
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
                        throw_ca_err("Cannot negate non-value.");
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
                        throw_ca_err("Unary - on unsupported type");
                        return nullptr;
                    }
                }
                if (op == '+')
                {
                    // unary plus: just return the operand value
                    return generateIR(node->right.get(), F, builder);
                }
                throw_ca_err("Unknown unary operator");
            }
            llvm::Value *L = generateIR(node->left.get(), F, builder);
            llvm::Value *R = generateIR(node->right.get(), F, builder);
            if (!L || !R)
            {
                throw_ca_err("Cannot express a non-value as a part of an expression.");
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
            throw_ca_err("Unsupported operator.");
        }
        }
        throw_ca_err("Invalid AST node kind");
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
                throw_ca_err("No function called '" + std::string(match[1]) + "'.");
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
                    throw_ca_err("Invalid argument for function call");
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