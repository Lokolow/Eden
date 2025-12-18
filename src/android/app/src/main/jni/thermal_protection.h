// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <string>

namespace AndroidThermal {

enum class DeviceModel {
    Unknown,
    HuaweiMate9,      // Kirin 960 - CRITICAL: Aquece muito!
    SamsungA14_5G,    // Dimensity 700 - Melhor controle térmico
    Generic,
};

enum class ThermalLevel {
    Safe = 0,         // < 40°C - Tudo OK
    Warning = 1,      // 40-45°C - Reduzir qualidade
    Hot = 2,          // 45-50°C - Throttle moderado
    Critical = 3,     // 50-55°C - Throttle agressivo
    Emergency = 4,    // > 55°C - PARAR IMEDIATAMENTE
};

struct ThermalConfig {
    f32 safe_temp = 40.0f;
    f32 warning_temp = 45.0f;
    f32 hot_temp = 50.0f;
    f32 critical_temp = 55.0f;
    f32 emergency_temp = 60.0f;
    
    // Mate 9 precisa de limites mais conservadores
    static ThermalConfig ForMate9() {
        ThermalConfig config;
        config.safe_temp = 38.0f;      // Mais conservador
        config.warning_temp = 42.0f;
        config.hot_temp = 47.0f;
        config.critical_temp = 52.0f;
        config.emergency_temp = 57.0f;
        return config;
    }
    
    static ThermalConfig ForA14() {
        ThermalConfig config;
        config.safe_temp = 42.0f;      // Pode aguentar mais
        config.warning_temp = 48.0f;
        config.hot_temp = 52.0f;
        config.critical_temp = 57.0f;
        config.emergency_temp = 62.0f;
        return config;
    }
};

class ThermalMonitor {
public:
    ThermalMonitor();
    ~ThermalMonitor();
    
    void Initialize(DeviceModel device);
    void StartMonitoring();
    void StopMonitoring();
    
    f32 GetCurrentTemperature() const { return current_temp.load(); }
    ThermalLevel GetThermalLevel() const { return current_level.load(); }
    bool IsSafeToRun() const { return current_level.load() < ThermalLevel::Emergency; }
    
    // Callbacks para ajustes automáticos
    void SetOnWarningCallback(std::function<void()> callback) { on_warning = callback; }
    void SetOnCriticalCallback(std::function<void()> callback) { on_critical = callback; }
    void SetOnEmergencyCallback(std::function<void()> callback) { on_emergency = callback; }
    
private:
    void MonitorThread();
    f32 ReadTemperatureFromSensor();
    void ApplyThrottling(ThermalLevel level);
    
    std::atomic<f32> current_temp{0.0f};
    std::atomic<ThermalLevel> current_level{ThermalLevel::Safe};
    std::atomic<bool> running{false};
    std::jthread monitor_thread;
    
    DeviceModel device_model{DeviceModel::Unknown};
    ThermalConfig config;
    
    std::function<void()> on_warning;
    std::function<void()> on_critical;
    std::function<void()> on_emergency;
    
    static constexpr auto CHECK_INTERVAL = std::chrono::seconds(3);
    static constexpr int EMERGENCY_COOLDOWN_SECONDS = 30;
};

// Detecta o modelo do dispositivo automaticamente
DeviceModel DetectDeviceModel();

} // namespace AndroidThermal
