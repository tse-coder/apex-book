#pragma once
#include <atomic>
#include <vector>
#include <cstddef>
#include <stdexcept>
#include <bit>

template<typename T>
class SPSCQueue {
private:
    std::vector<T> buffer;
    const size_t capacity_mask;
    
    // Prevent false sharing by forcing these to different cache lines
    alignas(64) std::atomic<size_t> write_idx{0};
    alignas(64) std::atomic<size_t> read_idx{0};

public:
    SPSCQueue(size_t capacity) : 
        buffer(std::bit_ceil(capacity)), 
        capacity_mask(std::bit_ceil(capacity) - 1) 
    {
    }

    bool push(const T& item) {
        const size_t current_write = write_idx.load(std::memory_order_relaxed);
        const size_t next_write = current_write + 1;
        
        if ((current_write - read_idx.load(std::memory_order_acquire)) >= buffer.size()) {
            return false; // Queue full
        }
        
        buffer[current_write & capacity_mask] = item;
        write_idx.store(next_write, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        const size_t current_read = read_idx.load(std::memory_order_relaxed);
        
        if (current_read == write_idx.load(std::memory_order_acquire)) {
            return false; // Queue empty
        }
        
        item = buffer[current_read & capacity_mask];
        read_idx.store(current_read + 1, std::memory_order_release);
        return true;
    }
};
