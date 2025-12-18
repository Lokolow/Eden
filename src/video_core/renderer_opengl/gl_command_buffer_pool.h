// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include "common/common_types.h"

namespace OpenGL {

/**
 * Command Buffer Pool
 * 
 * Reusable command buffer system to avoid constant malloc/free during game loop.
 * Significantly reduces memory allocation overhead and improves performance.
 * 
 * Features:
 * - Pre-allocated buffer pool
 * - Automatic buffer recycling
 * - Thread-safe operations
 * - Memory usage tracking
 * - Configurable pool size
 * - Zero-copy when possible
 */
class CommandBufferPool {
public:
    struct Config {
        // Initial number of buffers to pre-allocate
        size_t initial_pool_size = 16;
        
        // Maximum number of buffers in pool
        size_t max_pool_size = 64;
        
        // Size of each buffer (in bytes)
        size_t buffer_size = 1024 * 1024;  // 1MB default
        
        // Enable automatic pool expansion
        bool auto_expand = true;
        
        // Shrink pool when usage is low
        bool auto_shrink = true;
        
        // Frames to wait before shrinking
        u32 shrink_delay_frames = 300;  // ~5 seconds at 60fps
    };
    
    // Command buffer wrapper
    class CommandBuffer {
    public:
        CommandBuffer(size_t size);
        ~CommandBuffer();
        
        // Get raw buffer pointer
        u8* Data() { return data_.data(); }
        const u8* Data() const { return data_.data(); }
        
        // Get buffer size
        size_t Size() const { return data_.size(); }
        size_t Capacity() const { return data_.capacity(); }
        
        // Get current write position
        size_t Position() const { return position_; }
        
        // Reset buffer for reuse
        void Reset();
        
        // Write data to buffer
        template<typename T>
        void Write(const T& value) {
            Write(&value, sizeof(T));
        }
        
        void Write(const void* data, size_t size);
        
        // Reserve space
        void Reserve(size_t size);
        
        // Check if buffer has space
        bool HasSpace(size_t size) const {
            return position_ + size <= data_.size();
        }
        
        // Get remaining space
        size_t RemainingSpace() const {
            return data_.size() - position_;
        }
        
    private:
        std::vector<u8> data_;
        size_t position_ = 0;
        u64 allocation_id_ = 0;  // For tracking
        
        friend class CommandBufferPool;
    };
    
    using BufferPtr = std::shared_ptr<CommandBuffer>;
    
    explicit CommandBufferPool(Config config = {});
    ~CommandBufferPool();
    
    // Get a buffer from pool (reuses if available)
    BufferPtr AcquireBuffer();
    
    // Return buffer to pool for reuse
    void ReleaseBuffer(BufferPtr buffer);
    
    // Tick frame (for statistics and auto-shrink)
    void TickFrame();
    
    // Force pool expansion
    void ExpandPool(size_t count);
    
    // Force pool shrinking
    void ShrinkPool();
    
    // Get statistics
    struct Stats {
        size_t total_buffers = 0;
        size_t available_buffers = 0;
        size_t active_buffers = 0;
        size_t total_memory_mb = 0;
        u64 total_acquisitions = 0;
        u64 total_releases = 0;
        u64 pool_expansions = 0;
        u64 pool_shrinks = 0;
        u32 current_frame = 0;
    };
    Stats GetStats() const;
    
    // Get configuration
    const Config& GetConfig() const { return config_; }
    
private:
    Config config_;
    
    // Available buffers (ready for reuse)
    std::queue<BufferPtr> available_buffers_;
    
    // All allocated buffers (for tracking)
    std::vector<BufferPtr> all_buffers_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // Statistics
    u64 total_acquisitions_ = 0;
    u64 total_releases_ = 0;
    u64 pool_expansions_ = 0;
    u64 pool_shrinks_ = 0;
    u32 current_frame_ = 0;
    u32 last_shrink_frame_ = 0;
    
    // Allocation tracking
    u64 next_allocation_id_ = 0;
    
    // Helper methods
    BufferPtr CreateBuffer();
    void InitializePool();
    bool ShouldExpand() const;
    bool ShouldShrink() const;
};

} // namespace OpenGL
