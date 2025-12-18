// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_command_buffer_pool.h"

namespace OpenGL {

// CommandBuffer implementation
CommandBufferPool::CommandBuffer::CommandBuffer(size_t size) {
    data_.resize(size);
    position_ = 0;
}

CommandBufferPool::CommandBuffer::~CommandBuffer() = default;

void CommandBufferPool::CommandBuffer::Reset() {
    position_ = 0;
    // Don't deallocate, just reset position for reuse
}

void CommandBufferPool::CommandBuffer::Write(const void* data, size_t size) {
    if (!HasSpace(size)) {
        // Auto-expand if needed
        const size_t new_size = std::max(data_.size() * 2, position_ + size);
        data_.resize(new_size);
        LOG_DEBUG(Render_OpenGL, "CommandBuffer auto-expanded to {} bytes", new_size);
    }
    
    std::memcpy(data_.data() + position_, data, size);
    position_ += size;
}

void CommandBufferPool::CommandBuffer::Reserve(size_t size) {
    if (data_.capacity() < size) {
        data_.reserve(size);
    }
}

// CommandBufferPool implementation
CommandBufferPool::CommandBufferPool(Config config) : config_(config) {
    LOG_INFO(Render_OpenGL, "CommandBufferPool initialized - Size: {}KB, Pool: {}-{} buffers",
             config_.buffer_size / 1024, config_.initial_pool_size, config_.max_pool_size);
    
    InitializePool();
}

CommandBufferPool::~CommandBufferPool() {
    const auto stats = GetStats();
    LOG_INFO(Render_OpenGL, 
             "CommandBufferPool destroyed - Acquisitions: {}, Releases: {}, Expansions: {}, Shrinks: {}",
             stats.total_acquisitions, stats.total_releases, 
             stats.pool_expansions, stats.pool_shrinks);
}

void CommandBufferPool::InitializePool() {
    std::lock_guard lock(mutex_);
    
    for (size_t i = 0; i < config_.initial_pool_size; ++i) {
        auto buffer = CreateBuffer();
        available_buffers_.push(buffer);
        all_buffers_.push_back(buffer);
    }
    
    LOG_DEBUG(Render_OpenGL, "Pre-allocated {} command buffers ({}MB total)",
              config_.initial_pool_size,
              (config_.initial_pool_size * config_.buffer_size) / (1024 * 1024));
}

CommandBufferPool::BufferPtr CommandBufferPool::CreateBuffer() {
    auto buffer = std::make_shared<CommandBuffer>(config_.buffer_size);
    buffer->allocation_id_ = next_allocation_id_++;
    return buffer;
}

CommandBufferPool::BufferPtr CommandBufferPool::AcquireBuffer() {
    std::lock_guard lock(mutex_);
    
    BufferPtr buffer;
    
    if (!available_buffers_.empty()) {
        // Reuse existing buffer
        buffer = available_buffers_.front();
        available_buffers_.pop();
        buffer->Reset();
    } else {
        // Need to allocate new buffer
        if (config_.auto_expand && all_buffers_.size() < config_.max_pool_size) {
            buffer = CreateBuffer();
            all_buffers_.push_back(buffer);
            pool_expansions_++;
            
            LOG_DEBUG(Render_OpenGL, "Pool expanded - Total buffers: {}", all_buffers_.size());
        } else {
            // Pool is at max size, allocate temporary buffer
            buffer = CreateBuffer();
            LOG_WARNING(Render_OpenGL, 
                        "Pool exhausted! Allocating temporary buffer (consider increasing max_pool_size)");
        }
    }
    
    total_acquisitions_++;
    return buffer;
}

void CommandBufferPool::ReleaseBuffer(BufferPtr buffer) {
    if (!buffer) {
        return;
    }
    
    std::lock_guard lock(mutex_);
    
    // Check if buffer is from our pool
    bool is_pool_buffer = false;
    for (const auto& pool_buffer : all_buffers_) {
        if (pool_buffer.get() == buffer.get()) {
            is_pool_buffer = true;
            break;
        }
    }
    
    if (is_pool_buffer) {
        // Return to pool for reuse
        buffer->Reset();
        available_buffers_.push(buffer);
    }
    // If not a pool buffer, just let it be destroyed
    
    total_releases_++;
}

void CommandBufferPool::TickFrame() {
    current_frame_++;
    
    // Check if we should shrink the pool
    if (config_.auto_shrink && ShouldShrink()) {
        ShrinkPool();
    }
    
    // Log stats every 5 seconds
    if (current_frame_ % 300 == 0) {
        const auto stats = GetStats();
        LOG_DEBUG(Render_OpenGL,
                  "CommandBufferPool Stats - Total: {}, Available: {}, Active: {}, Memory: {}MB",
                  stats.total_buffers, stats.available_buffers, 
                  stats.active_buffers, stats.total_memory_mb);
    }
}

void CommandBufferPool::ExpandPool(size_t count) {
    std::lock_guard lock(mutex_);
    
    const size_t new_total = all_buffers_.size() + count;
    if (new_total > config_.max_pool_size) {
        count = config_.max_pool_size - all_buffers_.size();
    }
    
    for (size_t i = 0; i < count; ++i) {
        auto buffer = CreateBuffer();
        available_buffers_.push(buffer);
        all_buffers_.push_back(buffer);
    }
    
    pool_expansions_++;
    LOG_INFO(Render_OpenGL, "Pool manually expanded by {} buffers - Total: {}", 
             count, all_buffers_.size());
}

void CommandBufferPool::ShrinkPool() {
    std::lock_guard lock(mutex_);
    
    // Only shrink if we have excess available buffers
    const size_t available_count = available_buffers_.size();
    const size_t target_available = config_.initial_pool_size / 2;
    
    if (available_count <= target_available) {
        return;
    }
    
    const size_t to_remove = available_count - target_available;
    
    // Remove excess buffers
    for (size_t i = 0; i < to_remove && !available_buffers_.empty(); ++i) {
        auto buffer = available_buffers_.front();
        available_buffers_.pop();
        
        // Remove from all_buffers
        all_buffers_.erase(
            std::remove_if(all_buffers_.begin(), all_buffers_.end(),
                          [&buffer](const BufferPtr& b) { return b.get() == buffer.get(); }),
            all_buffers_.end()
        );
    }
    
    pool_shrinks_++;
    last_shrink_frame_ = current_frame_;
    
    LOG_INFO(Render_OpenGL, "Pool shrunk by {} buffers - Total: {}", 
             to_remove, all_buffers_.size());
}

bool CommandBufferPool::ShouldExpand() const {
    return available_buffers_.empty() && all_buffers_.size() < config_.max_pool_size;
}

bool CommandBufferPool::ShouldShrink() const {
    // Don't shrink too frequently
    if (current_frame_ - last_shrink_frame_ < config_.shrink_delay_frames) {
        return false;
    }
    
    // Shrink if we have too many available buffers
    const size_t available_count = available_buffers_.size();
    const size_t total_count = all_buffers_.size();
    
    // If more than 75% are available, we can shrink
    return total_count > config_.initial_pool_size && 
           available_count > (total_count * 3 / 4);
}

CommandBufferPool::Stats CommandBufferPool::GetStats() const {
    std::lock_guard lock(mutex_);
    
    Stats stats;
    stats.total_buffers = all_buffers_.size();
    stats.available_buffers = available_buffers_.size();
    stats.active_buffers = stats.total_buffers - stats.available_buffers;
    stats.total_memory_mb = (stats.total_buffers * config_.buffer_size) / (1024 * 1024);
    stats.total_acquisitions = total_acquisitions_;
    stats.total_releases = total_releases_;
    stats.pool_expansions = pool_expansions_;
    stats.pool_shrinks = pool_shrinks_;
    stats.current_frame = current_frame_;
    
    return stats;
}

} // namespace OpenGL
