#include "json_node.hpp"
#include <fmt/core.h>
#include <unordered_map>
#include <stdexcept>

int main() {
    // Mock file system for testing
    std::unordered_map<std::string, std::string> fileSystem = {
        {"config/database.json", R"({"host": "localhost", "port": 5432, "database": "myapp"})"},
        {"data/admin-user.json", R"({"id": 1, "name": "Administrator", "email": "admin@example.com"})"},
        {"permissions/user-permissions.json", R"({"read": true, "write": false, "admin": false})"},
        {"features/auth.json", R"({"enabled": true, "provider": "oauth2", "settings": {"timeout": 3600}})"},
        {"features/logging.json", R"({"level": "info", "output": "file", "rotation": true})"}
    };

    // Mock file reader function
    auto mockFileReader = [&fileSystem](const std::string& path) -> std::string {
        auto it = fileSystem.find(path);
        if (it != fileSystem.end()) {
            return it->second;
        }
        throw std::runtime_error("File not found: " + path);
    };

    JsonParser parser(mockFileReader);

    // Example 1: Single includes
    std::string singleIncludeJson = R"({
        "name": "MyApplication",
        "version": "1.0.0",
        "config": {
            "$include": "config/database.json"
        },
        "users": [
            {
                "$include": "data/admin-user.json"
            }
        ],
        "defaultPermissions": {
            "$include": "permissions/user-permissions.json"
        }
    })";

    // Example 2: Multiple includes (merging)
    std::string multipleIncludeJson = R"({
        "name": "MyApplication",
        "version": "2.0.0",
        "features": {
            "$include": ["features/auth.json", "features/logging.json"]
        },
        "environment": "production"
    })";

    try {
        fmt::print("=== Example 1: Single Includes ===\n");
        JsonNodePtr rootNode1 = parser.parse(singleIncludeJson);
        
        fmt::print("\nParsed tree structure:\n");
        parser.printTree(rootNode1);
        
        fmt::print("\nConverted back to JSON:\n");
        fmt::print("{}\n", parser.toString(rootNode1));

        fmt::print("\n=== Example 2: Multiple Includes (Merging) ===\n");
        JsonNodePtr rootNode2 = parser.parse(multipleIncludeJson);
        
        fmt::print("\nParsed tree structure:\n");
        parser.printTree(rootNode2);
        
        fmt::print("\nConverted back to JSON:\n");
        fmt::print("{}\n", parser.toString(rootNode2));

        fmt::print("\n=== Accessing Data Programmatically ===\n");
        
        // Access nested data
        JsonNodePtr config = rootNode1->getChild("config");
        if (config) {
            JsonNodePtr host = config->getChild("host");
            if (host && host->type == JsonType::STRING) {
                fmt::print("Database host: {}\n", host->stringValue);
            }
        }

        // Access array elements
        JsonNodePtr users = rootNode1->getChild("users");
        if (users && users->type == JsonType::ARRAY) {
            JsonNodePtr firstUser = users->getElement(0);
            if (firstUser) {
                JsonNodePtr userName = firstUser->getChild("name");
                if (userName && userName->type == JsonType::STRING) {
                    fmt::print("First user name: {}\n", userName->stringValue);
                }
            }
        }

        // Check merged features
        JsonNodePtr features = rootNode2->getChild("features");
        if (features) {
            JsonNodePtr authEnabled = features->getChild("enabled");
            JsonNodePtr logLevel = features->getChild("level");
            
            if (authEnabled && authEnabled->type == JsonType::BOOLEAN) {
                fmt::print("Auth enabled: {}\n", authEnabled->booleanValue ? "true" : "false");
            }
            
            if (logLevel && logLevel->type == JsonType::STRING) {
                fmt::print("Log level: {}\n", logLevel->stringValue);
            }
        }
        
    } catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
        return 1;
    }

    return 0;
}
