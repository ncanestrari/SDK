#include "memory_manager.hpp"
#include <fmt/core.h>
#include <vector>
#include <chrono>
#include <random>
#include <map>
#include <string>

// Test classes of different sizes
class SmallObject {
    int value;
public:
    SmallObject(int v = 0) : value(v) {}
    int getValue() const { return value; }
};

class MediumObject {
    char data[64];
    int id;
public:
    MediumObject(int i = 0) : id(i) {
        std::fill_n(data, sizeof(data), static_cast<char>('A' + (i % 26)));
    }
    int getId() const { return id; }
};

class LargeObject {
    char buffer[512];
    double values[32];
    std::string name;
public:
    LargeObject(const std::string& n = "default") : name(n) {
        std::fill_n(buffer, sizeof(buffer), 'X');
        for (int i = 0; i < 32; ++i) {
            values[i] = i * 3.14159;
        }
    }
    const std::string& getName() const { return name; }
};

void testPoolInitialization() {
    fmt::print("\n=== Pool Initialization Test ===\n");
    
    MemoryManager& mm = MemoryManager::getGlobalManager();
    
    fmt::print("Testing lazy pool initialization...\n");
    
    // These allocations should create pools on-demand
    SmallObject* small = mm.construct<SmallObject>(42);
    MediumObject* medium = mm.construct<MediumObject>(100);
    LargeObject* large = mm.construct<LargeObject>("test");
    
    fmt::print("Objects created:\n");
    fmt::print("  Small ({}B): value = {}\n", sizeof(SmallObject), small->getValue());
    fmt::print("  Medium ({}B): id = {}\n", sizeof(MediumObject), medium->getId());
    fmt::print("  Large ({}B): name = '{}'\n", sizeof(LargeObject), large->getName().c_str());
    
    mm.printDetailedStatus();
    
    // Clean up
    mm.destroy(small);
    mm.destroy(medium);
    mm.destroy(large);
}

void testPowerOf2Alignment() {
    fmt::print("\n=== Power of 2 Alignment Test ===\n");
    
    MemoryManager mm;
    
    // Test various sizes to see power-of-2 rounding
    std::vector<size_t> testSizes = {1, 3, 7, 15, 31, 63, 127, 255, 511, 1023};
    
    for (size_t size : testSizes) {
        size_t category = MemoryManager::getSizeCategory(size);
        fmt::print("Size {} -> Pool category {}B\n", size, category);
        
        // Allocate to trigger pool creation
        void* ptr = mm.allocate(size);
        if (ptr) {
            mm.deallocate(ptr);
        }
    }
    
    fmt::print("\nPool status after alignment test:\n");
    mm.printDetailedStatus();
}

void testPoolExhaustion() {
    fmt::print("\n=== Pool Exhaustion Test ===\n");
    
    MemoryManager mm;
    std::vector<SmallObject*> objects;
    
    fmt::print("Allocating SmallObjects until pool exhaustion...\n");
    fmt::print("Default strategy: 256 objects per pool\n");
    
    // Allocate more than the pool size (256 objects)
    for (int i = 0; i < 300; ++i) {
        SmallObject* obj = mm.construct<SmallObject>(i);
        if (obj) {
            objects.push_back(obj);
        }
    }
    
    fmt::print("Successfully allocated {} SmallObjects\n", objects.size());
    mm.printStatistics();
    
    // Check fallback allocations
    if (mm.getFallbackAllocations() > 0) {
        fmt::print("Pool exhausted, {} allocations fell back to system allocator\n", 
                   mm.getFallbackAllocations());
    } else {
        fmt::print("All allocations served from pools\n");
    }
    
    // Clean up
    for (auto obj : objects) {
        mm.destroy(obj);
    }
    
    fmt::print("After cleanup:\n");
    mm.printStatistics();
}

void testMixedSizeAllocations() {
    fmt::print("\n=== Mixed Size Allocations Test ===\n");
    
    MemoryManager mm;
    std::vector<void*> allocations;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sizeDist(1, 1024);
    
    fmt::print("Allocating 1000 objects of random sizes (1-1024 bytes)...\n");
    
    for (int i = 0; i < 1000; ++i) {
        size_t size = sizeDist(gen);
        void* ptr = mm.allocate(size);
        if (ptr) {
            allocations.push_back(ptr);
        }
    }
    
    fmt::print("Allocated {} objects\n", allocations.size());
    mm.printDetailedStatus();
    
    // Deallocate half randomly
    std::shuffle(allocations.begin(), allocations.end(), gen);
    size_t halfSize = allocations.size() / 2;
    
    for (size_t i = 0; i < halfSize; ++i) {
        mm.deallocate(allocations[i]);
        allocations[i] = nullptr;
    }
    
    fmt::print("\nAfter deallocating half the objects:\n");
    mm.printStatistics();
    
    // Clean up remaining
    for (void* ptr : allocations) {
        if (ptr) {
            mm.deallocate(ptr);
        }
    }
}

