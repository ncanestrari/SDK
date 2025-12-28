#include "gameentity_initializer.hpp"
#include "configuration_initializer.hpp"
#include "playerstats_initializer.hpp"
#include "json_init_example.hpp"
#include "json_node.hpp"
#include <fmt/core.h>
#include <unordered_map>

// Initialize ObjectRegistry with test objects
void initializeTestObjects() {
    auto& registry = ObjectRegistry::getInstance();
    registry.registerObject("MainRenderer", std::make_shared<Renderer>());
    registry.registerObject("PlayerTransform", std::make_shared<Transform>());
    registry.registerObject("GameAudio", std::make_shared<AudioSystem>());
    registry.registerObject("UITransform", std::make_shared<Transform>());
}

// Mock file reader for testing
std::unordered_map<std::string, std::string> testJsonFiles = {
    {"gameentity.json", R"({
        "name": "Player",
        "health": 100,
        "speed": 5.5,
        "isActive": true,
        "renderer": "MainRenderer",
        "transform": "PlayerTransform",
        "audioSystem": "GameAudio"
    })"},
    
    {"config.json", R"({
        "appName": "MyGame",
        "maxConnections": 50,
        "timeout": 30.0,
        "enableLogging": true,
        "logLevel": "INFO"
    })"},
    
    {"playerstats.json", R"({
        "playerName": "TestPlayer",
        "level": 25,
        "experience": 15000,
        "accuracy": 0.85,
        "isOnline": true,
        "position": "UITransform"
    })"},
    
    {"playerstats_simple.json", R"({
        "playerName": "SimplePlayer",
        "level": 10
    })"}
};

std::string mockFileReader(const std::string& path) {
    auto it = testJsonFiles.find(path);
    if (it != testJsonFiles.end()) {
        return it->second;
    }
    throw std::runtime_error("Test file not found: " + path);
}

void testGameEntityCreation() {
    fmt::print("=== Testing GameEntity Creation ===\n");
    
    try {
        JsonParser parser(mockFileReader);
        JsonNodePtr node = parser.parse(testJsonFiles["gameentity.json"]);
        
        GameEntity entity = createGameEntityFromJson(node);
        
        fmt::print("GameEntity created successfully:\n");
        fmt::print("  name: {}\n", entity.getName());
        fmt::print("  health: {}\n", entity.getHealth());
        fmt::print("  speed: {}\n", entity.getSpeed());
        fmt::print("  isActive: {}\n", entity.getIsActive() ? "true" : "false");
        fmt::print("  renderer: {}\n", entity.getRenderer() ? entity.getRenderer()->getType() : "null");
        fmt::print("  transform: {}\n", entity.getTransform() ? entity.getTransform()->getType() : "null");
        fmt::print("  audioSystem: {}\n", entity.getAudioSystem() ? entity.getAudioSystem()->getType() : "null");
        
    } catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
    }
}

void testConfigurationCreation() {
    fmt::print("\n=== Testing Configuration Creation ===\n");
    
    try {
        JsonParser parser(mockFileReader);
        JsonNodePtr node = parser.parse(testJsonFiles["config.json"]);
        
        Configuration config = createConfigurationFromJson(node);
        
        fmt::print("Configuration created successfully:\n");
        fmt::print("  appName: {}\n", config.getAppName());
        fmt::print("  maxConnections: {}\n", config.getMaxConnections());
        fmt::print("  timeout: {}\n", config.getTimeout());
        fmt::print("  enableLogging: {}\n", config.getEnableLogging() ? "true" : "false");
        fmt::print("  logLevel: {}\n", config.getLogLevel());
        
    } catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
    }
}

void testPlayerStatsCreation() {
    fmt::print("\n=== Testing PlayerStats Creation (Full Constructor) ===\n");
    
    try {
        JsonParser parser(mockFileReader);
        JsonNodePtr node = parser.parse(testJsonFiles["playerstats.json"]);
        
        PlayerStats stats = createPlayerStatsFromJson(node);
        
        fmt::print("PlayerStats created successfully:\n");
        fmt::print("  playerName: {}\n", stats.getPlayerName());
        fmt::print("  level: {}\n", stats.getLevel());
        fmt::print("  experience: {}\n", stats.getExperience());
        fmt::print("  accuracy: {}\n", stats.getAccuracy());
        fmt::print("  isOnline: {}\n", stats.getIsOnline() ? "true" : "false");
        fmt::print("  position: {}\n", stats.getPosition() ? stats.getPosition()->getType() : "null");
        
    } catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
    }
}

