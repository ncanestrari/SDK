#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Generic Object base class that can be inherited
class Object {
public:
    Object();
    virtual ~Object();
    
    // Virtual method for runtime type identification
    virtual std::string getType() const;
    
    // Virtual method that derived classes can override
    virtual void display() const;
};

// Singleton map that stores string names with pointers to Object
class ObjectRegistry {
private:
    std::unordered_map<std::string, std::shared_ptr<Object>> objects;
    
    // Private constructor for singleton
    ObjectRegistry();
    
public:
    // Delete copy constructor and assignment operator
    ObjectRegistry(const ObjectRegistry&) = delete;
    ObjectRegistry& operator=(const ObjectRegistry&) = delete;
    
    // Static method to get the singleton instance
    static ObjectRegistry& getInstance();
    
    // Register an object with a name
    void registerObject(const std::string& name, std::shared_ptr<Object> obj);
    
    // Get an object by name
    std::shared_ptr<Object> getObject(const std::string& name);
    
    // Remove an object by name
    bool removeObject(const std::string& name);
    
    // Check if an object exists
    bool hasObject(const std::string& name) const;
    
    // Get all registered object names
    std::vector<std::string> getObjectNames() const;
    
    // Clear all objects
    void clear();
    
    // Get the number of registered objects
    size_t size() const;
};
