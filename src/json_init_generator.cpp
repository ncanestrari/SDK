#include "json_init_generator.hpp"
#include <fmt/core.h>
#include <fstream>
#include <algorithm>

// JsonInitVisitor Implementation
JsonInitVisitor::JsonInitVisitor(clang::ASTContext* ctx) : context(ctx) {}

bool JsonInitVisitor::hasAnnotation(const clang::CXXRecordDecl* decl, const std::string& annotation) {
    if (!decl->hasAttrs()) {
        return false;
    }
    
    for (const auto* attr : decl->attrs()) {
        if (const auto* annotateAttr = clang::dyn_cast<clang::AnnotateAttr>(attr)) {
            if (annotateAttr->getAnnotation().str() == annotation) {
                return true;
            }
        }
    }
    return false;
}

bool JsonInitVisitor::isDerivedFromObject(clang::QualType type) {
    // Remove pointer/reference qualifiers
    clang::QualType canonicalType = type.getCanonicalType();
    if (canonicalType->isPointerType()) {
        canonicalType = canonicalType->getPointeeType();
    } else if (canonicalType->isReferenceType()) {
        canonicalType = canonicalType.getNonReferenceType();
    }
    
    const auto* recordType = canonicalType->getAs<clang::RecordType>();
    if (!recordType) {
        return false;
    }
    
    const auto* cxxRecordDecl = clang::dyn_cast<clang::CXXRecordDecl>(recordType->getDecl());
    if (!cxxRecordDecl) {
        return false;
    }
    
    // Check if this class or any of its bases is named "Object"
    if (cxxRecordDecl->getNameAsString() == "Object") {
        return true;
    }
    
    // Check base classes
    for (const auto& base : cxxRecordDecl->bases()) {
        if (isDerivedFromObject(base.getType())) {
            return true;
        }
    }
    
    return false;
}

std::string JsonInitVisitor::getFullTypeName(clang::QualType type) {
    return type.getAsString(context->getPrintingPolicy());
}

std::string JsonInitVisitor::getBaseTypeName(clang::QualType type) {
    clang::QualType baseType = type.getCanonicalType();
    
    if (baseType->isPointerType()) {
        baseType = baseType->getPointeeType();
    } else if (baseType->isReferenceType()) {
        baseType = baseType.getNonReferenceType();
    }
    
    return baseType.getAsString(context->getPrintingPolicy());
}

ParameterInfo JsonInitVisitor::analyzeParameter(const clang::ParmVarDecl* param) {
    ParameterInfo info;
    info.name = param->getNameAsString();
    
    clang::QualType paramType = param->getType();
    info.type = getFullTypeName(paramType);
    info.isPointer = paramType->isPointerType();
    info.isReference = paramType->isReferenceType();
    
    if (info.isPointer || info.isReference) {
        info.baseType = getBaseTypeName(paramType);
        info.isDerivedFromObject = isDerivedFromObject(paramType);
    } else {
        info.baseType = info.type;
        info.isDerivedFromObject = isDerivedFromObject(paramType);
    }
    
    // Check for default value
    info.hasDefaultValue = param->hasDefaultArg();
    if (info.hasDefaultValue && param->getDefaultArg()) {
        // Try to get default value as string (simplified)
        llvm::raw_string_ostream stream(info.defaultValue);
        param->getDefaultArg()->printPretty(stream, nullptr, context->getPrintingPolicy());
        stream.flush();
    }
    
    return info;
}

ConstructorInfo JsonInitVisitor::analyzeConstructor(const clang::CXXConstructorDecl* constructor) {
    ConstructorInfo info;
    info.isDefault = constructor->isDefaultConstructor();
    info.isExplicit = constructor->isExplicit();
    
    // Generate signature for debugging
    llvm::raw_string_ostream stream(info.signature);
    constructor->printName(stream);
    stream << "(";
    for (unsigned i = 0; i < constructor->getNumParams(); ++i) {
        if (i > 0) stream << ", ";
        stream << constructor->getParamDecl(i)->getType().getAsString();
    }
    stream << ")";
    stream.flush();
    
    // Collect parameters
    for (const auto* param : constructor->parameters()) {
        info.parameters.push_back(analyzeParameter(param));
    }
    
    return info;
}