void testPlayerStatsSimple() {
    fmt::print("\n=== Testing PlayerStats Creation (Simple Constructor) ===\n");
    
    try {
        JsonParser parser(mockFileReader);
        JsonNodePtr node = parser.parse(testJsonFiles["playerstats_simple.json"]);
        
        PlayerStats stats = createPlayerStatsFromJson(node);
        
        fmt::print("PlayerStats (simple) created successfully:\n");
        fmt::print("  playerName: {}\n", stats.getPlayerName());
        fmt::print("  level: {}\n", stats.getLevel());
        fmt::print("  experience: {} (default)\n", stats.getExperience());
        fmt::print("  accuracy: {} (default)\n", stats.getAccuracy());
        fmt::print("  isOnline: {} (default)\n", stats.getIsOnline() ? "true" : "false");
        fmt::print("  position: {} (default)\n", stats.getPosition() ? stats.getPosition()->getType() : "null");
        
    } catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
    }
}

void testWithIncludeDirectives() {
    fmt::print("\n=== Testing with Include Directives ===\n");
    
    // Test with includes
    std::unordered_map<std::string, std::string> includeTestFiles = {
        {"entity_config.json", R"({
            "entity": {
                "$include": "gameentity.json"
            },
            "settings": {
                "$include": "config.json"
            },
            "player": {
                "$include": "playerstats.json"
            }
        })"}
    };
    
    auto includeFileReader = [&](const std::string& path) -> std::string {
        // First check include test files
        auto it = includeTestFiles.find(path);
        if (it != includeTestFiles.end()) {
            return it->second;
        }
        // Fall back to regular test files
        return mockFileReader(path);
    };
    
    try {
        JsonParser parser(includeFileReader);
        JsonNodePtr rootNode = parser.parse(includeTestFiles["entity_config.json"]);
        
        fmt::print("Parsed structure with includes:\n");
        parser.printTree(rootNode);
        
        // Extract and create objects from included JSON
        JsonNodePtr entityNode = rootNode->getChild("entity");
        if (entityNode) {
            GameEntity entity = createGameEntityFromJson(entityNode);
            fmt::print("\nCreated entity from included JSON:\n");
            fmt::print("  name: {}\n", entity.getName());
            fmt::print("  health: {}\n", entity.getHealth());
            fmt::print("  speed: {}\n", entity.getSpeed());
        }
        
        JsonNodePtr settingsNode = rootNode->getChild("settings");
        if (settingsNode) {
            Configuration config = createConfigurationFromJson(settingsNode);
            fmt::print("\nCreated config from included JSON:\n");
            fmt::print("  appName: {}\n", config.getAppName());
            fmt::print("  maxConnections: {}\n", config.getMaxConnections());
        }
        
        JsonNodePtr playerNode = rootNode->getChild("player");
        if (playerNode) {
            PlayerStats stats = createPlayerStatsFromJson(playerNode);
            fmt::print("\nCreated player stats from included JSON:\n");
            fmt::print("  playerName: {}\n", stats.getPlayerName());
            fmt::print("  level: {}\n", stats.getLevel());
            fmt::print("  experience: {}\n", stats.getExperience());
        }
        
    } catch (const std::exception& e) {
        fmt::print("Error with includes: {}\n", e.what());
    }
}

