// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "opengl_frame_generator.h"
#include "common/logging/log.h"
#include <glad/glad.h>
#include <cstring>

namespace VideoCore {

// Shader para interpolação simples de frames
const char* INTERPOLATION_VERTEX_SHADER = R"(
#version 310 es
precision highp float;

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

out vec2 v_texcoord;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    v_texcoord = texcoord;
}
)";

const char* INTERPOLATION_FRAGMENT_SHADER = R"(
#version 310 es
precision highp float;

in vec2 v_texcoord;
out vec4 frag_color;

uniform sampler2D u_texture_prev;
uniform sampler2D u_texture_next;
uniform float u_blend_factor; // 0.5 para interpolação no meio

void main() {
    vec4 color_prev = texture(u_texture_prev, v_texcoord);
    vec4 color_next = texture(u_texture_next, v_texcoord);
    
    // Interpolação linear simples
    frag_color = mix(color_prev, color_next, u_blend_factor);
}
)";

OpenGLFrameGenerator::OpenGLFrameGenerator() {
    LOG_INFO(Render_OpenGL, "OpenGL Frame Generator initialized");
}

OpenGLFrameGenerator::~OpenGLFrameGenerator() {
    Shutdown();
}

void OpenGLFrameGenerator::Initialize(u32 width, u32 height) {
    frame_width = width;
    frame_height = height;
    
    // Detectar GPU
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::string renderer_str(renderer ? renderer : "Unknown");
    
    is_mali = renderer_str.find("Mali") != std::string::npos;
    is_adreno = renderer_str.find("Adreno") != std::string::npos;
    
    LOG_INFO(Render_OpenGL, "GPU Detected: {} (Mali: {}, Adreno: {})", 
             renderer_str, is_mali, is_adreno);
    
    // Detectar suporte a compute shaders
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    supports_compute_shaders = (major > 3) || (major == 3 && minor >= 1);
    
    // Criar shaders e framebuffers
    CreateShaders();
    CreateFramebuffers();
    
    // Detectar CPU
    cpu_info = DetectCPU();
    OptimizeForDevice(cpu_info);
    
    LOG_INFO(Render_OpenGL, "OpenGL Frame Generator ready - {}x{}", width, height);
}

void OpenGLFrameGenerator::CreateShaders() {
    // Criar programa de interpolação
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &INTERPOLATION_VERTEX_SHADER, nullptr);
    glCompileShader(vertex_shader);
    
    // Verificar compilação
    GLint success;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(vertex_shader, 512, nullptr, log);
        LOG_ERROR(Render_OpenGL, "Vertex shader compilation failed: {}", log);
        return;
    }
    
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &INTERPOLATION_FRAGMENT_SHADER, nullptr);
    glCompileShader(fragment_shader);
    
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(fragment_shader, 512, nullptr, log);
        LOG_ERROR(Render_OpenGL, "Fragment shader compilation failed: {}", log);
        glDeleteShader(vertex_shader);
        return;
    }
    
    // Linkar programa
    interpolation_program = glCreateProgram();
    glAttachShader(interpolation_program, vertex_shader);
    glAttachShader(interpolation_program, fragment_shader);
    glLinkProgram(interpolation_program);
    
    glGetProgramiv(interpolation_program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(interpolation_program, 512, nullptr, log);
        LOG_ERROR(Render_OpenGL, "Shader program linking failed: {}", log);
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    LOG_INFO(Render_OpenGL, "Interpolation shader compiled successfully");
}

void OpenGLFrameGenerator::CreateFramebuffers() {
    // Criar textura para frame interpolado
    glGenTextures(1, &texture_interpolated);
    glBindTexture(GL_TEXTURE_2D, texture_interpolated);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, frame_width, frame_height, 
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Criar FBO
    glGenFramebuffers(1, &fbo_interpolated);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_interpolated);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                          GL_TEXTURE_2D, texture_interpolated, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR(Render_OpenGL, "Framebuffer is not complete!");
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Criar VAO para fullscreen quad
    glGenVertexArrays(1, &vao_fullscreen);
    glGenBuffers(1, &vbo_fullscreen);
    
    // Fullscreen triangle vertices
    float vertices[] = {
        // Pos        // TexCoord
        -1.0f, -1.0f,  0.0f, 0.0f,
         3.0f, -1.0f,  2.0f, 0.0f,
        -1.0f,  3.0f,  0.0f, 2.0f
    };
    
    glBindVertexArray(vao_fullscreen);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_fullscreen);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
    
    LOG_INFO(Render_OpenGL, "Framebuffers created successfully");
}

