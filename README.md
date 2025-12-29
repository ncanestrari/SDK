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

### 5. Logger System
A flexible asynchronous logging system with multiple endpoints and configurable formatting.

- **Asynchronous logging** using the task scheduler
- **Multiple endpoints** (stdout, file, socket, logger chaining)
- **Configurable format** with placeholders for date, module, level, and message
- **Log levels** (DEBUG, INFO, LOG, WARN, ERROR) with runtime filtering
- **Buffered output** with configurable flush triggers (byte limit and time interval)
- Thread-safe operations

### 6. Clock System
A controllable timing system that can replace `std::steady_clock` with support for time scaling and simulation.

- **RealTimeClock** - Standard real-time clock
- **SimulatorClock** - Controllable clock with three modes:
  - **REALTIME** - Follow real time with configurable speed (0.1x to 100x+)
  - **MANUAL** - Deterministic time control for testing
  - **PAUSED** - Freeze time for debugging
- **GlobalClock** - Singleton interface for application-wide time control
- **ScopedClockOverride** - Temporarily change clock behavior
- Thread-safe operations
- Drop-in replacement for std::chrono clocks

### 7. Socket System
A comprehensive socket abstraction supporting Unix domain and network sockets with multiple communication patterns.

- **ISocket** - Base interface with send/receive operations
- **Unix Domain Sockets**:
  - **UnixStreamSocket** - Connection-oriented stream communication (SOCK_STREAM)
  - **UnixDatagramSocket** - Connectionless datagram communication (SOCK_DGRAM)
  - **UnixSeqPacketSocket** - Connection-oriented with message boundaries (SOCK_SEQPACKET)
- **Network Sockets**:
  - **TcpSocket** - TCP stream sockets with IPv4/IPv6 support
  - **UdpSocket** - UDP datagram sockets with IPv4/IPv6 support
  - **RawSocket** - Raw IP sockets for custom protocols
- **Server Sockets** - Bind, listen, and accept for stream-oriented protocols
- **Socket Options** - Timeout, non-blocking mode, broadcast, address reuse
- Type-safe address handling (UnixAddress, InetAddress)
- Move-only semantics for resource safety

### 8. JSON Initialization Code Generator
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

### Logger

```cpp
#include "logger.hpp"

// Create logger with module name
auto logger = std::make_shared<Logger>("MyApp");

// Add endpoints
logger->addEndpoint(std::make_shared<StdoutEndpoint>());
logger->addEndpoint(std::make_shared<FileEndpoint>("app.log"));

// Configure
logger->setFormat("{} - {} - [{}] {}\n");
logger->setFlushByteLimit(1024);  // Flush every 1KB
logger->setFlushTimeInterval(std::chrono::seconds(5));  // Or every 5s
logger->setLevel(static_cast<int>(Logger::LogLevel::INFO));

// Log messages
logger->info("Application started");
logger->warn("Low memory");
logger->error("Failed to load config");

// Template formatting
logger->info("User {} logged in", username);
logger->warn("Retry attempt {}/{}", current, max);
```

### Clock System

```cpp
#include "clock.hpp"

// Use real-time clock (default)
GlobalClock::useRealTime();
auto start = GlobalClock::now();
doWork();
auto elapsed = GlobalClock::now() - start;

// Use simulator clock with 2x speed
GlobalClock::useSimulator(2.0);
std::this_thread::sleep_for(std::chrono::milliseconds(100));
// Time advances by 200ms

// Manual time control for testing
auto* sim = GlobalClock::getSimulator();
sim->setMode(SimulatorClock::Mode::MANUAL);
sim->reset();
sim->advance(std::chrono::seconds(10));  // Jump 10 seconds forward

// Pause/resume time
sim->pause();
// ... time is frozen
sim->resume();

// Change time scale dynamically
sim->setTimeScale(0.1);   // Slow motion
sim->setTimeScale(100.0); // Fast forward

// Temporary clock override
{
    ScopedClockOverride override(std::make_unique<SimulatorClock>(10.0));
    // All code here runs with 10x speed
} // Automatically restored
```

### Socket System

#### Unix Domain Stream Socket (client/server)

