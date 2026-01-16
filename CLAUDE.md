# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A C++17/20 library for game development and application infrastructure with JSON parsing, memory management, task scheduling, logging, clock control, socket abstraction, and automatic code generation.

## Build Commands

### Standard Build
```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Quick Build and Test
```bash
./build_and_test.sh
```

### Build Targets
- `sdk` - Main static library
- `json_init_generator` - Clang-based code generator tool
- `json_example` - JSON parser demo
- `scheduler_example` - Task scheduler demo
- `scheduler_example_graph` - Graph-based scheduler demo
- `logger_example` - Logger system demo
- `clock_example` - Clock system demo
- `socket_example` - Socket system demo
- `test_generated` - Tests for generated initialization code

### Code Generation
Code generation happens automatically during build via custom CMake commands, but you can trigger manually:

```bash
# Generate initialization code for annotated classes
./build/json_init_generator example/json_init_example.hpp --output-dir generated

# With compiler flags
./build/json_init_generator include/socket.hpp \
    --output-dir generated/socket \
    -p build \
    -- -std=c++20 \
    -I./include \
    -resource-dir /usr/local/lib/clang/21
```

### Install
```bash
cd build
make install
```

## Core Architecture Patterns

### 1. Object Registry Pattern
The `ObjectRegistry` is a singleton that manages globally accessible objects by string name. All major system components can inherit from `Object` to be registry-compatible.

**Key insight:** When adding new major components (like renderers, managers, systems), inherit from `Object` and register them for cross-system access without tight coupling.

**Implementation:**
- `Object` base class provides virtual `getType()` and `display()`
- `ObjectRegistry::getInstance()` provides global access
- Objects stored as `shared_ptr<Object>` with string keys
- Used extensively by JSON initialization system for pointer resolution

### 2. Memory Pool Architecture
The `MemoryManager` uses power-of-2 size categories (1, 2, 4, 8... up to 1MB) with separate pools per size. This avoids fragmentation and provides O(1) allocations.

**Key insight:** Objects are rounded up to nearest power-of-2, so a 65-byte object uses 128-byte pool. Consider size implications when designing data structures.

**Thread-safety:** All pool operations are mutex-protected. Safe for multi-threaded allocation.

### 3. Single-Threaded Task Scheduler
The `Scheduler` uses ONE worker thread with a task queue. It's designed for async operations, not parallelism.

**Key insight:** Don't expect parallel execution - tasks run sequentially in FIFO order. For parallel work, create multiple Scheduler instances.

**Fire-and-forget:**
```cpp
scheduler.schedule([]() { /* async work */ });
```

**Future-based:**
```cpp
auto future = scheduler.prepare([]() { return 42; });
int result = future.get();
```

### 4. JSON Include System
The JSON parser supports recursive file inclusion with `$include` directive and automatic merging. Included files are cached to avoid re-parsing.

**Key insight:** When an object contains `$include`, it's replaced with the included content. For arrays of includes, all files are merged. Path resolution is relative to the including file's directory.

**Pattern:**
```json
{
  "config": {
    "$include": "database.json"
  },
  "features": {
    "$include": ["auth.json", "logging.json"]
  }
}
```

### 5. Code Generation System
The `json_init_generator` is a Clang LibTooling-based tool that finds classes with `__attribute__((annotate("initialize")))` and generates initialization code.

**Key insights:**
- Analyzes constructor parameters to determine initialization order
- Generates three files per class: `<class>_initializer.hpp`, `<class>_initializer.cpp`, `<class>_.conf`
- Object-derived pointer members are resolved via `ObjectRegistry::getInstance().getObject()`
- Non-pointer primitives (int, double, string, bool) initialized directly from JSON
- Generated code is type-aware and matches constructor signatures

**CMake integration:**
- Code generation is a build dependency for the `sdk` library
- Socket and scheduler initializers are auto-generated and linked into `sdk`
- Custom targets: `generate_code`, `generate_socket_code`, `generate_scheduler_code`

### 6. Global Clock System
The `GlobalClock` singleton allows runtime switching between `RealTimeClock` and `SimulatorClock`. All timing code should use `GlobalClock::now()` instead of `std::chrono::steady_clock::now()`.

**Key insights:**
- `SimulatorClock` has three modes: REALTIME (with time scale), MANUAL (deterministic), PAUSED
- Time scale can be 0.1x to 100x+ in REALTIME mode
- MANUAL mode allows advancing time programmatically for testing
- `ScopedClockOverride` temporarily changes clock behavior and auto-restores

**Testing pattern:**
```cpp
auto* sim = GlobalClock::getSimulator();
sim->setMode(SimulatorClock::Mode::MANUAL);
sim->reset();
sim->advance(std::chrono::seconds(10)); // Jump forward
```

### 7. Socket Abstraction Hierarchy
The socket system provides a uniform `ISocket` interface with specialized implementations:

**Unix Domain:**
- `UnixStreamSocket` (SOCK_STREAM) - connection-oriented
- `UnixDatagramSocket` (SOCK_DGRAM) - connectionless
- `UnixSeqPacketSocket` (SOCK_SEQPACKET) - connection-oriented with message boundaries

**Network:**
- `TcpSocket` - IPv4/IPv6 TCP streams
- `UdpSocket` - IPv4/IPv6 UDP datagrams
- `RawSocket` - Raw IP packets

**Server variants:**
- `UnixStreamServerSocket`, `TcpServerSocket`, `UnixSeqPacketServerSocket`
- Pattern: `bind()` → `listen()` → `accept()` returns client socket

**Key insights:**
- All sockets are move-only (no copy)
- Address types: `UnixAddress` (file paths), `InetAddress` (host:port)
- SeqPacket preserves message boundaries unlike stream sockets
- Socket options (timeout, non-blocking, broadcast) are chainable

### 8. Logger Buffering System
The `Logger` uses asynchronous logging via the `Scheduler` with configurable flush triggers (byte limit OR time interval).

**Key insights:**
- Endpoints (stdout, file, socket, chained loggers) receive formatted messages
- Format string uses placeholders: date, module, level, message
- Template-based formatting via fmt library: `logger->info("User {}", username)`
- Flush occurs when buffer exceeds byte limit OR time interval expires
- Thread-safe via scheduler's queue

## Important File Locations

### Headers (include/)
- `object.hpp` - Object base class and ObjectRegistry singleton
- `memory_manager.hpp` - Pool allocator with power-of-2 strategy
- `scheduler.hpp` - Single-threaded async task scheduler
- `json_node.hpp` - JSON parser with include support
- `logger.hpp` - Async logger with multiple endpoints
- `clock.hpp` - Controllable timing system
- `socket.hpp` - Socket abstraction layer
- `json_init_generator.hpp` - Code generator declarations
- `task.hpp` - Task wrapper for scheduler

### Source (src/)
All `.cpp` implementation files matching headers above.

### Examples (example/)
Each system has a corresponding `*_example.cpp` demonstrating usage patterns.

## Dependencies

### Required
- CMake 3.15+
- C++20 compiler (uses C++17 features but standard set to C++20)
- [fmt](https://github.com/fmtlib/fmt) library
- LLVM/Clang development libraries (for code generator)

### Finding Clang Resources
The code generator requires the Clang resource directory. Current path is `/usr/local/lib/clang/21` (see CMakeLists.txt:259, 281, 295). If Clang version changes, update `-resource-dir` flag.

## Code Style Conventions

### Annotations
Use `__attribute__((annotate("initialize")))` on classes that need JSON initialization:
```cpp
class __attribute__((annotate("initialize"))) MyClass : public Object {
    // ...
};
```

### Thread Safety
- `MemoryManager`: All operations mutex-protected
- `Scheduler`: Queue operations mutex-protected, tasks run on single thread
- `ObjectRegistry`: Not thread-safe (single-threaded design)
- `Logger`: Thread-safe via scheduler
- `Clock`: Thread-safe via atomic operations

### Inheritance Pattern
When creating new system components, inherit from `Object` to enable:
1. Runtime type identification via `getType()`
2. Global registration via `ObjectRegistry`
3. JSON initialization via code generator
4. Pointer resolution in generated code

## Testing and Debugging

### Run Examples
```bash
cd build
./json_example
./scheduler_example
./logger_example
./clock_example
./socket_example
./test_generated
```

### Common Issues
1. **Code generator fails:** Check Clang resource directory path matches installed version
2. **Missing generated files:** Run `make generate_code` or full rebuild
3. **Link errors with Clang:** Ensure LLVM/Clang development packages installed
4. **Scheduler tasks not executing:** Scheduler destructor waits for completion - ensure proper shutdown

## Key Design Decisions

1. **Single-threaded scheduler:** Simplicity over parallelism. Create multiple instances for parallel work.
2. **Power-of-2 pools:** Reduces fragmentation but may waste space for odd-sized objects.
3. **Global singletons:** ObjectRegistry and GlobalClock are singletons for convenience - limits to one instance per process.
4. **Move-only sockets:** Prevents accidental socket duplication and double-close errors.
5. **Async logging:** Prevents I/O from blocking main thread, but messages may be delayed.
6. **Cached includes:** JSON files are cached on first parse - changes require cache clear or restart.
