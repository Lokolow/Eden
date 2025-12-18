// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "thermal_protection.h"
#include "common/logging/log.h"
#include "common/settings.h"

#ifdef __ANDROID__
#include <fstream>
#include <sstream>
#include <sys/system_properties.h>
#include <android/api-level.h>
#endif

namespace AndroidThermal {

DeviceModel DetectDeviceModel() {
#ifdef __ANDROID__
    char manufacturer[PROP_VALUE_MAX] = {0};
    char model[PROP_VALUE_MAX] = {0};
    char device[PROP_VALUE_MAX] = {0};
    
    __system_property_get("ro.product.manufacturer", manufacturer);
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.product.device", device);
    
    std::string mfr(manufacturer);
    std::string mdl(model);
    std::string dev(device);
    
    LOG_INFO(Frontend, "Device Detection: {} {} ({})", mfr, mdl, dev);
    
    // Huawei Mate 9 (vários nomes possíveis)
    if (mfr.find("HUAWEI") != std::string::npos || mfr.find("Huawei") != std::string::npos) {
        if (mdl.find("MHA") != std::string::npos ||  // MHA-L29, MHA-L09
            mdl.find("Mate 9") != std::string::npos ||
            dev.find("hi3660") != std::string::npos) { // Kirin 960
            LOG_WARNING(Frontend, "⚠️ HUAWEI MATE 9 DETECTED - THERMAL PROTECTION CRITICAL!");
            return DeviceModel::HuaweiMate9;
        }
    }
    
    // Samsung Galaxy A14 5G
    if (mfr.find("samsung") != std::string::npos || mfr.find("Samsung") != std::string::npos) {
        if (mdl.find("SM-A146") != std::string::npos ||  // A14 5G model number
            mdl.find("A14") != std::string::npos) {
            LOG_INFO(Frontend, "Samsung Galaxy A14 5G detected");
            return DeviceModel::SamsungA14_5G;
        }
    }
    
    LOG_INFO(Frontend, "Generic Android device detected");
    return DeviceModel::Generic;
#else
    return DeviceModel::Unknown;
#endif
}

ThermalMonitor::ThermalMonitor() {
    LOG_INFO(Frontend, "Thermal Monitor initialized");
}

ThermalMonitor::~ThermalMonitor() {
    StopMonitoring();
}

void ThermalMonitor::Initialize(DeviceModel device) {
    device_model = device;
    
    // Configurar limites baseados no dispositivo
    switch (device) {
        case DeviceModel::HuaweiMate9:
            config = ThermalConfig::ForMate9();
            LOG_WARNING(Frontend, "🔥 Mate 9: Limites conservadores de temperatura ativados!");
            LOG_WARNING(Frontend, "   Safe: {:.1f}°C | Warning: {:.1f}°C | Critical: {:.1f}°C",
                       config.safe_temp, config.warning_temp, config.critical_temp);
            break;
            
        case DeviceModel::SamsungA14_5G:
            config = ThermalConfig::ForA14();
            LOG_INFO(Frontend, "Samsung A14 5G: Limites padrão de temperatura");
            break;
            
        default:
            config = ThermalConfig(); // Padrão
            LOG_INFO(Frontend, "Generic device: Limites padrão de temperatura");
            break;
    }
}

void ThermalMonitor::StartMonitoring() {
    if (running.exchange(true)) {
        LOG_WARNING(Frontend, "Thermal monitoring already running");
        return;
    }
    
    LOG_INFO(Frontend, "🌡️ Starting thermal monitoring...");
    
    monitor_thread = std::jthread([this](std::stop_token stop_token) {
        MonitorThread();
    });
}

void ThermalMonitor::StopMonitoring() {
    running = false;
    if (monitor_thread.joinable()) {
        monitor_thread.request_stop();
    }
    LOG_INFO(Frontend, "Thermal monitoring stopped");
}

f32 ThermalMonitor::ReadTemperatureFromSensor() {
#ifdef __ANDROID__
    // Tentar múltiplos sensores de temperatura
    std::vector<std::string> thermal_zones = {
        "/sys/class/thermal/thermal_zone0/temp",  // CPU principal
        "/sys/class/thermal/thermal_zone1/temp",  // GPU
        "/sys/class/thermal/thermal_zone2/temp",  // Alternativo
        "/sys/devices/virtual/thermal/thermal_zone0/temp",
        "/sys/devices/virtual/thermal/thermal_zone1/temp",
    };
    
    f32 max_temp = 0.0f;
    int sensors_read = 0;
    
    for (const auto& zone : thermal_zones) {
        std::ifstream temp_file(zone);
        if (!temp_file.is_open()) {
            continue;
        }
        
        int temp_millicelsius;
        temp_file >> temp_millicelsius;
        
        if (temp_millicelsius > 0 && temp_millicelsius < 200000) { // Validação básica
            f32 temp_celsius = temp_millicelsius / 1000.0f;
            max_temp = std::max(max_temp, temp_celsius);
            sensors_read++;
        }
    }
    
    if (sensors_read > 0) {
        return max_temp;
    }
    
    // Fallback: Tentar ler do sensor de bateria
    std::ifstream battery_temp("/sys/class/power_supply/battery/temp");
    if (battery_temp.is_open()) {
        int temp_decidegrees;
        battery_temp >> temp_decidegrees;
        return temp_decidegrees / 10.0f;
    }
    
    LOG_WARNING(Frontend, "Could not read temperature from any sensor");
    return 0.0f;
#else
    return 0.0f;
#endif
}

void ThermalMonitor::ApplyThrottling(ThermalLevel level) {
    switch (level) {
        case ThermalLevel::Safe:
            // Tudo normal
            break;
            
        case ThermalLevel::Warning:
            LOG_WARNING(Frontend, "⚠️ Temperature WARNING: {:.1f}°C - Reducing quality", 
                       current_temp.load());
            
            // Reduzir qualidade gradualmente
            Settings::values.resolution_setup = Settings::ResolutionSetup::Res1_2X;
            Settings::values.fps_limit = 25;
            
            if (on_warning) on_warning();
            break;
            
        case ThermalLevel::Hot:
            LOG_ERROR(Frontend, "🔥 Temperature HOT: {:.1f}°C - Aggressive throttling!", 
                     current_temp.load());
            
            // Throttle agressivo
            Settings::values.resolution_setup = Settings::ResolutionSetup::Res1_4X;
            Settings::values.fps_limit = 20;
            Settings::values.use_asynchronous_shaders = false;
            break;
            
        case ThermalLevel::Critical:
            LOG_CRITICAL(Frontend, "🔥🔥 Temperature CRITICAL: {:.1f}°C - MAXIMUM THROTTLE!", 
                        current_temp.load());
            
            // Throttle máximo
            Settings::values.resolution_setup = Settings::ResolutionSetup::Res1_4X;
            Settings::values.fps_limit = 15;
            Settings::values.use_asynchronous_shaders = false;
            
            if (on_critical) on_critical();
            break;
            
        case ThermalLevel::Emergency:
            LOG_CRITICAL(Frontend, "🚨🔥 EMERGENCY: {:.1f}°C - STOPPING EMULATION!", 
                        current_temp.load());
            
            if (on_emergency) {
                on_emergency();
            }
            
            // Parar emulação por segurança
            // O callback deve pausar o jogo
            break;
    }
}

void ThermalMonitor::MonitorThread() {
    LOG_INFO(Frontend, "Thermal monitoring thread started");
    
    int emergency_countdown = 0;
    
    while (running.load()) {
        // Ler temperatura
        f32 temp = ReadTemperatureFromSensor();
        
        if (temp > 0.0f) {
            current_temp = temp;
            
            // Determinar nível térmico
            ThermalLevel new_level = ThermalLevel::Safe;
            
            if (temp >= config.emergency_temp) {
                new_level = ThermalLevel::Emergency;
            } else if (temp >= config.critical_temp) {
                new_level = ThermalLevel::Critical;
            } else if (temp >= config.hot_temp) {
                new_level = ThermalLevel::Hot;
            } else if (temp >= config.warning_temp) {
                new_level = ThermalLevel::Warning;
            }
            
            // Se mudou de nível, aplicar throttling
            ThermalLevel old_level = current_level.exchange(new_level);
            
            if (new_level != old_level) {
                LOG_INFO(Frontend, "Thermal level changed: {} -> {} ({:.1f}°C)", 
                        static_cast<int>(old_level), static_cast<int>(new_level), temp);
                ApplyThrottling(new_level);
            }
            
            // Modo emergência: countdown para desligar
            if (new_level == ThermalLevel::Emergency) {
                emergency_countdown++;
                
                if (emergency_countdown >= EMERGENCY_COOLDOWN_SECONDS / 3) {
                    LOG_CRITICAL(Frontend, "🚨 EMERGENCY SHUTDOWN - Temperature too high for too long!");
                    // Forçar parada
                    if (on_emergency) on_emergency();
                    break;
                }
            } else {
                emergency_countdown = 0;
            }
            
            // Log periódico (a cada 30 segundos)
            static int log_counter = 0;
            if (++log_counter >= 10) {
                LOG_INFO(Frontend, "🌡️ Temp: {:.1f}°C | Level: {} | Device: {}", 
                        temp, static_cast<int>(new_level), static_cast<int>(device_model));
                log_counter = 0;
            }
        }
        
        std::this_thread::sleep_for(CHECK_INTERVAL);
    }
    
    LOG_INFO(Frontend, "Thermal monitoring thread stopped");
}

} // namespace AndroidThermal