```cpp
#include "socket.hpp"

// Server
UnixStreamServerSocket server(UnixAddress{"/tmp/my_socket.sock"});
server.bind();
server.listen();

auto client = server.accept();
uint8_t buffer[256];
auto received = client->receive(std::span<uint8_t>(buffer, sizeof(buffer)));

// Client
UnixStreamSocket client(UnixAddress{"/tmp/my_socket.sock"});
client.connect();
std::string msg = "Hello";
client.send(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));
```

#### TCP Socket (client/server)

```cpp
#include "socket.hpp"

// Server
TcpServerSocket server(InetAddress{"0.0.0.0", 8080});
server.bind();
server.listen();

auto client = server.accept();
uint8_t buffer[1024];
auto received = client->receive(std::span<uint8_t>(buffer, sizeof(buffer)));

// Client
TcpSocket client(InetAddress{"127.0.0.1", 8080});
client.connect();
std::string msg = "GET / HTTP/1.1\r\n\r\n";
client.send(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));
```

#### UDP Socket (datagram)

```cpp
#include "socket.hpp"

// Sender
UdpSocket sender;
std::string msg = "UDP message";
sender.sendTo(
    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()),
    InetAddress{"127.0.0.1", 9090}
);

// Receiver
UdpSocket receiver(InetAddress{"0.0.0.0", 9090});
receiver.bind();

uint8_t buffer[1024];
InetAddress senderAddr{"", 0};
auto received = receiver.receiveFrom(std::span<uint8_t>(buffer, sizeof(buffer)), senderAddr);
```

#### Unix Datagram Socket

```cpp
#include "socket.hpp"

// Sender
UnixDatagramSocket sender(UnixAddress{"/tmp/client.sock"});
sender.bind();
std::string msg = "Datagram";
sender.sendTo(
    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()),
    UnixAddress{"/tmp/server.sock"}
);

// Receiver
UnixDatagramSocket receiver(UnixAddress{"/tmp/server.sock"});
receiver.bind();

uint8_t buffer[256];
UnixAddress senderAddr{""};
auto received = receiver.receiveFrom(std::span<uint8_t>(buffer, sizeof(buffer)), senderAddr);
```

#### Sequenced Packet Socket (message boundaries preserved)

```cpp
#include "socket.hpp"

// Server
UnixSeqPacketServerSocket server(UnixAddress{"/tmp/seqpacket.sock"});
server.bind();
server.listen();

auto client = server.accept();
uint8_t buffer[256];
// Each receive gets exactly one complete message
auto msg1 = client->receive(std::span<uint8_t>(buffer, sizeof(buffer)));
auto msg2 = client->receive(std::span<uint8_t>(buffer, sizeof(buffer)));

// Client
UnixSeqPacketSocket client(UnixAddress{"/tmp/seqpacket.sock"});
client.connect();
// Send two separate messages - boundaries preserved
client.sendMessage(std::span<const uint8_t>(...));  // Message 1
client.sendMessage(std::span<const uint8_t>(...));  // Message 2
```

#### Socket Options

```cpp
// Non-blocking mode
socket.setNonBlocking(true);

// Timeout
socket.setTimeout(std::chrono::milliseconds(5000));

// UDP broadcast
udpSocket.setBroadcast(true);

// TCP address reuse
tcpServer.setReuseAddr(true);

// Raw socket with IP header
rawSocket.setIpHeaderInclude(true);
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
│   ├── logger.hpp
│   ├── clock.hpp
│   ├── socket.hpp
│   └── json_init_generator.hpp
├── src/                  # Implementation files
│   ├── json_node.cpp
│   ├── memory_manager.cpp
│   ├── object.cpp
│   ├── scheduler.cpp
│   ├── logger.cpp
│   ├── clock.cpp
│   ├── socket.cpp
│   ├── json_init_generator.cpp
│   └── json_init_generator_main.cpp
├── example/              # Example usage
│   ├── json_node_example.cpp
│   ├── scheduler_example.cpp
│   ├── logger_example.cpp
│   ├── clock_example.cpp
│   ├── socket_example.cpp
│   ├── json_init_example.hpp
│   └── test_generated.cpp
├── CMakeLists.txt
└── README.md
```

## Targets

The build system creates the following targets:

- `sdk` - Static library containing core functionality
- `json_init_generator` - Code generation tool
- `json_example` - JSON parser example
- `scheduler_example` - Task scheduler example
- `logger_example` - Logger system example
- `clock_example` - Clock system example
- `socket_example` - Socket system example
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
