#pragma once

#include <string>
#include <memory>
#include "object.hpp"

// Example derived classes
class Renderer : public Object {
public:
    std::string getType() const override { return "Renderer"; }
    void display() const override {}
};

class AudioSystem : public Object {
public:
    std::string getType() const override { return "AudioSystem"; }
    void display() const override {}
};

class Transform : public Object {
public:
    std::string getType() const override { return "Transform"; }
    void display() const override {}
};

// Annotated classes for code generation
class __attribute__((annotate("initialize"))) GameEntity {
private:
    std::string name;
    int health;
    double speed;
    bool isActive;
    std::shared_ptr<Renderer> renderer;        // Object pointer - should use registry
    Transform* transform;                       // Object pointer - should use registry
    std::shared_ptr<AudioSystem> audioSystem;  // Object pointer - should use registry

public:
    // Constructor that will be used for JSON initialization
    GameEntity(const std::string& name, 
               int health, 
               double speed, 
               bool isActive,
               std::shared_ptr<Renderer> renderer = nullptr,
               Transform* transform = nullptr,
               std::shared_ptr<AudioSystem> audioSystem = nullptr)
        : name(name), health(health), speed(speed), isActive(isActive),
          renderer(renderer), transform(transform), audioSystem(audioSystem) {}
    
    // Getters
    const std::string& getName() const { return name; }
    int getHealth() const { return health; }
    double getSpeed() const { return speed; }
    bool getIsActive() const { return isActive; }
    std::shared_ptr<Renderer> getRenderer() const { return renderer; }
    Transform* getTransform() const { return transform; }
    std::shared_ptr<AudioSystem> getAudioSystem() const { return audioSystem; }
};

class __attribute__((annotate("initialize"))) Configuration {
private:
    std::string appName;
    int maxConnections;
    double timeout;
    bool enableLogging;
    std::string logLevel;

public:
    Configuration(const std::string& appName,
                  int maxConnections,
                  double timeout,
                  bool enableLogging = true,
                  const std::string& logLevel = "INFO")
        : appName(appName), maxConnections(maxConnections), timeout(timeout),
          enableLogging(enableLogging), logLevel(logLevel) {}
    
    // Getters
    const std::string& getAppName() const { return appName; }
    int getMaxConnections() const { return maxConnections; }
    double getTimeout() const { return timeout; }
    bool getEnableLogging() const { return enableLogging; }
    const std::string& getLogLevel() const { return logLevel; }
};

class __attribute__((annotate("initialize"))) PlayerStats {
private:
    std::string playerName;
    int level;
    long experience;
    float accuracy;
    bool isOnline;
    std::shared_ptr<Transform> position;  // Object pointer - should use registry

public:
    PlayerStats(const std::string& playerName,
                int level,
                long experience,
                float accuracy,
                bool isOnline = false,
                std::shared_ptr<Transform> position = nullptr)
        : playerName(playerName), level(level), experience(experience),
          accuracy(accuracy), isOnline(isOnline), position(position) {}
    
    // Alternative constructor with fewer parameters
    PlayerStats(const std::string& playerName, int level)
        : playerName(playerName), level(level), experience(0), 
          accuracy(0.0f), isOnline(false), position(nullptr) {}
    
    // Getters
    const std::string& getPlayerName() const { return playerName; }
    int getLevel() const { return level; }
    long getExperience() const { return experience; }
    float getAccuracy() const { return accuracy; }
    bool getIsOnline() const { return isOnline; }
    std::shared_ptr<Transform> getPosition() const { return position; }
};

// Non-annotated class (should be ignored by generator)
class RegularClass {
public:
    std::string data;
    int value;
};
