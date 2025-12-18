// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <jni.h>
#include "video_core/ai_frame_generator.h"
#include "video_core/gpu.h"
#include "core/core.h"
#include "common/logging/log.h"

extern "C" {

static VideoCore::AIFrameGenerator* g_ai_frame_gen = nullptr;

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_initAIFrameGenerator(JNIEnv* env, jclass clazz) {
    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn() && system.GPU().Renderer()) {
        g_ai_frame_gen = new VideoCore::AIFrameGenerator(system.GPU());
        g_ai_frame_gen->Initialize();
        LOG_INFO(Frontend, "AI Frame Generator initialized from Java");
    }
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_shutdownAIFrameGenerator(JNIEnv* env, jclass clazz) {
    if (g_ai_frame_gen) {
        g_ai_frame_gen->Shutdown();
        delete g_ai_frame_gen;
        g_ai_frame_gen = nullptr;
        LOG_INFO(Frontend, "AI Frame Generator shutdown from Java");
    }
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_setAIFrameGenEnabled(JNIEnv* env, jclass clazz, jboolean enabled) {
    if (g_ai_frame_gen) {
        g_ai_frame_gen->Enable(enabled);
    }
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_setAIFrameGenMode(JNIEnv* env, jclass clazz, jint mode) {
    if (g_ai_frame_gen && mode >= 0 && mode <= 4) {
        g_ai_frame_gen->SetMode(static_cast<VideoCore::AIFrameGenMode>(mode));
    }
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_setAIFrameGenTargetFPS(JNIEnv* env, jclass clazz, jint target_fps) {
    if (g_ai_frame_gen) {
        g_ai_frame_gen->SetTargetFPS(static_cast<u32>(target_fps));
    }
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_setAIFrameGenMemoryLimit(JNIEnv* env, jclass clazz, jint limit_mb) {
    if (g_ai_frame_gen) {
        g_ai_frame_gen->SetMemoryLimit(static_cast<u32>(limit_mb));
    }
}

JNIEXPORT void JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_setAIFrameGenUseNEON(JNIEnv* env, jclass clazz, jboolean use_neon) {
    if (g_ai_frame_gen) {
        g_ai_frame_gen->EnableNEONOptimizations(use_neon);
    }
}

JNIEXPORT jobject JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_getAIFrameGenCPUInfo(JNIEnv* env, jclass clazz) {
    if (!g_ai_frame_gen) {
        return nullptr;
    }

    auto cpu_info = g_ai_frame_gen->DetectCPU();
    
    // Create CPUInfo Java object
    jclass cpuInfoClass = env->FindClass("org/yuzu/yuzu_emu/model/CPUInfo");
    if (!cpuInfoClass) {
        return nullptr;
    }

    jmethodID constructor = env->GetMethodID(cpuInfoClass, "<init>", "(IIIJIZLjava/lang/String;)V");
    if (!constructor) {
        return nullptr;
    }

    jstring cpuModel = env->NewStringUTF(cpu_info.cpu_model.c_str());
    
    return env->NewObject(
        cpuInfoClass, 
        constructor,
        static_cast<jint>(cpu_info.arch),
        static_cast<jint>(cpu_info.core_count),
        static_cast<jint>(cpu_info.big_cores),
        static_cast<jlong>(cpu_info.max_freq_mhz),
        static_cast<jint>(cpu_info.ram_mb),
        static_cast<jboolean>(cpu_info.has_neon),
        cpuModel
    );
}

JNIEXPORT jobject JNICALL
Java_org_yuzu_yuzu_1emu_NativeLibrary_getAIFrameGenStats(JNIEnv* env, jclass clazz) {
    if (!g_ai_frame_gen) {
        return nullptr;
    }

    auto stats = g_ai_frame_gen->GetStats();
    
    jclass statsClass = env->FindClass("org/yuzu/yuzu_emu/model/FrameGenStats");
    if (!statsClass) {
        return nullptr;
    }

    jmethodID constructor = env->GetMethodID(statsClass, "<init>", "(JJJFFFFIF)V");
    if (!constructor) {
        return nullptr;
    }
    
    return env->NewObject(
        statsClass,
        constructor,
        static_cast<jlong>(stats.frames_generated),
        static_cast<jlong>(stats.frames_skipped),
        static_cast<jlong>(stats.frames_interpolated),
        static_cast<jfloat>(stats.current_fps),
        static_cast<jfloat>(stats.target_fps),
        static_cast<jfloat>(stats.cpu_usage_percent),
        static_cast<jfloat>(stats.gpu_usage_percent),
        static_cast<jint>(stats.ram_usage_mb),
        static_cast<jfloat>(stats.frame_time_ms)
    );
}

} // extern "C"
