#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration
class JsonNode;
using JsonNodePtr = std::shared_ptr<JsonNode>;

enum class JsonType {
    NULL_TYPE,
    STRING,
    NUMBER,
    BOOLEAN,
    OBJECT,
    ARRAY
};

struct NodeMetadata {
    std::string includePath;
    bool isIncluded = false;
};

class JsonNode {
public:
    JsonType type;
    std::string stringValue;
    double numberValue;
    bool booleanValue;
    std::unordered_map<std::string, JsonNodePtr> children; // For objects
    std::vector<JsonNodePtr> elements; // For arrays
    NodeMetadata metadata;

    explicit JsonNode(JsonType t = JsonType::NULL_TYPE);

    // Factory methods
    static JsonNodePtr createNull();
    static JsonNodePtr createString(const std::string& value);
    static JsonNodePtr createNumber(double value);
    static JsonNodePtr createBoolean(bool value);
    static JsonNodePtr createObject();
    static JsonNodePtr createArray();

    // Node manipulation
    void addChild(const std::string& key, JsonNodePtr child);
    void addElement(JsonNodePtr element);
    JsonNodePtr getChild(const std::string& key) const;
    JsonNodePtr getElement(size_t index) const;
};

class JsonParser {
private:
    std::string json;
    size_t pos;
    std::unordered_map<std::string, JsonNodePtr> includeCache;
    std::function<std::string(const std::string&)> fileReader;

    // Parsing utilities
    void skipWhitespace();
    char peek() const;
    char consume();
    bool match(const std::string& str);

    // Value parsers
    std::string parseString();
    double parseNumber();
    JsonNodePtr parseValue(const std::string& basePath = "");
    JsonNodePtr parseArray(const std::string& basePath = "");
    JsonNodePtr parseObject(const std::string& basePath = "");

    // Include processing
    JsonNodePtr processInclude(const std::string& basePath);
    std::string resolvePath(const std::string& includePath, const std::string& basePath);
    JsonNodePtr loadIncludedFile(const std::string& path);

    // Utility
    std::string escapeString(const std::string& str) const;

public:
    explicit JsonParser(std::function<std::string(const std::string&)> reader = nullptr);
    
    static std::string defaultFileReader(const std::string& path);

    JsonNodePtr parse(const std::string& jsonString, const std::string& basePath = "");
    std::string toString(JsonNodePtr node, int indent = 0) const;
    void printTree(JsonNodePtr node, int indent = 0) const;
};
