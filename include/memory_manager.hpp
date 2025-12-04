#pragma once

#include <memory>
#include <vector>
#include <array>
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <new>
#include <atomic>

// Forward declarations
class MemoryPool;
class MemoryManager;

// Pool allocation strategy interface
class PoolStrategy {
public:
    virtual ~PoolStrategy() = default;
    virtual size_t calculatePoolSize(size_t objectSize) const = 0;
    virtual size_t calculateObjectCount(size_t objectSize) const = 0;
};

// Default strategy: 256 * object_size pool size
class DefaultPoolStrategy : public PoolStrategy {
public:
    size_t calculatePoolSize(size_t objectSize) const override {
        return 256 * objectSize;
    }
    
    size_t calculateObjectCount(size_t objectSize) const override {
        return 256;
    }
};

// Memory pool for objects of a specific size
class MemoryPool {
private:
    size_t objectSize;
    size_t objectCount;
    size_t poolSize;
    std::unique_ptr<uint8_t[]> memory;
    std::vector<void*> freeList;
    std::mutex poolMutex;
    
    // Statistics
    std::atomic<size_t> allocatedObjects{0};
    std::atomic<size_t> totalAllocations{0};
    std::atomic<size_t> totalDeallocations{0};
    
    void initializePool();
    
public:
    MemoryPool(size_t objSize, size_t objCount);
    ~MemoryPool() = default;
    
    // Non-copyable, non-movable
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;
    
    void* allocate();
    bool deallocate(void* ptr);
    
    bool containsPointer(void* ptr) const;
    size_t getObjectSize() const { return objectSize; }
    size_t getObjectCount() const { return objectCount; }
    size_t getAvailableObjects() const;
    size_t getAllocatedObjects() const { return allocatedObjects.load(); }
    size_t getTotalAllocations() const { return totalAllocations.load(); }
    size_t getTotalDeallocations() const { return totalDeallocations.load(); }
    
    void printStatus() const;
};

// Main memory manager with size-based pools
class MemoryManager {
private:
    // Maximum supported object size (2^20 = 1MB)
    static constexpr size_t MAX_POOL_SIZE_BITS = 20;
    static constexpr size_t MAX_POOLS = MAX_POOL_SIZE_BITS + 1;
    
    std::array<std::unique_ptr<MemoryPool>, MAX_POOLS> pools;
    std::array<std::once_flag, MAX_POOLS> poolInitFlags;
    std::unique_ptr<PoolStrategy> strategy;
    mutable std::mutex managerMutex;
    
    // Statistics
    std::atomic<size_t> totalAllocations{0};
    std::atomic<size_t> totalDeallocations{0};
    std::atomic<size_t> fallbackAllocations{0};
    
    // Helper functions
    static size_t roundToPowerOf2(size_t size);
    static size_t getPoolIndex(size_t size);
    MemoryPool* getOrCreatePool(size_t size);
    MemoryPool* findPoolForPointer(void* ptr);
    
public:
    MemoryManager();
    explicit MemoryManager(std::unique_ptr<PoolStrategy> poolStrategy);
    ~MemoryManager() = default;
    