void OpenGLFrameGenerator::Shutdown() {
    if (interpolation_program) {
        glDeleteProgram(interpolation_program);
        interpolation_program = 0;
    }
    
    if (fbo_interpolated) {
        glDeleteFramebuffers(1, &fbo_interpolated);
        fbo_interpolated = 0;
    }
    
    if (texture_interpolated) {
        glDeleteTextures(1, &texture_interpolated);
        texture_interpolated = 0;
    }
    
    if (vao_fullscreen) {
        glDeleteVertexArrays(1, &vao_fullscreen);
        vao_fullscreen = 0;
    }
    
    if (vbo_fullscreen) {
        glDeleteBuffers(1, &vbo_fullscreen);
        vbo_fullscreen = 0;
    }
    
    LOG_INFO(Render_OpenGL, "OpenGL Frame Generator shutdown");
}

void OpenGLFrameGenerator::ProcessFrameGL(GLuint texture_prev, GLuint texture_curr, 
                                         GLuint texture_next) {
    if (!enabled || !interpolation_program) {
        return;
    }
    
    // Salvar estado OpenGL
    GLint old_fbo, old_program, old_vao;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
    glGetIntegerv(GL_CURRENT_PROGRAM, &old_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &old_vao);
    
    // Renderizar frame interpolado
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_interpolated);
    glViewport(0, 0, frame_width, frame_height);
    
    glUseProgram(interpolation_program);
    
    // Bind texturas
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_prev);
    glUniform1i(glGetUniformLocation(interpolation_program, "u_texture_prev"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture_next);
    glUniform1i(glGetUniformLocation(interpolation_program, "u_texture_next"), 1);
    
    // Blend factor (0.5 = meio do caminho entre os frames)
    glUniform1f(glGetUniformLocation(interpolation_program, "u_blend_factor"), 0.5f);
    
    // Renderizar fullscreen quad
    glBindVertexArray(vao_fullscreen);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    
    // Restaurar estado
    glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
    glUseProgram(old_program);
    glBindVertexArray(old_vao);
    
    stats.frames_interpolated++;
}

GLuint OpenGLFrameGenerator::GetInterpolatedTexture() const {
    return texture_interpolated;
}

CPUInfo OpenGLFrameGenerator::DetectCPU() {
    // Reutilizar função do AI Frame Generator
    AIFrameGenerator temp_gen(*(Tegra::GPU*)nullptr); // Temporário
    return temp_gen.DetectCPU();
}

void OpenGLFrameGenerator::OptimizeForDevice(const CPUInfo& info) {
    cpu_info = info;
    
    // Otimizações específicas
    if (is_mali) {
        LOG_INFO(Render_OpenGL, "Applying Mali GPU optimizations");
        // Mali tem largura de banda limitada, usar resoluções menores
        use_advanced_interpolation = false;
    }
    
    if (is_adreno) {
        LOG_INFO(Render_OpenGL, "Applying Adreno GPU optimizations");
        // Adreno tem bom desempenho com shaders
        use_advanced_interpolation = cpu_info.arch >= CPUArchitecture::ARM_Cortex_A73;
    }
}

void OpenGLFrameGenerator::SetMode(AIFrameGenMode mode) {
    current_mode = mode;
    LOG_INFO(Render_OpenGL, "Frame gen mode: {}", static_cast<u32>(mode));
}

void OpenGLFrameGenerator::Enable(bool enable) {
    enabled = enable;
    LOG_INFO(Render_OpenGL, "Frame generation: {}", enable ? "enabled" : "disabled");
}

void OpenGLFrameGenerator::ResetStats() {
    stats = FrameGenStats{};
}

} // namespace VideoCore