const ConstructorInfo* ClassInfo::getBestConstructor() const {
    if (constructors.empty()) {
        return nullptr;
    }
    
    // Prefer non-default constructors with parameters
    for (const auto& ctor : constructors) {
        if (!ctor.isDefault && !ctor.parameters.empty()) {
            return &ctor;
        }
    }
    
    // Fall back to any constructor
    return &constructors[0];
}

bool JsonInitVisitor::VisitCXXRecordDecl(clang::CXXRecordDecl* decl) {
    if (!decl->isCompleteDefinition()) {
        return true;
    }
    
    if (hasAnnotation(decl, "initialize")) {
        ClassInfo classInfo;
        classInfo.name = decl->getNameAsString();
        classInfo.fullName = decl->getQualifiedNameAsString();
        classInfo.annotation = "initialize";
        
        // Collect constructors
        for (const auto* constructor : decl->ctors()) {
            if (constructor->getAccess() == clang::AS_public) {
                classInfo.constructors.push_back(analyzeConstructor(constructor));
            }
        }
        
        // Only add classes that have constructors
        if (!classInfo.constructors.empty()) {
            annotatedClasses.push_back(classInfo);
        }
    }
    
    return true;
}

const std::vector<ClassInfo>& JsonInitVisitor::getAnnotatedClasses() const {
    return annotatedClasses;
}

// JsonInitConsumer Implementation
JsonInitConsumer::JsonInitConsumer(clang::ASTContext* context) : visitor(context) {}

void JsonInitConsumer::HandleTranslationUnit(clang::ASTContext& context) {
    visitor.TraverseDecl(context.getTranslationUnitDecl());
}

// JsonInitAction Implementation
std::unique_ptr<clang::ASTConsumer> JsonInitAction::CreateASTConsumer(
    clang::CompilerInstance& compiler, 
    llvm::StringRef file) {
    return std::make_unique<JsonInitConsumer>(&compiler.getASTContext());
}

// CodeGenerator Implementation
CodeGenerator::CodeGenerator(const std::vector<ClassInfo>& classInfos) : classes(classInfos) {}

std::string CodeGenerator::generateParameterInitialization(const ParameterInfo& param, int index) {
    std::string nodeAccess = fmt::format("node->getChild(\"{}\")", param.name);
    std::string varName = fmt::format("param{}", index);
    
    if (param.isDerivedFromObject && (param.isPointer || param.isReference)) {
        // Object pointer/reference from registry
        return fmt::format(
            "    {} {};\n"
            "    if (auto {}Node = {}) {{\n"
            "        if ({}Node->type == JsonType::STRING) {{\n"
            "            auto obj = ObjectRegistry::getInstance().getObject({}Node->stringValue);\n"
            "            {} = std::dynamic_pointer_cast<{}>(obj);\n"
            "        }}\n"
            "    }}\n",
            param.type, varName, param.name, nodeAccess, param.name, param.name, varName, param.baseType
        );
    } else if (param.type.find("std::string") != std::string::npos) {
        // String parameter
        return fmt::format(
            "    {} {} = \"\";\n"
            "    if (auto {}Node = {}) {{\n"
            "        if ({}Node->type == JsonType::STRING) {{\n"
            "            {} = {}Node->stringValue;\n"
            "        }}\n"
            "    }}\n",
            param.type, varName, param.name, nodeAccess, param.name, varName, param.name
        );
    } else if (param.type.find("int") != std::string::npos || 
               param.type.find("long") != std::string::npos ||
               param.type.find("short") != std::string::npos) {
        // Integer parameter
        return fmt::format(
            "    {} {} = 0;\n"
            "    if (auto {}Node = {}) {{\n"
            "        if ({}Node->type == JsonType::NUMBER) {{\n"
            "            {} = static_cast<{}>({}Node->numberValue);\n"
            "        }}\n"
            "    }}\n",
            param.type, varName, param.name, nodeAccess, param.name, varName, param.type, param.name
        );
    } else if (param.type.find("double") != std::string::npos || 
               param.type.find("float") != std::string::npos) {
        // Floating point parameter
        return fmt::format(
            "    {} {} = 0.0;\n"
            "    if (auto {}Node = {}) {{\n"
            "        if ({}Node->type == JsonType::NUMBER) {{\n"
            "            {} = static_cast<{}>({}Node->numberValue);\n"
            "        }}\n"
            "    }}\n",
            param.type, varName, param.name, nodeAccess, param.name, varName, param.type, param.name
        );
    } else if (param.type.find("bool") != std::string::npos) {
        // Boolean parameter
        return fmt::format(
            "    {} {} = false;\n"
            "    if (auto {}Node = {}) {{\n"
            "        if ({}Node->type == JsonType::BOOLEAN) {{\n"
            "            {} = {}Node->booleanValue;\n"
            "        }}\n"
            "    }}\n",
            param.type, varName, param.name, nodeAccess, param.name, varName, param.name
        );
    } else {
        // Generic/unknown type
        return fmt::format(
            "    // TODO: Handle parameter '{}' of type '{}'\n"
            "    {} {}; // Default constructed\n",
            param.name, param.type, param.type, varName
        );
    }
}

