// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_texture_gc.h"

namespace OpenGL {

TextureGarbageCollector::TextureGarbageCollector(Config config) : config_(config) {
    LOG_INFO(Render_OpenGL, "Texture GC initialized - Threshold: {} frames, Aggressive: {}", 
             config_.unused_frame_threshold, config_.aggressive_mode);
}

void TextureGarbageCollector::TickFrame() {
    current_frame_++;
    
    // Log stats every 300 frames (~5 seconds at 60fps)
    if (current_frame_ % 300 == 0) {
        const auto stats = GetStats();
        LOG_DEBUG(Render_OpenGL, 
                  "Texture GC Stats - Textures: {}, VRAM: {}MB, Purged: {}, Freed: {}MB",
                  stats.total_textures, stats.total_vram_mb, 
                  stats.textures_purged, stats.vram_freed_mb);
    }
}

void TextureGarbageCollector::MarkTextureUsed(VideoCommon::ImageId image_id) {
    auto it = tracked_textures_.find(image_id);
    if (it != tracked_textures_.end()) {
        it->second.last_used_frame = current_frame_;
        it->second.usage_count++;
    }
}

void TextureGarbageCollector::RegisterTexture(VideoCommon::ImageId image_id, u64 size_bytes, 
                                               bool is_render_target) {
    TextureInfo info;
    info.size_bytes = size_bytes;
    info.last_used_frame = current_frame_;
    info.is_render_target = is_render_target;
    info.usage_count = 1;
    
    tracked_textures_[image_id] = info;
    current_vram_usage_ += size_bytes;
    
    LOG_TRACE(Render_OpenGL, "Registered texture {} - Size: {}KB, RT: {}", 
              image_id.index, size_bytes / 1024, is_render_target);
}

void TextureGarbageCollector::UnregisterTexture(VideoCommon::ImageId image_id) {
    auto it = tracked_textures_.find(image_id);
    if (it != tracked_textures_.end()) {
        current_vram_usage_ -= it->second.size_bytes;
        tracked_textures_.erase(it);
    }
}

std::vector<VideoCommon::ImageId> TextureGarbageCollector::GetTexturesToPurge() {
    std::vector<VideoCommon::ImageId> to_purge;
    const u32 threshold = GetEffectiveThreshold();
    
    for (const auto& [image_id, info] : tracked_textures_) {
        const u32 frames_unused = current_frame_ - info.last_used_frame;
        
        if (ShouldPurgeTexture(info, frames_unused)) {
            to_purge.push_back(image_id);
        }
    }
    
    // Sort by priority (least important first)
    SortTexturesByPriority(to_purge);
    
    // Under memory pressure, be more aggressive
    if (IsMemoryPressureHigh() && to_purge.size() > 10) {
        // Keep only the first N textures for purging
        to_purge.resize(std::min<size_t>(to_purge.size(), 50));
    }
    
    if (!to_purge.empty()) {
        LOG_DEBUG(Render_OpenGL, "Marking {} textures for purge (threshold: {} frames)", 
                  to_purge.size(), threshold);
    }
    
    // Update statistics
    for (const auto& id : to_purge) {
        auto it = tracked_textures_.find(id);
        if (it != tracked_textures_.end()) {
            total_textures_purged_++;
            total_vram_freed_ += it->second.size_bytes;
        }
    }
    
    return to_purge;
}

void TextureGarbageCollector::ForceCleanup(u32 target_free_mb) {
    LOG_INFO(Render_OpenGL, "Force cleanup requested - Target: {}MB", target_free_mb);
    
    std::vector<std::pair<VideoCommon::ImageId, u32>> candidates;
    
    for (const auto& [image_id, info] : tracked_textures_) {
        const u32 frames_unused = current_frame_ - info.last_used_frame;
        // Don't force-delete render targets or recently used textures
        if (!info.is_render_target && frames_unused > 10) {
            candidates.emplace_back(image_id, frames_unused);
        }
    }
    
    // Sort by frames unused (oldest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    u64 freed = 0;
    const u64 target_bytes = static_cast<u64>(target_free_mb) * 1024 * 1024;
    
    for (const auto& [image_id, _] : candidates) {
        if (freed >= target_bytes) break;
        
        auto it = tracked_textures_.find(image_id);
        if (it != tracked_textures_.end()) {
            freed += it->second.size_bytes;
            UnregisterTexture(image_id);
        }
    }
    
    LOG_INFO(Render_OpenGL, "Force cleanup freed ~{}MB", freed / 1024 / 1024);
}

bool TextureGarbageCollector::IsMemoryPressureHigh() const {
    const u64 vram_mb = current_vram_usage_ / 1024 / 1024;
    return vram_mb > config_.memory_pressure_mb || vram_mb > config_.max_vram_target_mb;
}

TextureGarbageCollector::Stats TextureGarbageCollector::GetStats() const {
    Stats stats;
    stats.total_textures = tracked_textures_.size();
    stats.total_vram_mb = current_vram_usage_ / 1024 / 1024;
    stats.textures_purged = total_textures_purged_;
    stats.vram_freed_mb = total_vram_freed_ / 1024 / 1024;
    stats.current_frame = current_frame_;
    return stats;
}

void TextureGarbageCollector::UpdateMemoryUsage(u64 current_vram_bytes) {
    current_vram_usage_ = current_vram_bytes;
}

u32 TextureGarbageCollector::GetEffectiveThreshold() const {
    if (config_.aggressive_mode && IsMemoryPressureHigh()) {
        return config_.aggressive_threshold;
    }
    return config_.unused_frame_threshold;
}

bool TextureGarbageCollector::ShouldPurgeTexture(const TextureInfo& info, u32 frames_unused) const {
    const u32 threshold = GetEffectiveThreshold();
    
    // Never purge recently used textures
    if (frames_unused < threshold) {
        return false;
    }
    
    // Render targets get extra grace period
    if (info.is_render_target) {
        return frames_unused > (threshold * 2);
    }
    
    // Frequently used textures get grace period
    if (info.usage_count > 100) {
        return frames_unused > (threshold + 30);
    }
    
    // Under memory pressure, be more aggressive
    if (IsMemoryPressureHigh()) {
        return frames_unused > (threshold / 2);
    }
    
    return frames_unused >= threshold;
}

void TextureGarbageCollector::SortTexturesByPriority(std::vector<VideoCommon::ImageId>& textures) {
    // Sort by priority: effects first, then by size (larger first), then by usage
    std::sort(textures.begin(), textures.end(), [this](const auto& a, const auto& b) {
        const auto& info_a = tracked_textures_.at(a);
        const auto& info_b = tracked_textures_.at(b);
        
        // Render targets have lowest priority for deletion (keep them)
        if (info_a.is_render_target != info_b.is_render_target) {
            return !info_a.is_render_target && info_b.is_render_target;
        }
        
        // Larger textures are prioritized for deletion (free more memory)
        if (info_a.size_bytes != info_b.size_bytes) {
            return info_a.size_bytes > info_b.size_bytes;
        }
        
        // Less frequently used textures deleted first
        return info_a.usage_count < info_b.usage_count;
    });
}

} // namespace OpenGL
