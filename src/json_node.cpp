#include "json_node.hpp"
#include <fmt/core.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <filesystem>

// JsonNode Implementation
JsonNode::JsonNode(JsonType t) : type(t), numberValue(0.0), booleanValue(false) {}

JsonNodePtr JsonNode::createNull() {
    return std::make_shared<JsonNode>(JsonType::NULL_TYPE);
}

JsonNodePtr JsonNode::createString(const std::string& value) {
    auto node = std::make_shared<JsonNode>(JsonType::STRING);
    node->stringValue = value;
    return node;
}

JsonNodePtr JsonNode::createNumber(double value) {
    auto node = std::make_shared<JsonNode>(JsonType::NUMBER);
    node->numberValue = value;
    return node;
}

JsonNodePtr JsonNode::createBoolean(bool value) {
    auto node = std::make_shared<JsonNode>(JsonType::BOOLEAN);
    node->booleanValue = value;
    return node;
}

JsonNodePtr JsonNode::createObject() {
    return std::make_shared<JsonNode>(JsonType::OBJECT);
}

JsonNodePtr JsonNode::createArray() {
    return std::make_shared<JsonNode>(JsonType::ARRAY);
}

void JsonNode::addChild(const std::string& key, JsonNodePtr child) {
    if (type != JsonType::OBJECT) {
        throw std::runtime_error("Cannot add child to non-object node");
    }
    children[key] = child;
}

void JsonNode::addElement(JsonNodePtr element) {
    if (type != JsonType::ARRAY) {
        throw std::runtime_error("Cannot add element to non-array node");
    }
    elements.push_back(element);
}

JsonNodePtr JsonNode::getChild(const std::string& key) const {
    auto it = children.find(key);
    return (it != children.end()) ? it->second : nullptr;
}

JsonNodePtr JsonNode::getElement(size_t index) const {
    return (index < elements.size()) ? elements[index] : nullptr;
}

// JsonParser Implementation
JsonParser::JsonParser(std::function<std::string(const std::string&)> reader) 
    : pos(0), fileReader(reader ? reader : defaultFileReader) {}

std::string JsonParser::defaultFileReader(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

void JsonParser::skipWhitespace() {
    while (pos < json.length() && std::isspace(json[pos])) {
        pos++;
    }
}

char JsonParser::peek() const {
    return (pos < json.length()) ? json[pos] : '\0';
}

char JsonParser::consume() {
    return (pos < json.length()) ? json[pos++] : '\0';
}

bool JsonParser::match(const std::string& str) {
    if (pos + str.length() > json.length()) return false;
    
    for (size_t i = 0; i < str.length(); i++) {
        if (json[pos + i] != str[i]) return false;
    }
    pos += str.length();
    return true;
}

std::string JsonParser::parseString() {
    if (consume() != '"') {
        throw std::runtime_error("Expected '\"' at start of string");
    }

    std::string result;
    while (pos < json.length()) {
        char c = consume();
        if (c == '"') break;
        if (c == '\\') {
            char escaped = consume();
            switch (escaped) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += escaped; break;
            }
        } else {
            result += c;
        }
    }
    return result;
}

double JsonParser::parseNumber() {
    size_t start = pos;
    
    if (peek() == '-') consume();
    
    if (!std::isdigit(peek())) {
        throw std::runtime_error("Invalid number format");
    }
    
    while (std::isdigit(peek())) consume();
    
    if (peek() == '.') {
        consume();
        while (std::isdigit(peek())) consume();
    }
    
    if (peek() == 'e' || peek() == 'E') {
        consume();
        if (peek() == '+' || peek() == '-') consume();
        while (std::isdigit(peek())) consume();
    }
    
    return std::stod(json.substr(start, pos - start));
}

JsonNodePtr JsonParser::parseValue(const std::string& basePath) {
    skipWhitespace();
    
    char c = peek();
    
    if (c == '"') {
        return JsonNode::createString(parseString());
    } else if (c == '-' || std::isdigit(c)) {
        return JsonNode::createNumber(parseNumber());
    } else if (match("true")) {
        return JsonNode::createBoolean(true);
    } else if (match("false")) {
        return JsonNode::createBoolean(false);
    } else if (match("null")) {
        return JsonNode::createNull();
    } else if (c == '[') {
        return parseArray(basePath);
    } else if (c == '{') {
        return parseObject(basePath);
    } else {
        throw std::runtime_error("Unexpected character: " + std::string(1, c));
    }
}

JsonNodePtr JsonParser::parseArray(const std::string& basePath) {
    consume(); // '['
    skipWhitespace();
    
    auto arrayNode = JsonNode::createArray();
    
    if (peek() == ']') {
        consume();
        return arrayNode;
    }
    
    while (true) {
        arrayNode->addElement(parseValue(basePath));
        skipWhitespace();
        
        if (peek() == ']') {
            consume();
            break;
        } else if (peek() == ',') {
            consume();
            skipWhitespace();
        } else {
            throw std::runtime_error("Expected ',' or ']' in array");
        }
    }
    
    return arrayNode;
}

JsonNodePtr JsonParser::parseObject(const std::string& basePath) {
    consume(); // '{'
    skipWhitespace();
    
    auto objectNode = JsonNode::createObject();
    
    if (peek() == '}') {
        consume();
        return objectNode;
    }
    
    while (true) {
        skipWhitespace();
        std::string key = parseString();
        
        skipWhitespace();
        if (consume() != ':') {
            throw std::runtime_error("Expected ':' after object key");
        }
        
        // Check for include directive
        if (key == "$include") {
            return processInclude(basePath);
        }
        
        JsonNodePtr value = parseValue(basePath);
        objectNode->addChild(key, value);
        
        skipWhitespace();
        if (peek() == '}') {
            consume();
            break;
        } else if (peek() == ',') {
            consume();
            skipWhitespace();
        } else {
            throw std::runtime_error("Expected ',' or '}' in object");
        }
    }
    
    return objectNode;
}