    // Non-copyable
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    
    // Memory allocation interface
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));
    bool deallocate(void* ptr);
    
    // Typed allocation helpers
    template<typename T>
    T* allocate(size_t count = 1) {
        size_t totalSize = sizeof(T) * count;
        void* ptr = allocate(totalSize, alignof(T));
        return static_cast<T*>(ptr);
    }
    
    template<typename T>
    bool deallocate(T* ptr) {
        return deallocate(static_cast<void*>(ptr));
    }
    
    // Construction/destruction helpers
    template<typename T, typename... Args>
    T* construct(Args&&... args) {
        T* ptr = allocate<T>(1);
        if (ptr) {
            new(ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }
    
    template<typename T>
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            deallocate(ptr);
        }
    }
    
    // Array construction/destruction
    template<typename T>
    T* constructArray(size_t count) {
        T* ptr = allocate<T>(count);
        if (ptr) {
            for (size_t i = 0; i < count; ++i) {
                new(ptr + i) T();
            }
        }
        return ptr;
    }
    
    template<typename T>
    void destroyArray(T* ptr, size_t count) {
        if (ptr) {
            for (size_t i = 0; i < count; ++i) {
                ptr[i].~T();
            }
            deallocate(ptr);
        }
    }
    
    // Statistics and information
    size_t getTotalAllocations() const { return totalAllocations.load(); }
    size_t getTotalDeallocations() const { return totalDeallocations.load(); }
    size_t getFallbackAllocations() const { return fallbackAllocations.load(); }
    size_t getActiveAllocations() const { return getTotalAllocations() - getTotalDeallocations(); }
    
    void printStatistics() const;
    void printDetailedStatus() const;
    
    // Pool management
    void setStrategy(std::unique_ptr<PoolStrategy> newStrategy);
    size_t getPoolCount() const;
    
    // Static interface for global memory manager
    static MemoryManager& getGlobalManager();
    static void setGlobalManager(MemoryManager* manager);
    
    // Utility functions
    static size_t getSizeCategory(size_t size) { return roundToPowerOf2(size); }
    static size_t getMaxSupportedSize() { return 1ULL << MAX_POOL_SIZE_BITS; }
};

// Custom allocator class for STL containers
template<typename T>
class MemoryManagerAllocator {
private:
    MemoryManager* manager;
    
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = MemoryManagerAllocator<U>;
    };
    
    explicit MemoryManagerAllocator(MemoryManager* mgr = nullptr) 
        : manager(mgr ? mgr : &MemoryManager::getGlobalManager()) {}
    
    template<typename U>
    MemoryManagerAllocator(const MemoryManagerAllocator<U>& other) 
        : manager(other.getManager()) {}
    
    pointer allocate(size_type n) {
        void* ptr = manager->allocate(n * sizeof(T), alignof(T));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(ptr);
    }
    
    void deallocate(pointer p, size_type n) {
        (void)n; // Unused parameter
        manager->deallocate(p);
    }
    
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new(p) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
    
    MemoryManager* getManager() const { return manager; }
    
    template<typename U>
    bool operator==(const MemoryManagerAllocator<U>& other) const {
        return manager == other.getManager();
    }
    
    template<typename U>
    bool operator!=(const MemoryManagerAllocator<U>& other) const {
        return !(*this == other);
    }
};

// Convenience typedefs for common STL containers
template<typename T>
using managed_vector = std::vector<T, MemoryManagerAllocator<T>>;

template<typename Key, typename T>
using managed_map = std::map<Key, T, std::less<Key>, 
    MemoryManagerAllocator<std::pair<const Key, T>>>;

template<typename T>
using managed_set = std::set<T, std::less<T>, MemoryManagerAllocator<T>>;

using managed_string = std::basic_string<char, std::char_traits<char>, 
    MemoryManagerAllocator<char>>;

// Global operator new/delete overrides
// These can be enabled/disabled by defining ENABLE_GLOBAL_MEMORY_OVERRIDE

#ifdef ENABLE_GLOBAL_MEMORY_OVERRIDE
void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void* operator new(size_t size, const std::nothrow_t&) noexcept;
void* operator new[](size_t size, const std::nothrow_t&) noexcept;
void operator delete(void* ptr, const std::nothrow_t&) noexcept;
void operator delete[](void* ptr, const std::nothrow_t&) noexcept;
#endif

// Placement new with specific memory manager
void* operator new(size_t size, MemoryManager& manager);
void* operator new[](size_t size, MemoryManager& manager);
void operator delete(void* ptr, MemoryManager& manager) noexcept;
void operator delete[](void* ptr, MemoryManager& manager) noexcept;
