# SDK

A C++17 library providing essential utilities for game development and application infrastructure, including JSON parsing with include support, memory management, task scheduling, and automatic JSON initialization code generation.

## Features

### 1. JSON Parser with Include Support
A flexible JSON parser that supports file inclusion and merging, enabling modular configuration files.

- **Standard JSON parsing** with support for objects, arrays, strings, numbers, booleans, and null
- **File inclusion** via `$include` directive for single or multiple files
- **Automatic merging** of multiple included JSON objects
- **Path resolution** with support for relative paths
- **Caching** to avoid re-parsing included files
- Tree traversal and programmatic access to parsed data

### 2. Memory Manager
A high-performance memory pool allocator with power-of-2 size categories for efficient object allocation.

- **Pool-based allocation** with automatic pool creation for different object sizes
- **Thread-safe** operations with mutex protection
- **Power-of-2 alignment** for optimal memory usage
- **Custom allocators** for STL containers
- **Statistics tracking** for allocations, deallocations, and pool usage
- Support for objects up to 1MB in size
- **Optional global new/delete override** for application-wide memory management

### 3. Task Scheduler
A single-threaded asynchronous task scheduler for fire-and-forget operations and future-based results.

- **Fire-and-forget** task execution
- **Future-based** results for async operations
- **Thread-safe** task queue
- Support for functions with parameters
- Task count monitoring
- Graceful shutdown with task completion waiting

### 4. Object Registry
A singleton registry for managing named objects with runtime type identification.

- **Global object storage** with string-based lookup
- **Shared pointer management** for automatic lifetime handling
- Type-safe object retrieval
- Registry introspection (list all objects, check existence)

### 5. JSON Initialization Code Generator
A Clang-based tool that automatically generates JSON initialization code for annotated C++ classes.

- **Automatic code generation** from class annotations
- **Constructor analysis** to select the best initialization path
- **Type-aware initialization** with support for primitives, strings, and Object-derived pointers
- **Object registry integration** for pointer member initialization
- **Example JSON generation** with proper type formatting
- Separate file generation per class (header, implementation, and config)

## Building

### Requirements

- CMake 3.15 or later
- C++17 compatible compiler (GCC, Clang, MSVC)
- [fmt](https://github.com/fmtlib/fmt) library
- LLVM/Clang development libraries (for code generator)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

Or use the provided build script:

```bash
./build_and_test.sh
```

## Usage Examples

### JSON Parser with Includes

```cpp
#include "json_node.hpp"

// Define your file system or use default file reader
JsonParser parser;

std::string json = R"({
    "name": "MyApp",
    "config": {
        "$include": "config/database.json"
    },
    "features": {
        "$include": ["features/auth.json", "features/logging.json"]
    }
})";

JsonNodePtr root = parser.parse(json);

// Access parsed data
JsonNodePtr config = root->getChild("config");
JsonNodePtr host = config->getChild("host");
if (host && host->type == JsonType::STRING) {
    std::cout << "Host: " << host->stringValue << std::endl;
}
```

### Memory Manager

```cpp
#include "memory_manager.hpp"

// Use global memory manager
auto& manager = MemoryManager::getGlobalManager();

// Allocate and construct objects
MyClass* obj = manager.construct<MyClass>(arg1, arg2);

// Use the object
obj->doSomething();

// Destroy and deallocate
manager.destroy(obj);

// Use with STL containers
managed_vector<int> vec(&manager);
vec.push_back(42);
```

### Task Scheduler

```cpp
#include "scheduler.hpp"

Scheduler scheduler;

// Fire-and-forget task
scheduler.schedule([]() {
    std::cout << "Task executed!" << std::endl;
});

// Task with result
auto future = scheduler.prepare([]() -> int {
    return 42;
});

int result = future.get();
```

### Object Registry

```cpp
#include "object.hpp"

auto& registry = ObjectRegistry::getInstance();

// Register objects
auto renderer = std::make_shared<Renderer>();
registry.registerObject("main_renderer", renderer);

// Retrieve objects
auto obj = registry.getObject("main_renderer");
if (obj) {
    obj->display();
}
```

### JSON Initialization Code Generator

#### Step 1: Annotate your classes

```cpp
#include "object.hpp"

class __attribute__((annotate("initialize"))) GameEntity {
private:
    std::string name;
    int health;
    double speed;
    bool isActive;
    std::shared_ptr<Renderer> renderer;

public:
    GameEntity(const std::string& name,
               int health,
               double speed,
               bool isActive,
               std::shared_ptr<Renderer> renderer = nullptr)
        : name(name), health(health), speed(speed),
          isActive(isActive), renderer(renderer) {}

    // Getters...
};
```

#### Step 2: Generate initialization code

```bash
./build/json_init_generator example/json_init_example.hpp --output-dir generated
```

This generates:
- `generated/gameentity_initializer.hpp` - Header with initialization function
- `generated/gameentity_initializer.cpp` - Implementation
- `generated/gameentity_.conf` - Example JSON configuration

#### Step 3: Use the generated code

```cpp
#include "generated/gameentity_initializer.hpp"
#include "json_node.hpp"

JsonParser parser;
JsonNodePtr config = parser.parse(jsonString);

GameEntity entity;
initializeFromJson(entity, config);
```

Example JSON configuration:

```json
{
    "name": "Player",
    "health": 100,
    "speed": 5.5,
    "isActive": true,
    "renderer": "main_renderer"
}
```

Object-derived pointers (like `renderer`) are automatically resolved from the ObjectRegistry.

## Project Structure

```
.
├── include/              # Public headers
│   ├── json_node.hpp
│   ├── memory_manager.hpp
│   ├── object.hpp
│   ├── scheduler.hpp
│   └── json_init_generator.hpp
├── src/                  # Implementation files
│   ├── json_node.cpp
│   ├── memory_manager.cpp
│   ├── object.cpp
│   ├── scheduler.cpp
│   ├── json_init_generator.cpp
│   └── json_init_generator_main.cpp
├── example/              # Example usage
│   ├── json_node_example.cpp
│   ├── memory_manager_example.cpp
│   ├── scheduler_example.cpp
│   ├── json_init_example.hpp
│   └── test_generated.cpp
├── CMakeLists.txt
├── build_and_test.sh
└── README.md
```

## Targets

The build system creates the following targets:

- `sdk` - Static library containing core functionality
- `json_init_generator` - Code generation tool
- `json_example` - JSON parser example
- `test_generated` - Test for generated initialization code

## Installation

```bash
make install
```

This installs:
- Library files to `lib/`
- Headers to `include/`
- Tools to `bin/`

## License

See repository for license information.
