
#include "memory_manager.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

// Helper functions
static size_t nextPowerOf2(size_t n) {
    if (n <= 1) return 1;
    //return 1ULL << (64 - std::countl_zero(n - 1));
    return 1ULL << (64 - __builtin_clz(n - 1));
}

// MemoryPool implementation
MemoryPool::MemoryPool(size_t objSize, size_t objCount) 
    : objectSize(objSize), objectCount(objCount), poolSize(objSize * objCount) {
    
    memory = std::make_unique<uint8_t[]>(poolSize);
    freeList.reserve(objectCount);
    initializePool();
}

void MemoryPool::initializePool() {
    // Initialize free list with all objects
    uint8_t* current = memory.get();
    for (size_t i = 0; i < objectCount; ++i) {
        freeList.push_back(current);
        current += objectSize;
    }
}

void* MemoryPool::allocate() {
    std::lock_guard<std::mutex> lock(poolMutex);
    
    if (freeList.empty()) {
        return nullptr; // Pool exhausted
    }
    
    void* ptr = freeList.back();
    freeList.pop_back();
    
    allocatedObjects.fetch_add(1);
    totalAllocations.fetch_add(1);
    
    return ptr;
}

bool MemoryPool::deallocate(void* ptr) {
    if (!ptr || !containsPointer(ptr)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(poolMutex);
    
    // Check if pointer is aligned to object boundary
    uintptr_t offset = static_cast<uint8_t*>(ptr) - memory.get();
    if (offset % objectSize != 0) {
        return false; // Invalid pointer alignment
    }
    
    freeList.push_back(ptr);
    allocatedObjects.fetch_sub(1);
    totalDeallocations.fetch_add(1);
    
    return true;
}

bool MemoryPool::containsPointer(void* ptr) const {
    uint8_t* memStart = memory.get();
    uint8_t* memEnd = memStart + poolSize;
    return ptr >= memStart && ptr < memEnd;
}

size_t MemoryPool::getAvailableObjects() const {
    std::lock_guard<std::mutex> lock(poolMutex);
    return freeList.size();
}

void MemoryPool::printStatus() const {
    fmt::print("  Pool ({}B objects): {}/{} allocated, {} total allocs, {} total deallocs\n",
               objectSize, getAllocatedObjects(), objectCount, 
               getTotalAllocations(), getTotalDeallocations());
}

// MemoryManager implementation
MemoryManager::MemoryManager() 
    : strategy(std::make_unique<DefaultPoolStrategy>()) {
}

MemoryManager::MemoryManager(std::unique_ptr<PoolStrategy> poolStrategy) 
    : strategy(std::move(poolStrategy)) {
}

size_t MemoryManager::roundToPowerOf2(size_t size) {
    if (size == 0) return 1;
    if (size <= 1) return 1;
    return nextPowerOf2(size);
}

size_t MemoryManager::getPoolIndex(size_t size) {
    if (size == 0) return 0;
    size_t powerOf2Size = roundToPowerOf2(size);
    
    // Find the bit position (log2)
    size_t index = 0;
    size_t temp = powerOf2Size;
    while (temp > 1) {
        temp >>= 1;
        index++;
    }
    
    return std::min(index, MAX_POOLS - 1);
}

MemoryPool* MemoryManager::getOrCreatePool(size_t size) {
    size_t index = getPoolIndex(size);
    
    std::call_once(poolInitFlags[index], [this, index, size]() {
        size_t powerOf2Size = roundToPowerOf2(size);
        size_t objectCount = strategy->calculateObjectCount(powerOf2Size);
        
        pools[index] = std::make_unique<MemoryPool>(powerOf2Size, objectCount);
        
        fmt::print("Initialized pool[{}]: {}B objects, {} objects per pool\n", 
                   index, powerOf2Size, objectCount);
    });
    
    return pools[index].get();
}

MemoryPool* MemoryManager::findPoolForPointer(void* ptr) {
    for (auto& pool : pools) {
        if (pool && pool->containsPointer(ptr)) {
            return pool.get();
        }
    }
    return nullptr;
}

void* MemoryManager::allocate(size_t size, size_t alignment) {
    if (size == 0) return nullptr;
    
    // Check if size exceeds maximum supported size
    if (size > getMaxSupportedSize()) {
        // Fallback to system allocator for very large objects
        fallbackAllocations.fetch_add(1);
        totalAllocations.fetch_add(1);
        return std::aligned_alloc(alignment, size);
    }
    
    // Get or create appropriate pool
    MemoryPool* pool = getOrCreatePool(size);
    if (!pool) {
        return nullptr;
    }
    
    void* ptr = pool->allocate();
    if (ptr) {
        totalAllocations.fetch_add(1);
        return ptr;
    }
    
    // Pool exhausted, fallback to system allocator
    fallbackAllocations.fetch_add(1);
    totalAllocations.fetch_add(1);
    return std::aligned_alloc(alignment, size);
}

bool MemoryManager::deallocate(void* ptr) {
    if (!ptr) return false;
    
    // Try to find the pool that owns this pointer
    MemoryPool* pool = findPoolForPointer(ptr);
    if (pool) {
        bool success = pool->deallocate(ptr);
        if (success) {
            totalDeallocations.fetch_add(1);
        }
        return success;
    }
    
    // Assume it's a fallback allocation
    std::free(ptr);
    totalDeallocations.fetch_add(1);
    return true;
}

void MemoryManager::printStatistics() const {
    fmt::print("=== Memory Manager Statistics ===\n");
    fmt::print("Total allocations: {}\n", getTotalAllocations());
    fmt::print("Total deallocations: {}\n", getTotalDeallocations());
    fmt::print("Active allocations: {}\n", getActiveAllocations());
    fmt::print("Fallback allocations: {}\n", getFallbackAllocations());
    fmt::print("Active pools: {}\n", getPoolCount());
    fmt::print("Max supported object size: {} bytes\n", getMaxSupportedSize());
}

void MemoryManager::printDetailedStatus() const {
    printStatistics();
    fmt::print("\n=== Pool Details ===\n");
    
    size_t activePoolCount = 0;
    for (size_t i = 0; i < MAX_POOLS; ++i) {
        if (pools[i]) {
            fmt::print("Pool[{}] ({}B): ", i, 1ULL << i);
            pools[i]->printStatus();
            activePoolCount++;
        }
    }
    
    if (activePoolCount == 0) {
        fmt::print("No active pools\n");
    }
}

void MemoryManager::setStrategy(std::unique_ptr<PoolStrategy> newStrategy) {
    std::lock_guard<std::mutex> lock(managerMutex);
    strategy = std::move(newStrategy);
    // Note: This doesn't affect already created pools
}

size_t MemoryManager::getPoolCount() const {
    size_t count = 0;
    for (const auto& pool : pools) {
        if (pool) count++;
    }
    return count;
}

// Global memory manager support
static MemoryManager* g_globalManager = nullptr;
static std::mutex g_globalMutex;

MemoryManager& MemoryManager::getGlobalManager() {
    std::lock_guard<std::mutex> lock(g_globalMutex);
    static MemoryManager defaultManager;
    return g_globalManager ? *g_globalManager : defaultManager;
}

void MemoryManager::setGlobalManager(MemoryManager* manager) {
    std::lock_guard<std::mutex> lock(g_globalMutex);
    g_globalManager = manager;
}

// Custom operator new/delete with specific memory manager
void* operator new(size_t size, MemoryManager& manager) {
    void* ptr = manager.allocate(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new[](size_t size, MemoryManager& manager) {
    void* ptr = manager.allocate(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void operator delete(void* ptr, MemoryManager& manager) noexcept {
    manager.deallocate(ptr);
}

void operator delete[](void* ptr, MemoryManager& manager) noexcept {
    manager.deallocate(ptr);
}

#ifdef ENABLE_GLOBAL_MEMORY_OVERRIDE
// Global operator new/delete overrides
void* operator new(size_t size) {
    void* ptr = MemoryManager::getGlobalManager().allocate(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void* operator new[](size_t size) {
    void* ptr = MemoryManager::getGlobalManager().allocate(size);
    if (!ptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    MemoryManager::getGlobalManager().deallocate(ptr);
}

void operator delete[](void* ptr) noexcept {
    MemoryManager::getGlobalManager().deallocate(ptr);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept {
    return MemoryManager::getGlobalManager().allocate(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept {
    return MemoryManager::getGlobalManager().allocate(size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
    MemoryManager::getGlobalManager().deallocate(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
    MemoryManager::getGlobalManager().deallocate(ptr);
}
#endif