std::string CodeGenerator::generateInitFunction(const ClassInfo& classInfo) {
    const ConstructorInfo* bestCtor = classInfo.getBestConstructor();
    if (!bestCtor) {
        return fmt::format(
            "// No suitable constructor found for {}\n"
            "// {} createFromJson(JsonNodePtr node) {{ /* TODO */ }}\n\n",
            classInfo.name, classInfo.fullName
        );
    }
    
    std::string result = fmt::format(
        "{} createFromJson(JsonNodePtr node) {{\n"
        "    if (!node || node->type != JsonType::OBJECT) {{\n"
        "        throw std::runtime_error(\"Expected object node for {} creation\");\n"
        "    }}\n\n",
        classInfo.fullName, classInfo.name
    );
    
    // Generate parameter extraction code
    for (size_t i = 0; i < bestCtor->parameters.size(); ++i) {
        const auto& param = bestCtor->parameters[i];
        result += generateParameterInitialization(param, i);
        result += "\n";
    }
    
    // Generate constructor call
    result += fmt::format("    return {}(", classInfo.fullName);
    for (size_t i = 0; i < bestCtor->parameters.size(); ++i) {
        if (i > 0) result += ", ";
        result += fmt::format("param{}", i);
    }
    result += ");\n";
    result += "}\n\n";
    
    return result;
}

std::string CodeGenerator::generateJsonValue(const ParameterInfo& param, int indent) {
    std::string spaces(indent * 4, ' ');
    
    if (param.isDerivedFromObject && (param.isPointer || param.isReference)) {
        return fmt::format("{}\"{}Object\"", spaces, param.name);
    } else if (param.type.find("std::string") != std::string::npos) {
        return fmt::format("{}\"example{}\"", spaces, param.name);
    } else if (param.type.find("int") != std::string::npos || 
               param.type.find("long") != std::string::npos ||
               param.type.find("short") != std::string::npos) {
        return fmt::format("{}42", spaces);
    } else if (param.type.find("double") != std::string::npos || 
               param.type.find("float") != std::string::npos) {
        return fmt::format("{}3.14", spaces);
    } else if (param.type.find("bool") != std::string::npos) {
        return fmt::format("{}true", spaces);
    } else {
        return fmt::format("{}\"defaultValue\"", spaces);
    }
}

std::string CodeGenerator::generateExampleJson(const ClassInfo& classInfo) {
    const ConstructorInfo* bestCtor = classInfo.getBestConstructor();
    if (!bestCtor) {
        return fmt::format("// No constructor found for {}\n", classInfo.name);
    }
    
    std::string result = fmt::format(
        "// Example JSON for class {} (constructor with {} parameters)\n",
        classInfo.name, bestCtor->parameters.size()
    );
    
    return result;
}

std::string CodeGenerator::generateClassHeader(const ClassInfo& classInfo) {
    std::string result = "#pragma once\n\n";
    result += "#include \"JsonNode.h\"\n\n";
    
    // Forward declaration if needed
    result += fmt::format("class {};\n\n", classInfo.name);
    
    // Function declaration - now returns object instead of void
    result += fmt::format("{} createFromJson(JsonNodePtr node);\n", classInfo.fullName);
    
    return result;
}

std::string CodeGenerator::generateClassImplementation(const ClassInfo& classInfo) {
    std::string result = fmt::format("#include \"{}_initializer.hpp\"\n", classInfo.name);
    result += "#include \"object.hpp\"\n";
    result += "#include <stdexcept>\n";
    result += "#include <memory>\n\n";
    
    // Include the actual class header (assuming it exists)
    result += fmt::format("// Include your class header here\n");
    result += fmt::format("// #include \"{}.h\"\n\n", classInfo.name);
    
    // Generate the creation function
    result += generateInitFunction(classInfo);
    
    return result;
}