JsonNodePtr JsonParser::processInclude(const std::string& basePath) {
    skipWhitespace();
    JsonNodePtr includeValue = parseValue(basePath);
    
    if (includeValue->type == JsonType::STRING) {
        // Single include
        std::string includePath = includeValue->stringValue;
        std::string resolvedPath = resolvePath(includePath, basePath);
        JsonNodePtr includedNode = loadIncludedFile(resolvedPath);
        includedNode->metadata.isIncluded = true;
        includedNode->metadata.includePath = includePath;
        return includedNode;
    } else if (includeValue->type == JsonType::ARRAY) {
        // Multiple includes - merge them
        auto mergedNode = JsonNode::createObject();
        
        for (const auto& element : includeValue->elements) {
            if (element->type == JsonType::STRING) {
                std::string includePath = element->stringValue;
                std::string resolvedPath = resolvePath(includePath, basePath);
                JsonNodePtr includedNode = loadIncludedFile(resolvedPath);
                
                if (includedNode->type == JsonType::OBJECT) {
                    // Merge object properties
                    for (const auto& [key, value] : includedNode->children) {
                        mergedNode->addChild(key, value);
                    }
                }
            }
        }
        
        mergedNode->metadata.isIncluded = true;
        return mergedNode;
    }
    
    throw std::runtime_error("Include directive must be a string or array of strings");
}

std::string JsonParser::resolvePath(const std::string& includePath, const std::string& basePath) {
    if (includePath.empty()) return includePath;
    
    // Absolute path or URL
    if (includePath[0] == '/' || includePath.find("://") != std::string::npos) {
        return includePath;
    }
    
    // Relative path
    if (basePath.empty()) {
        return includePath;
    }
    
    std::filesystem::path base(basePath);
    std::filesystem::path include(includePath);
    return (base.parent_path() / include).string();
}

JsonNodePtr JsonParser::loadIncludedFile(const std::string& path) {
    // Check cache first
    auto it = includeCache.find(path);
    if (it != includeCache.end()) {
        return it->second;
    }
    
    try {
        std::string content = fileReader(path);
        JsonParser parser(fileReader);
        JsonNodePtr node = parser.parse(content, std::filesystem::path(path).parent_path().string());
        includeCache[path] = node;
        return node;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load included file '" + path + "': " + e.what());
    }
}

JsonNodePtr JsonParser::parse(const std::string& jsonString, const std::string& basePath) {
    json = jsonString;
    pos = 0;
    return parseValue(basePath);
}

std::string JsonParser::toString(JsonNodePtr node, int indent) const {
    std::string spaces(indent * 2, ' ');
    
    switch (node->type) {
        case JsonType::NULL_TYPE:
            return "null";
        case JsonType::STRING:
            return "\"" + escapeString(node->stringValue) + "\"";
        case JsonType::NUMBER:
            return std::to_string(node->numberValue);
        case JsonType::BOOLEAN:
            return node->booleanValue ? "true" : "false";
        case JsonType::ARRAY: {
            if (node->elements.empty()) return "[]";
            
            std::string result = "[\n";
            for (size_t i = 0; i < node->elements.size(); ++i) {
                result += std::string((indent + 1) * 2, ' ');
                result += toString(node->elements[i], indent + 1);
                if (i < node->elements.size() - 1) result += ",";
                result += "\n";
            }
            result += spaces + "]";
            return result;
        }
        case JsonType::OBJECT: {
            if (node->children.empty()) return "{}";
            
            std::string result = "{\n";
            size_t count = 0;
            for (const auto& [key, value] : node->children) {
                result += std::string((indent + 1) * 2, ' ');
                result += "\"" + escapeString(key) + "\": ";
                result += toString(value, indent + 1);
                if (++count < node->children.size()) result += ",";
                result += "\n";
            }
            result += spaces + "}";
            return result;
        }
    }
    return "";
}

void JsonParser::printTree(JsonNodePtr node, int indent) const {
    std::string spaces(indent * 2, ' ');
    std::string includedInfo = node->metadata.isIncluded ? 
        fmt::format(" [included from: {}]", node->metadata.includePath) : "";

    switch (node->type) {
        case JsonType::NULL_TYPE:
            fmt::print("{}null{}\n", spaces, includedInfo);
            break;
        case JsonType::STRING:
            fmt::print("{}\"{}\"{}\n", spaces, node->stringValue, includedInfo);
            break;
        case JsonType::NUMBER:
            fmt::print("{}{}{}\n", spaces, node->numberValue, includedInfo);
            break;
        case JsonType::BOOLEAN:
            fmt::print("{}{}{}\n", spaces, node->booleanValue ? "true" : "false", includedInfo);
            break;
        case JsonType::ARRAY:
            fmt::print("{}[{}\n", spaces, includedInfo);
            for (const auto& element : node->elements) {
                printTree(element, indent + 1);
            }
            fmt::print("{}]\n", spaces);
            break;
        case JsonType::OBJECT:
            fmt::print("{}{{{}\n", spaces, includedInfo);
            for (const auto& [key, value] : node->children) {
                fmt::print("{}  \"{}\":\n", spaces, key);
                printTree(value, indent + 2);
            }
            fmt::print("{}}}\n", spaces);
            break;
    }
}

std::string JsonParser::escapeString(const std::string& str) const {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}