void testCustomStrategy() {
    fmt::print("\n=== Custom Strategy Test ===\n");
    
    // Custom strategy: smaller pools (64 objects instead of 256)
    class SmallPoolStrategy : public PoolStrategy {
    public:
        size_t calculatePoolSize(size_t objectSize) const override {
            return 64 * objectSize;
        }
        
        size_t calculateObjectCount(size_t objectSize) const override {
            return 64;
        }
    };
    
    MemoryManager mm(std::make_unique<SmallPoolStrategy>());
    
    fmt::print("Using custom strategy: 64 objects per pool\n");
    
    std::vector<MediumObject*> objects;
    
    // Allocate enough to potentially exhaust the smaller pool
    for (int i = 0; i < 80; ++i) {
        MediumObject* obj = mm.construct<MediumObject>(i);
        if (obj) {
            objects.push_back(obj);
        }
    }
    
    fmt::print("Allocated {} MediumObjects with custom strategy\n", objects.size());
    mm.printDetailedStatus();
    
    // Clean up
    for (auto obj : objects) {
        mm.destroy(obj);
    }
}

void testPerformanceComparison() {
    fmt::print("\n=== Performance Comparison ===\n");
    
    const int iterations = 50000;
    std::vector<void*> ptrs;
    ptrs.reserve(iterations);
    
    // Test system malloc/free
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        void* ptr = malloc(64); // Fixed size for fair comparison
        ptrs.push_back(ptr);
    }
    
    for (void* ptr : ptrs) {
        free(ptr);
    }
    
    auto systemTime = std::chrono::high_resolution_clock::now() - start;
    ptrs.clear();
    
    // Test MemoryManager pools
    MemoryManager mm;
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        void* ptr = mm.allocate(64);
        ptrs.push_back(ptr);
    }
    
    for (void* ptr : ptrs) {
        mm.deallocate(ptr);
    }
    
    auto poolTime = std::chrono::high_resolution_clock::now() - start;
    
    fmt::print("Performance test ({} allocations of 64 bytes):\n", iterations);
    fmt::print("System malloc/free: {:.2f}ms\n", 
               std::chrono::duration<double, std::milli>(systemTime).count());
    fmt::print("Memory pools:       {:.2f}ms\n", 
               std::chrono::duration<double, std::milli>(poolTime).count());
    
    if (systemTime > poolTime) {
        fmt::print("Speedup: {:.2f}x faster\n", 
                   static_cast<double>(systemTime.count()) / poolTime.count());
    } else {
        fmt::print("Slowdown: {:.2f}x slower\n", 
                   static_cast<double>(poolTime.count()) / systemTime.count());
    }
    
    mm.printStatistics();
}

void testSTLContainersWithPools() {
    fmt::print("\n=== STL Containers with Pool Allocator ===\n");
    
    MemoryManager mm;
    
    // Test vector with pool allocator
    {
        managed_vector<int> vec(MemoryManagerAllocator<int>(&mm));
        
        for (int i = 0; i < 1000; ++i) {
            vec.push_back(i * i);
        }
        
        fmt::print("Created vector with {} elements\n", vec.size());
        fmt::print("Sample values: vec[100]={}, vec[500]={}\n", vec[100], vec[500]);
    }
    
    // Test map with pool allocator
    {
        managed_map<int, managed_string> map(
            std::less<int>(), 
            MemoryManagerAllocator<std::pair<const int, managed_string>>(&mm)
        );
        
        for (int i = 0; i < 500; ++i) {
            managed_string str(MemoryManagerAllocator<char>(&mm));
            str = fmt::format("Value_{}", i);
            map[i] = std::move(str);
        }
        
        fmt::print("Created map with {} elements\n", map.size());
        fmt::print("Sample entries: map[100]='{}', map[250]='{}'\n", 
                   map[100].c_str(), map[250].c_str());
    }
    
    fmt::print("STL containers destroyed, pool status:\n");
    mm.printDetailedStatus();
}