void testErrorHandling() {
    fmt::print("\n=== Testing Error Handling ===\n");
    
    try {
        JsonParser parser(mockFileReader);
        
        // Test with invalid JSON structure
        std::string invalidJson = R"({
            "name": "TestEntity",
            "health": "not_a_number",
            "speed": 5.5
        })";
        
        JsonNodePtr node = parser.parse(invalidJson);
        
        fmt::print("Testing with invalid health value (string instead of number):\n");
        GameEntity entity = createGameEntityFromJson(node);
        
        fmt::print("Entity created with defaults for invalid fields:\n");
        fmt::print("  name: {}\n", entity.getName());
        fmt::print("  health: {} (should be 0 due to type mismatch)\n", entity.getHealth());
        fmt::print("  speed: {}\n", entity.getSpeed());
        
    } catch (const std::exception& e) {
        fmt::print("Error during error handling test: {}\n", e.what());
    }
    
    // Test with missing required fields
    try {
        JsonParser parser(mockFileReader);
        
        std::string partialJson = R"({
            "name": "PartialEntity"
        })";
        
        JsonNodePtr node = parser.parse(partialJson);
        
        fmt::print("\nTesting with missing fields:\n");
        GameEntity entity = createGameEntityFromJson(node);
        
        fmt::print("Entity created with defaults for missing fields:\n");
        fmt::print("  name: {}\n", entity.getName());
        fmt::print("  health: {} (default)\n", entity.getHealth());
        fmt::print("  speed: {} (default)\n", entity.getSpeed());
        fmt::print("  isActive: {} (default)\n", entity.getIsActive() ? "true" : "false");
        
    } catch (const std::exception& e) {
        fmt::print("Error during partial JSON test: {}\n", e.what());
    }
}

void demonstrateObjectRegistryUsage() {
    fmt::print("\n=== Demonstrating ObjectRegistry Usage ===\n");
    
    // Register additional objects
    auto& registry = ObjectRegistry::getInstance();
    registry.registerObject("CustomRenderer", std::make_shared<Renderer>());
    registry.registerObject("PlayerSpawn", std::make_shared<Transform>());
    
    std::string customJson = R"({
        "name": "CustomEntity",
        "health": 150,
        "speed": 7.5,
        "isActive": true,
        "renderer": "CustomRenderer",
        "transform": "PlayerSpawn",
        "audioSystem": "GameAudio"
    })";
    
    try {
        JsonParser parser(mockFileReader);
        JsonNodePtr node = parser.parse(customJson);
        
        GameEntity entity = createGameEntityFromJson(node);
        
        fmt::print("Created entity with custom registered objects:\n");
        fmt::print("  name: {}\n", entity.getName());
        fmt::print("  renderer: {}\n", entity.getRenderer() ? entity.getRenderer()->getType() : "null");
        fmt::print("  transform: {}\n", entity.getTransform() ? entity.getTransform()->getType() : "null");
        fmt::print("  audioSystem: {}\n", entity.getAudioSystem() ? entity.getAudioSystem()->getType() : "null");
        
    } catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
    }
    
    // Test with non-existent object reference
    std::string missingObjectJson = R"({
        "name": "EntityWithMissingObject",
        "health": 100,
        "speed": 5.0,
        "isActive": true,
        "renderer": "NonExistentRenderer"
    })";
    
    try {
        JsonParser parser(mockFileReader);
        JsonNodePtr node = parser.parse(missingObjectJson);
        
        GameEntity entity = createGameEntityFromJson(node);
        
        fmt::print("\nTesting with non-existent object reference:\n");
        fmt::print("  name: {}\n", entity.getName());
        fmt::print("  renderer: {} (should be null)\n", entity.getRenderer() ? entity.getRenderer()->getType() : "null");
        
    } catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
    }
}

