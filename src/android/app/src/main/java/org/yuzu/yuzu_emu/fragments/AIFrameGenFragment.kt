// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.FragmentSettingsBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter
import org.yuzu.yuzu_emu.features.settings.ui.SettingsViewModel
import org.yuzu.yuzu_emu.utils.NativeConfig

class AIFrameGenFragment : Fragment() {
    private lateinit var binding: FragmentSettingsBinding
    private val settingsViewModel: SettingsViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        binding = FragmentSettingsBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val settingsList = ArrayList<SettingsItem>()

        // Header
        settingsList.add(HeaderSetting(R.string.ai_frame_gen_title))

        // Enable AI Frame Generator
        val aiFrameGenEnable = NativeConfig.getBoolean("ai_frame_gen_enable", false)
        settingsList.add(
            SwitchSetting(
                NativeConfig.getBooleanSetting("ai_frame_gen_enable"),
                R.string.ai_frame_gen_enable,
                R.string.ai_frame_gen_enable_description
            )
        )

        // AI Frame Gen Mode
        settingsList.add(
            SingleChoiceSetting(
                NativeConfig.getIntSetting("ai_frame_gen_mode"),
                R.string.ai_frame_gen_mode,
                R.string.ai_frame_gen_mode_description,
                R.array.ai_frame_gen_modes,
                R.array.ai_frame_gen_modes_values
            )
        )

        // Target FPS
        settingsList.add(
            SliderSetting(
                NativeConfig.getIntSetting("ai_frame_gen_target_fps"),
                R.string.ai_frame_gen_target_fps,
                R.string.ai_frame_gen_target_fps_description,
                30,
                120,
                " FPS"
            )
        )

        // Memory Limit
        settingsList.add(
            SliderSetting(
                NativeConfig.getIntSetting("ai_frame_gen_memory_limit"),
                R.string.ai_frame_gen_memory_limit,
                R.string.ai_frame_gen_memory_limit_description,
                256,
                1024,
                " MB"
            )
        )

        // NEON Optimizations
        settingsList.add(
            SwitchSetting(
                NativeConfig.getBooleanSetting("ai_frame_gen_use_neon"),
                R.string.ai_frame_gen_use_neon,
                R.string.ai_frame_gen_use_neon_description
            )
        )

        // CPU Info section
        settingsList.add(HeaderSetting(R.string.ai_frame_gen_stats))
        
        // Add CPU detection info (these would be populated from native code)
        settingsList.add(
            RunnableSetting(
                R.string.ai_frame_gen_cpu_detected,
                R.string.ai_frame_gen_description,
                false
            ) {
                // Show CPU details dialog
                showCPUInfoDialog()
            }
        )

        val adapter = SettingsAdapter(this, requireContext())
        adapter.submitList(settingsList)
        
        binding.listSettings.apply {
            this.adapter = adapter
            layoutManager = LinearLayoutManager(requireContext())
        }

        setInsets()
    }

    private fun showCPUInfoDialog() {
        // TODO: Create dialog showing detected CPU info from native
        // This would call into native code to get CPU detection results
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.listSettings) { view, windowInsets ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.updatePadding(bottom = insets.bottom)
            windowInsets
        }
    }
}
