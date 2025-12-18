// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "thermal_protection.h"
#include <string>
#include <vector>

namespace SafeTesting {

/**
 * Framework de testes seguro para evitar danos ao hardware
 * 
 * PROTEÇÕES:
 * 1. Monitoramento térmico contínuo
 * 2. Limites de tempo de teste
 * 3. Cooldown obrigatório entre testes
 * 4. Validação de configurações antes de aplicar
 * 5. Rollback automático em caso de problema
 */

enum class TestPhase {
    Idle,
    Preparing,
    Running,
    Cooldown,
    Completed,
    Aborted,
};

struct TestConfig {
    std::string name;
    u32 max_duration_seconds = 300;        // 5 minutos máximo por padrão
    u32 cooldown_seconds = 60;             // 1 minuto de cooldown
    f32 temp_limit = 50.0f;                // Limite de temperatura
    bool enable_thermal_protection = true;  // Sempre ativo!
    
    // Configurações do teste
    struct {
        bool enable_frame_gen = false;
        bool enable_async_shaders = false;
        bool enable_cpu_pinning = false;
        bool enable_adaptive_resolution = false;
        u32 resolution_scale_percent = 100;
        u32 fps_limit = 30;
    } features;
};

struct TestResult {
    bool success = false;
    std::string error_message;
    
    // Métricas de performance
    f32 avg_fps = 0.0f;
    f32 min_fps = 0.0f;
    f32 max_fps = 0.0f;
    
    // Métricas térmicas
    f32 initial_temp = 0.0f;
    f32 max_temp = 0.0f;
    f32 final_temp = 0.0f;
    
    // Métricas de memória
    u32 initial_ram_mb = 0;
    u32 peak_ram_mb = 0;
    u32 final_ram_mb = 0;
    
    // Estabilidade
    u32 crashes = 0;
    u32 throttle_events = 0;
    
    std::string GetSummary() const;
};

class SafeTestRunner {
public:
    SafeTestRunner();
    ~SafeTestRunner();
    
    // Configuração inicial obrigatória
    bool Initialize(AndroidThermal::DeviceModel device);
    void Shutdown();
    
    // Executar teste
    TestResult RunTest(const TestConfig& config);
    
    // Abortar teste em execução
    void AbortTest();
    
    // Estado atual
    TestPhase GetCurrentPhase() const { return current_phase; }
    f32 GetCurrentTemperature() const;
    
    // Validação de segurança
    static bool ValidateConfig(const TestConfig& config, std::string& error);
    
private:
    void PrepareTest(const TestConfig& config);
    void ExecuteTest(const TestConfig& config, TestResult& result);
    void PerformCooldown(u32 duration_seconds);
    void CollectMetrics(TestResult& result);
    void SaveBackupSettings();
    void RestoreBackupSettings();
    
    std::unique_ptr<AndroidThermal::ThermalMonitor> thermal_monitor;
    TestPhase current_phase = TestPhase::Idle;
    AndroidThermal::DeviceModel device_model = AndroidThermal::DeviceModel::Unknown;
    
    bool test_running = false;
    bool abort_requested = false;
    
    // Backup de configurações
    struct SettingsBackup {
        u32 resolution_setup;
        u32 fps_limit;
        bool use_async_shaders;
        bool frame_interpolation;
    } backup_settings;
};

/**
 * Testes pré-configurados seguros para cada dispositivo
 */
namespace SafePresets {
    
    // MATE 9: Testes MUITO conservadores
    TestConfig GetMate9SafeTest();
    TestConfig GetMate9ModerateTest();
    
    // A14 5G: Pode ser mais agressivo
    TestConfig GetA14SafeTest();
    TestConfig GetA14ModerateTest();
    TestConfig GetA14AggressiveTest();
    
    // Teste de stress térmico (monitorado)
    TestConfig GetThermalStressTest(AndroidThermal::DeviceModel device);
    
} // namespace SafePresets

} // namespace SafeTesting