void testComplexConfiguration() {
    fmt::print("\n=== Testing Complex Configuration Scenarios ===\n");
    
    // Test nested includes with multiple object types
    std::unordered_map<std::string, std::string> complexTestFiles = {
        {"complex_config.json", R"({
            "game": {
                "entities": [
                    {
                        "$include": "gameentity.json"
                    },
                    {
                        "name": "Enemy1",
                        "health": 80,
                        "speed": 3.0,
                        "isActive": true,
                        "renderer": "MainRenderer"
                    }
                ],
                "settings": {
                    "$include": "config.json"
                },
                "players": {
                    "$include": "playerstats.json"
                }
            }
        })"}
    };
    
    auto complexFileReader = [&](const std::string& path) -> std::string {
        auto it = complexTestFiles.find(path);
        if (it != complexTestFiles.end()) {
            return it->second;
        }
        return mockFileReader(path);
    };
    
    try {
        JsonParser parser(complexFileReader);
        JsonNodePtr rootNode = parser.parse(complexTestFiles["complex_config.json"]);
        
        fmt::print("Complex configuration structure:\n");
        parser.printTree(rootNode);
        
        // Extract game section
        JsonNodePtr gameNode = rootNode->getChild("game");
        if (gameNode) {
            // Test entities array
            JsonNodePtr entitiesNode = gameNode->getChild("entities");
            if (entitiesNode && entitiesNode->type == JsonType::ARRAY) {
                fmt::print("\nProcessing entities array:\n");
                
                for (size_t i = 0; i < entitiesNode->elements.size(); ++i) {
                    JsonNodePtr entityNode = entitiesNode->getElement(i);
                    if (entityNode) {
                        GameEntity entity = createGameEntityFromJson(entityNode);
                        fmt::print("  Entity {}: {} (health: {})\n", 
                                 i, entity.getName(), entity.getHealth());
                    }
                }
            }
            
            // Test nested configuration
            JsonNodePtr settingsNode = gameNode->getChild("settings");
            if (settingsNode) {
                Configuration config = createConfigurationFromJson(settingsNode);
                fmt::print("\nNested configuration: {}\n", config.getAppName());
            }
            
            // Test nested player stats
            JsonNodePtr playersNode = gameNode->getChild("players");
            if (playersNode) {
                PlayerStats stats = createPlayerStatsFromJson(playersNode);
                fmt::print("Nested player: {} (level {})\n", 
                         stats.getPlayerName(), stats.getLevel());
            }
        }
        
    } catch (const std::exception& e) {
        fmt::print("Error in complex configuration test: {}\n", e.what());
    }
}

void printSummaryAndUsage() {
    fmt::print("\n=== Summary ===\n");
    fmt::print("✓ Constructor-based object creation from JSON\n");
    fmt::print("✓ Proper encapsulation with private members\n");
    fmt::print("✓ ObjectRegistry integration for Object-derived types\n");
    fmt::print("✓ Support for include directives\n");
    fmt::print("✓ Error handling for type mismatches\n");
    fmt::print("✓ Multiple constructor support\n");
    fmt::print("✓ Complex nested configurations\n");
    fmt::print("✓ Array processing with mixed data\n");
    
    fmt::print("\n=== New API Usage Examples ===\n");
    fmt::print("// Constructor-based creation (NEW)\n");
    fmt::print("GameEntity entity = createGameEntityFromJson(node);\n");
    fmt::print("Configuration config = createConfigurationFromJson(node);\n");
    fmt::print("PlayerStats stats = createPlayerStatsFromJson(node);\n");
    fmt::print("\n");
    fmt::print("// Instead of field-based initialization (OLD)\n");
    fmt::print("// GameEntity entity;\n");
    fmt::print("// initializeFromJson(entity, node);\n");
    fmt::print("\n");
    fmt::print("=== Benefits ===\n");
    fmt::print("• Proper encapsulation (private members)\n");
    fmt::print("• Immutable object creation\n");
    fmt::print("• Type-safe parameter extraction\n");
    fmt::print("• Constructor parameter mapping\n");
    fmt::print("• Cleaner API design\n");
    fmt::print("• Better error handling\n");
}

int main() {
    fmt::print("Testing Generated JSON Constructor-Based Initialization Code\n");
    fmt::print("============================================================\n");

    // Initialize test objects in the registry
    initializeTestObjects();

    // Test each class creation
    testGameEntityCreation();
    testConfigurationCreation();
    testPlayerStatsCreation();
    testPlayerStatsSimple();
    
    // Test with include directives
    testWithIncludeDirectives();
    
    // Test error handling
    testErrorHandling();
    
    // Demonstrate ObjectRegistry usage
    demonstrateObjectRegistryUsage();
    
    // Test complex scenarios
    testComplexConfiguration();
    
    // Print summary and usage information
    printSummaryAndUsage();
    
    return 0;
}
