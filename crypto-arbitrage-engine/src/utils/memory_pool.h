#pragma once

#include <memory>
#include <vector>
#include <stack>
#include <mutex>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include "core/constants.h"

namespace arbitrage {

template<typename T>
class ObjectPool {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    
    explicit ObjectPool(size_t initial_size = constants::memory::INITIAL_POOL_SIZE)
        : allocated_count_(0) {
        pool_.reserve(initial_size);
        
        for (size_t i = 0; i < initial_size; ++i) {
            pool_.push(std::make_unique<T>());
        }
    }
    
    ~ObjectPool() = default;
    
    // Get an object from the pool
    std::unique_ptr<T, std::function<void(T*)>> acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (pool_.empty()) {
            // Allocate new object if pool is empty
            allocated_count_++;
            return std::unique_ptr<T, std::function<void(T*)>>(
                new T(),
                [this](T* ptr) { release(ptr); }
            );
        }
        
        auto obj = std::move(pool_.top());
        pool_.pop();
        
        return std::unique_ptr<T, std::function<void(T*)>>(
            obj.release(),
            [this](T* ptr) { release(ptr); }
        );
    }
    
    // Return an object to the pool
    void release(T* ptr) {
        if (!ptr) return;
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Reset the object before returning to pool
        if constexpr (std::is_class_v<T>) {
            *ptr = T{};  // Reset to default state
        }
        
        pool_.push(std::unique_ptr<T>(ptr));
    }
    
    size_t available() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return pool_.size();
    }
    
    size_t allocated() const {
        return allocated_count_.load();
    }
    
private:
    mutable std::mutex mutex_;
    std::stack<std::unique_ptr<T>> pool_;
    std::atomic<size_t> allocated_count_;
};

// Fixed-size memory pool for high-frequency allocations
template<size_t BlockSize, size_t NumBlocks>
class FixedMemoryPool {
public:
    static_assert(BlockSize >= sizeof(void*), "Block size must be at least pointer size");
    
    FixedMemoryPool() : free_list_(nullptr), allocated_count_(0) {
        // Allocate aligned memory
        storage_ = aligned_alloc<char>(BlockSize * NumBlocks, constants::simd::ALIGNMENT);
        
        if (!storage_) {
            throw std::bad_alloc();
        }
        
        // Initialize free list
        for (size_t i = 0; i < NumBlocks; ++i) {
            auto block = reinterpret_cast<FreeBlock*>(storage_ + i * BlockSize);
            block->next = free_list_;
            free_list_ = block;
        }
    }
    
    ~FixedMemoryPool() {
        aligned_free(storage_);
    }
    
    void* allocate() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!free_list_) {
            return nullptr;  // Pool exhausted
        }
        
        auto block = free_list_;
        free_list_ = free_list_->next;
        allocated_count_++;
        
        return block;
    }
    
    void deallocate(void* ptr) {
        if (!ptr) return;
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto block = reinterpret_cast<FreeBlock*>(ptr);
        block->next = free_list_;
        free_list_ = block;
        allocated_count_--;
    }
    
    size_t allocated() const {
        return allocated_count_.load();
    }
    
    size_t available() const {
        return NumBlocks - allocated_count_.load();
    }
    
    static constexpr size_t block_size() { return BlockSize; }
    static constexpr size_t capacity() { return NumBlocks; }
    
private:
    struct FreeBlock {
        FreeBlock* next;
    };
    
    char* storage_;
    FreeBlock* free_list_;
    mutable std::mutex mutex_;
    std::atomic<size_t> allocated_count_;
    
    template<typename T>
    static T* aligned_alloc(size_t size, size_t alignment) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, alignment, size) != 0) {
            return nullptr;
        }
        return static_cast<T*>(ptr);
    }
    
    template<typename T>
    static void aligned_free(T* ptr) {
        free(ptr);
    }
};

// Memory pool allocator for STL containers
template<typename T>
class PoolAllocator {
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
        using other = PoolAllocator<U>;
    };
    
    PoolAllocator() : pool_(get_pool()) {}
    
    template<typename U>
    PoolAllocator(const PoolAllocator<U>&) : pool_(get_pool()) {}
    
    T* allocate(size_type n) {
        if (n != 1) {
            // Fall back to regular allocation for arrays
            return static_cast<T*>(::operator new(n * sizeof(T)));
        }
        
        auto obj = pool_->acquire();
        return obj.release();
    }
    
    void deallocate(T* ptr, size_type n) {
        if (n != 1) {
            ::operator delete(ptr);
            return;
        }
        
        pool_->release(ptr);
    }
    
    template<typename U, typename... Args>
    void construct(U* ptr, Args&&... args) {
        new(ptr) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* ptr) {
        ptr->~U();
    }
    
    bool operator==(const PoolAllocator& other) const {
        return pool_ == other.pool_;
    }
    
    bool operator!=(const PoolAllocator& other) const {
        return !(*this == other);
    }
    
private:
    static ObjectPool<T>* get_pool() {
        static ObjectPool<T> pool;
        return &pool;
    }
    
    ObjectPool<T>* pool_;
};

// Global memory pools for common sizes
class GlobalMemoryPools {
public:
    using SmallPool = FixedMemoryPool<constants::memory::SMALL_BLOCK_SIZE, 10000>;
    using MediumPool = FixedMemoryPool<constants::memory::MEDIUM_BLOCK_SIZE, 5000>;
    using LargePool = FixedMemoryPool<constants::memory::LARGE_BLOCK_SIZE, 1000>;
    
    static SmallPool& small_pool() {
        static SmallPool pool;
        return pool;
    }
    
    static MediumPool& medium_pool() {
        static MediumPool pool;
        return pool;
    }
    
    static LargePool& large_pool() {
        static LargePool pool;
        return pool;
    }
    
    static void* allocate(size_t size) {
        if (size <= SmallPool::block_size()) {
            if (auto ptr = small_pool().allocate()) {
                return ptr;
            }
        } else if (size <= MediumPool::block_size()) {
            if (auto ptr = medium_pool().allocate()) {
                return ptr;
            }
        } else if (size <= LargePool::block_size()) {
            if (auto ptr = large_pool().allocate()) {
                return ptr;
            }
        }
        
        // Fall back to regular allocation
        return ::operator new(size);
    }
    
    static void deallocate(void* ptr, size_t size) {
        if (!ptr) return;
        
        if (size <= SmallPool::block_size()) {
            small_pool().deallocate(ptr);
        } else if (size <= MediumPool::block_size()) {
            medium_pool().deallocate(ptr);
        } else if (size <= LargePool::block_size()) {
            large_pool().deallocate(ptr);
        } else {
            ::operator delete(ptr);
        }
    }
};

} // namespace arbitrage