
#include "json_init_generator.hpp"

#include <fmt/core.h>
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/CommandLine.h"

using namespace clang::tooling;
using namespace llvm;

// Command line options
static cl::OptionCategory JsonInitCategory("json-init-generator options");

static cl::opt<std::string> OutputDir(
    "output-dir",
    cl::desc("Output directory for generated files"),
    cl::value_desc("directory"),
    cl::init("generated"),
    cl::cat(JsonInitCategory)
);

class JsonInitTool {
private:
    std::vector<ClassInfo> collectedClasses;
    
public:
    void processFile(const std::string& filename) {
        // Create a compilation database for a single file
        std::vector<std::string> sourceFiles = {filename};
        
        // Create compiler arguments
        std::vector<std::string> args = {
            "clang++",
            "-std=c++17",
            "-I.", // Add current directory to include path
            filename
        };
        
        // Create a simple compilation database
        auto compilationDatabase = std::make_unique<FixedCompilationDatabase>(".", args);
        
        ClangTool tool(*compilationDatabase, sourceFiles);
        
        // Create our custom action factory
        auto actionFactory = newFrontendActionFactory<JsonInitAction>();
        
        // Run the tool
        int result = tool.run(actionFactory.get());
        if (result != 0) {
            fmt::print("Error running clang tool on file: {}\n", filename);
            return;
        }
        
        fmt::print("Successfully processed file: {}\n", filename);
    }
    
    void generateOutput(const std::string& outputDir) {
        if (collectedClasses.empty()) {
            fmt::print("No annotated classes found.\n");
            return;
        }
        
        try {
            CodeGenerator generator(collectedClasses);
            generator.generateFiles(outputDir);
            
            fmt::print("Generated files in directory: {}/\n", outputDir);
            
            // Print summary
            fmt::print("\nProcessed {} annotated classes:\n", collectedClasses.size());
            for (const auto& classInfo : collectedClasses) {
                std::string lowerClassName = classInfo.name;
                std::transform(lowerClassName.begin(), lowerClassName.end(), lowerClassName.begin(), ::tolower);
                
                fmt::print("  - {} ({} fields)\n", classInfo.name, classInfo.fields.size());
                fmt::print("    Files: {}_initializer.hpp, {}_initializer.cpp, {}_.conf\n", 
                          lowerClassName, lowerClassName, lowerClassName);
            }
            
        } catch (const std::exception& e) {
            fmt::print("Error generating output: {}\n", e.what());
        }
    }
};


class CollectingJsonInitConsumer : public clang::ASTConsumer {
private:
    JsonInitVisitor visitor;
    std::vector<ClassInfo>* classes;
    
public:
    CollectingJsonInitConsumer(clang::ASTContext* context, std::vector<ClassInfo>* classContainer)
        : visitor(context), classes(classContainer) {}
    
    void HandleTranslationUnit(clang::ASTContext& context) override {
        visitor.TraverseDecl(context.getTranslationUnitDecl());
        
        // Copy found classes to our container
        const auto& foundClasses = visitor.getAnnotatedClasses();
        classes->insert(classes->end(), foundClasses.begin(), foundClasses.end());
    }
};

// Custom action that collects class information
class CollectingJsonInitAction : public clang::ASTFrontendAction {
private:
    std::vector<ClassInfo>* classes;
    
public:
    CollectingJsonInitAction(std::vector<ClassInfo>* classContainer) 
        : classes(classContainer) {}
    
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance& compiler, 
        llvm::StringRef file) override {
        
        return std::make_unique<CollectingJsonInitConsumer>(&compiler.getASTContext(), classes);
    }
};


// Factory class for creating actions
class CollectingActionFactory : public clang::tooling::FrontendActionFactory {
private:
    std::vector<ClassInfo>* classes;

public:
    CollectingActionFactory(std::vector<ClassInfo>* classContainer)
        : classes(classContainer) {}

    std::unique_ptr<clang::FrontendAction> create() override {
        return std::make_unique<CollectingJsonInitAction>(classes);
    }
};

int main(int argc, const char** argv) {
    auto expectedParser = CommonOptionsParser::create(argc, argv, JsonInitCategory);
    if (!expectedParser) {
        llvm::errs() << expectedParser.takeError();
        return 1;
    }

    CommonOptionsParser& optionsParser = expectedParser.get();

    std::vector<ClassInfo> allClasses;

    // Process each source file
    ClangTool tool(optionsParser.getCompilations(), optionsParser.getSourcePathList());

    // Create action factory that collects class information
    auto actionFactory = std::make_unique<CollectingActionFactory>(&allClasses);

    // Run the tool
    int result = tool.run(actionFactory.get());
    if (result != 0) {
        fmt::print("Error running clang tool\n");
        return result;
    }
    
    // Generate output files
    if (allClasses.empty()) {
        fmt::print("No classes with 'initialize' annotation found.\n");
        return 0;
    }
    
    try {
        CodeGenerator generator(allClasses);
        generator.generateFiles(OutputDir.getValue());
        
        fmt::print("Generated files in directory: {}/\n", OutputDir.getValue());
        
        // Print summary
        fmt::print("\nFound {} annotated classes:\n", allClasses.size());
        for (const auto& classInfo : allClasses) {
            fmt::print("  - {} ({} constructors)\n", classInfo.name, classInfo.constructors.size());
            
            std::string lowerClassName = classInfo.name;
            std::transform(lowerClassName.begin(), lowerClassName.end(), lowerClassName.begin(), ::tolower);
            
            fmt::print("    Generated:\n");
            fmt::print("      * {}/{}_initializer.hpp\n", OutputDir.getValue(), lowerClassName);
            fmt::print("      * {}/{}_initializer.cpp\n", OutputDir.getValue(), lowerClassName);
            fmt::print("      * {}/{}_.conf\n", OutputDir.getValue(), lowerClassName);
            
            const ConstructorInfo* bestCtor = classInfo.getBestConstructor();
            if (bestCtor) {
                fmt::print("    Best constructor ({} parameters):\n", bestCtor->parameters.size());
                for (const auto& param : bestCtor->parameters) {
                    std::string special = "";
                    if (param.isDerivedFromObject) {
                        special = " [Object-derived]";
                    }
                    fmt::print("      - {}: {}{}\n", param.name, param.type, special);
                }
            }
            fmt::print("\n");
        }
        
        fmt::print("Usage example:\n");
        fmt::print("  #include \"{}/gameentity_initializer.hpp\"\n", OutputDir.getValue());
        fmt::print("  JsonNodePtr node = parser.parse(jsonString);\n");
        fmt::print("  GameEntity entity = createFromJson(node);\n");
        
    } catch (const std::exception& e) {
        fmt::print("Error generating output: {}\n", e.what());
        return 1;
    }
    
    return 0;
}
