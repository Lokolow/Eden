// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "video_core/ai_frame_generator.h"
#include <memory>

namespace OpenGL {
class OGLTexture;
class OGLFramebuffer;
}

namespace VideoCore {

/**
 * OpenGL Frame Generator - Versão do AI Frame Generator para OpenGL ES
 * 
 * Otimizado para:
 * - Mali GPUs (Huawei Mate 9, Samsung A14)
 * - Adreno GPUs
 * - OpenGL ES 3.1+
 * 
 * Usa shaders OpenGL para interpolação ao invés de CPU
 */
class OpenGLFrameGenerator {
public:
    OpenGLFrameGenerator();
    ~OpenGLFrameGenerator();
    
    void Initialize(u32 width, u32 height);
    void Shutdown();
    
    // Configuração
    void SetMode(AIFrameGenMode mode);
    void Enable(bool enable);
    bool IsEnabled() const { return enabled; }
    
    // Processamento de frame (OpenGL)
    void ProcessFrameGL(GLuint texture_prev, GLuint texture_curr, GLuint texture_next);
    GLuint GetInterpolatedTexture() const;
    
    // CPU detection (reusa do AI Frame Generator)
    CPUInfo DetectCPU();
    void OptimizeForDevice(const CPUInfo& cpu_info);
    
    // Stats
    FrameGenStats GetStats() const { return stats; }
    void ResetStats();
    
private:
    void CreateShaders();
    void CreateFramebuffers();
    void CompileInterpolationShader();
    
    // Shaders OpenGL
    GLuint interpolation_program = 0;
    GLuint motion_estimation_program = 0;
    
    // Framebuffers e texturas
    GLuint fbo_interpolated = 0;
    GLuint texture_interpolated = 0;
    GLuint texture_motion_vectors = 0;
    
    // Vertex Array Object para renderização fullscreen
    GLuint vao_fullscreen = 0;
    GLuint vbo_fullscreen = 0;
    
    // Estado
    bool enabled = false;
    bool use_advanced_interpolation = false;
    AIFrameGenMode current_mode = AIFrameGenMode::Adaptive;
    
    u32 frame_width = 1280;
    u32 frame_height = 720;
    
    CPUInfo cpu_info;
    FrameGenStats stats{};
    
    // Otimizações específicas para Mali/Adreno
    bool is_mali = false;
    bool is_adreno = false;
    bool supports_compute_shaders = false;
};

} // namespace VideoCore
