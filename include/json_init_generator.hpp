#pragma once

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>
#include <string>
#include <vector>

struct ParameterInfo {
    std::string name;
    std::string type;
    bool isPointer;
    bool isReference;
    bool isDerivedFromObject;
    std::string baseType; // For pointers/references, the underlying type
    bool hasDefaultValue;
    std::string defaultValue;
};

struct ConstructorInfo {
    std::vector<ParameterInfo> parameters;
    bool isDefault;
    bool isExplicit;
    std::string signature;
};

struct ClassInfo {
    std::string name;
    std::string fullName; // Including namespace
    std::vector<ConstructorInfo> constructors;
    std::vector<std::string> fields;
    std::string annotation;
    
    // Get the best constructor for JSON initialization
    const ConstructorInfo* getBestConstructor() const;
};

class JsonInitVisitor : public clang::RecursiveASTVisitor<JsonInitVisitor> {
private:
    clang::ASTContext* context;
    std::vector<ClassInfo> annotatedClasses;
    
    bool hasAnnotation(const clang::CXXRecordDecl* decl, const std::string& annotation);
    bool isDerivedFromObject(clang::QualType type);
    std::string getFullTypeName(clang::QualType type);
    std::string getBaseTypeName(clang::QualType type);
    ParameterInfo analyzeParameter(const clang::ParmVarDecl* param);
    ConstructorInfo analyzeConstructor(const clang::CXXConstructorDecl* constructor);

public:
    explicit JsonInitVisitor(clang::ASTContext* ctx);
    
    bool VisitCXXRecordDecl(clang::CXXRecordDecl* decl);
    
    const std::vector<ClassInfo>& getAnnotatedClasses() const;
};

class JsonInitConsumer : public clang::ASTConsumer {
private:
    JsonInitVisitor visitor;
    
public:
    explicit JsonInitConsumer(clang::ASTContext* context);
    
    void HandleTranslationUnit(clang::ASTContext& context) override;
};

class JsonInitAction : public clang::ASTFrontendAction {
public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance& compiler, 
        llvm::StringRef file
    ) override;
};

class CodeGenerator {
private:
    std::vector<ClassInfo> classes;
    
    std::string generateInitFunction(const ClassInfo& classInfo);
    std::string generateParameterInitialization(const ParameterInfo& param, int index);
    std::string generateExampleJson(const ClassInfo& classInfo);
    std::string generateJsonValue(const ParameterInfo& param, int indent = 1);
    std::string generateClassHeader(const ClassInfo& classInfo);
    std::string generateClassImplementation(const ClassInfo& classInfo);
    
public:
    explicit CodeGenerator(const std::vector<ClassInfo>& classInfos);
    
    void generateFiles(const std::string& outputDir);
    void generateClassFiles(const ClassInfo& classInfo, const std::string& outputDir);

    void generateCode(const std::string& outputPath);
    void generateHeader(const std::string& headerPath);
};
