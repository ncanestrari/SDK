#include "object.hpp"

#include <fmt/core.h>

// Object class implementation
Object::Object() = default;

Object::~Object() = default;

std::string Object::getType() const {
    return "Object";
}

void Object::display() const {
    fmt::print("Generic Object\n");
}

// ObjectRegistry class implementation
ObjectRegistry::ObjectRegistry() = default;

ObjectRegistry& ObjectRegistry::getInstance() {
    static ObjectRegistry instance;
    return instance;
}

void ObjectRegistry::registerObject(const std::string& name, std::shared_ptr<Object> obj) {
    objects[name] = obj;
}

std::shared_ptr<Object> ObjectRegistry::getObject(const std::string& name) {
    auto it = objects.find(name);
    if (it != objects.end()) {
        return it->second;
    }
    return nullptr;
}

bool ObjectRegistry::removeObject(const std::string& name) {
    return objects.erase(name) > 0;
}

bool ObjectRegistry::hasObject(const std::string& name) const {
    return objects.find(name) != objects.end();
}

std::vector<std::string> ObjectRegistry::getObjectNames() const {
    std::vector<std::string> names;
    for (const auto& pair : objects) {
        names.push_back(pair.first);
    }
    return names;
}

void ObjectRegistry::clear() {
    objects.clear();
}

size_t ObjectRegistry::size() const {
    return objects.size();
}