void testAutomaticNewOperator() {
    fmt::print("\n=== Automatic New Operator Test ===\n");
    
    MemoryManager customMM;
    
    // Test placement new with specific memory manager
    fmt::print("Using placement new with custom memory manager...\n");
    
    SmallObject* small = new(customMM) SmallObject(999);
    MediumObject* medium = new(customMM) MediumObject(888);
    LargeObject* large = new(customMM) LargeObject("custom_manager");
    
    fmt::print("Objects created via placement new:\n");
    fmt::print("  SmallObject: value = {}\n", small->getValue());
    fmt::print("  MediumObject: id = {}\n", medium->getId());
    fmt::print("  LargeObject: name = '{}'\n", large->getName().c_str());
    
    customMM.printDetailedStatus();
    
    // Manual cleanup for placement new
    small->~SmallObject();
    medium->~MediumObject();
    large->~LargeObject();
    customMM.deallocate(small);
    customMM.deallocate(medium);
    customMM.deallocate(large);
    
    fmt::print("After manual cleanup:\n");
    customMM.printStatistics();
}

void testLargeObjectFallback() {
    fmt::print("\n=== Large Object Fallback Test ===\n");
    
    MemoryManager mm;
    
    fmt::print("Max supported pool size: {} bytes\n", MemoryManager::getMaxSupportedSize());
    
    // Allocate objects larger than max pool size
    size_t largeSize = MemoryManager::getMaxSupportedSize() * 2; // 2MB
    
    fmt::print("Attempting to allocate {}MB object...\n", largeSize / (1024 * 1024));
    
    void* largePtr = mm.allocate(largeSize);
    if (largePtr) {
        fmt::print("Large allocation successful (should use system allocator)\n");
        mm.deallocate(largePtr);
    } else {
        fmt::print("Large allocation failed\n");
    }
    
    fmt::print("Fallback allocations: {}\n", mm.getFallbackAllocations());
    mm.printStatistics();
}

void demonstrateTransparentUsage() {
    fmt::print("\n=== Transparent Usage Demonstration ===\n");
    
    // The beauty of this design: client code doesn't need to know about pools!
    MemoryManager& mm = MemoryManager::getGlobalManager();
    
    fmt::print("Client code allocating various objects transparently...\n");
    
    // Client just allocates normally
    auto small1 = mm.construct<SmallObject>(1);
    auto small2 = mm.construct<SmallObject>(2);
    auto small3 = mm.construct<SmallObject>(3);
    
    auto medium1 = mm.construct<MediumObject>(10);
    auto medium2 = mm.construct<MediumObject>(20);
    
    auto large1 = mm.construct<LargeObject>("obj1");
    
    fmt::print("Objects allocated - pools created automatically\n");
    mm.printDetailedStatus();
    
    // More allocations go to existing pools
    auto small4 = mm.construct<SmallObject>(4);
    auto medium3 = mm.construct<MediumObject>(30);
    
    fmt::print("\nAfter additional allocations (reusing pools):\n");
    mm.printStatistics();
    
    // Cleanup
    mm.destroy(small1);
    mm.destroy(small2);
    mm.destroy(small3);
    mm.destroy(small4);
    mm.destroy(medium1);
    mm.destroy(medium2);
    mm.destroy(medium3);
    mm.destroy(large1);
    
    fmt::print("\nAfter cleanup:\n");
    mm.printStatistics();
}

int main() {
    fmt::print("=== Advanced Pool-Based Memory Manager ===\n");
    fmt::print("Features:\n");
    fmt::print("- Size-based pools with power-of-2 alignment\n");
    fmt::print("- Lazy pool initialization\n");
    fmt::print("- Default strategy: 256 objects per pool\n");
    fmt::print("- Automatic fallback for large objects\n");
    fmt::print("- Transparent usage - no client awareness needed\n\n");
    
    // Test pool initialization
    testPoolInitialization();
    
    // Test power-of-2 alignment
    testPowerOf2Alignment();
    
    // Test pool exhaustion and fallback
    testPoolExhaustion();
    
    // Test mixed size allocations
    testMixedSizeAllocations();
    
    // Test custom allocation strategy
    testCustomStrategy();
    
    // Test performance vs system allocator
    testPerformanceComparison();
    
    // Test STL containers with pool allocators
    testSTLContainersWithPools();
    
    // Test automatic new operator
    testAutomaticNewOperator();
    
    // Test large object fallback
    testLargeObjectFallback();
    
    // Demonstrate transparent usage
    demonstrateTransparentUsage();
    
    fmt::print("\n=== Summary ===\n");
    fmt::print("The pool-based memory manager provides:\n");
    fmt::print("1. Fast allocation/deallocation for common sizes\n");
    fmt::print("2. Reduced fragmentation through size-based pools\n");
    fmt::print("3. Lazy initialization - only create pools when needed\n");
    fmt::print("4. Transparent operation - client code unchanged\n");
    fmt::print("5. Configurable strategies for different use cases\n");
    fmt::print("6. Automatic fallback for oversized objects\n");
    
    return 0;
}