void CodeGenerator::generateFiles(const std::string& outputDir) {
    // Create output directory if it doesn't exist
    std::filesystem::create_directories(outputDir);
    
    for (const auto& classInfo : classes) {
        generateClassFiles(classInfo, outputDir);
    }
}

void CodeGenerator::generateClassFiles(const ClassInfo& classInfo, const std::string& outputDir) {
    std::string className = classInfo.name;
    
    // Convert class name to lowercase for file names
    std::string lowerClassName = className;
    std::transform(lowerClassName.begin(), lowerClassName.end(), lowerClassName.begin(), ::tolower);
    
    // Generate header file
    std::string headerPath = fmt::format("{}/{}_initializer.hpp", outputDir, lowerClassName);
    std::ofstream headerFile(headerPath);
    if (!headerFile.is_open()) {
        throw std::runtime_error("Cannot open header file: " + headerPath);
    }
    headerFile << generateClassHeader(classInfo);
    headerFile.close();
    
    // Generate implementation file
    std::string implPath = fmt::format("{}/{}_initializer.cpp", outputDir, lowerClassName);
    std::ofstream implFile(implPath);
    if (!implFile.is_open()) {
        throw std::runtime_error("Cannot open implementation file: " + implPath);
    }
    implFile << generateClassImplementation(classInfo);
    implFile.close();
    
    // Generate example JSON config file based on constructor parameters
    std::string configPath = fmt::format("{}/{}_.conf", outputDir, lowerClassName);
    std::ofstream configFile(configPath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Cannot open config file: " + configPath);
    }
    
    const ConstructorInfo* bestCtor = classInfo.getBestConstructor();
    if (bestCtor) {
        configFile << "{\n";
        for (size_t i = 0; i < bestCtor->parameters.size(); ++i) {
            const auto& param = bestCtor->parameters[i];
            configFile << fmt::format("    \"{}\": {}", param.name, generateJsonValue(param, 0));
            
            if (i < bestCtor->parameters.size() - 1) {
                configFile << ",";
            }
            configFile << "\n";
        }
        configFile << "}\n";
    } else {
        configFile << "// No suitable constructor found\n{\n}\n";
    }
    configFile.close();
    
    fmt::print("Generated files for class {}:\n", className);
    fmt::print("  - {}\n", headerPath);
    fmt::print("  - {}\n", implPath);
    fmt::print("  - {}\n", configPath);
    
    if (bestCtor) {
        fmt::print("  Constructor: {} parameters\n", bestCtor->parameters.size());
        for (const auto& param : bestCtor->parameters) {
            std::string special = param.isDerivedFromObject ? " [Object-derived]" : "";
            fmt::print("    - {}: {}{}\n", param.name, param.type, special);
        }
    }
}fmt::print("Generated files for class {}:\n", className);
    fmt::print("  - {}\n", headerPath);
    fmt::print("  - {}\n", implPath);
    fmt::print("  - {}\n", configPath);
}

void CodeGenerator::generateCode(const std::string& outputPath) {
    // Legacy method - now just calls generateFiles with parent directory
    std::filesystem::path path(outputPath);
    std::string outputDir = path.parent_path().string();
    if (outputDir.empty()) outputDir = ".";
    generateFiles(outputDir + "/generated");
}

void CodeGenerator::generateHeader(const std::string& headerPath) {
    // Legacy method - now generates a combined header in generated folder
    std::filesystem::path path(headerPath);
    std::string outputDir = path.parent_path().string();
    if (outputDir.empty()) outputDir = ".";
    
    std::string generatedDir = outputDir + "/generated";
    std::filesystem::create_directories(generatedDir);
    
    std::ofstream file(generatedDir + "/all_initializers.hpp");
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open combined header file");
    }
    
    file << "#pragma once\n\n";
    file << "#include \"JsonNode.h\"\n\n";
    
    // Include all individual headers
    for (const auto& classInfo : classes) {
        std::string lowerClassName = classInfo.name;
        std::transform(lowerClassName.begin(), lowerClassName.end(), lowerClassName.begin(), ::tolower);
        file << fmt::format("#include \"{}_initializer.hpp\"\n", lowerClassName);
    }
    
    file.close();
}
