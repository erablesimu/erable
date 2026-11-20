// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <type_traits>

#include "common/common_types.h"
#include "common/settings.h"

namespace Settings {

/// What "Use recommended settings" knows of the PC it runs on (the Vulkan device in use)
struct HostHardware {
    u32 gpu_vendor_id{};    ///< 0x10DE NVIDIA, 0x1002 AMD, 0x8086 Intel
    u64 vram_bytes{};       ///< The largest device-local heap
    bool integrated{};      ///< Shares system memory (Radeon 780M, Intel Arc iGPUs)
    bool old_amd{};         ///< AMD GCN4 or older (RX 400/500 series), no float16
    bool dlss{};            ///< GeForce RTX: NVIDIA DLSS can replace a game's own
    bool fsr4{};            ///< AMD FSR 4 capable (RDNA 4)
    u32 cpu_threads{};
    u64 ram_bytes{};
};

enum class HardwareTier : u8 {
    Low,  ///< Integrated GPUs, GCN4 and older, under 6 GiB of VRAM
    Mid,  ///< Discrete GPUs from 6 GiB (RTX 3060, RX 6600)
    High, ///< 10 GiB of VRAM or more, 24 GiB of RAM and 12 CPU threads (RTX 4070, RX 7800 XT)
};

constexpr HardwareTier ClassifyHardware(const HostHardware& hw) {
    constexpr u64 GiB = u64{1} << 30;
    if (hw.integrated || hw.old_amd || hw.vram_bytes < 6 * GiB) {
        return HardwareTier::Low;
    }
    if (hw.vram_bytes >= 10 * GiB && hw.ram_bytes >= 24 * GiB && hw.cpu_threads >= 12) {
        return HardwareTier::High;
    }
    return HardwareTier::Mid;
}

/// The recommended settings that depend on the PC, from players' sessions:
/// - Resolution 1x everywhere. Switch 2 games upscale themselves (their DLSS, replaced by NVIDIA
///   DLSS or FSR), and Ocarina of Time at 2x with DLSS on an RTX 4070 SUPER (32 GiB of RAM)
///   filled Windows' commit: the texture cache was held to 5.4 of its 11 GiB and recreated
///   12,000 images in 20 minutes, a hitch every 2-3 s.
/// - Docked mode (1080p) on discrete GPUs: an RX 570 runs Pokémon Sword at full speed docked,
///   and handheld's 720p stretched to the window was the "blurry" complaint.
/// - NVIDIA DLSS on GeForce RTX, with no anti-aliasing on top (DLSS anti-aliases); FSR 4 on RDNA 4.
/// - SMAA and 16x anisotropic filtering from the middle tier; FXAA and the automatic filtering
///   on low-end GPUs, where both cost.
/// - Mario Kart World's 60 FPS mode (falling back to 30 when slow) on high-end PCs: an
///   i5-14400F with an RTX 4070 SUPER ran races at 50-55 FPS, the CPU the limit.
/// Applied after the general table, which they override (Mario Kart World's FPS modes).
template <typename Func>
void ForEachHardwareSetting(Values& settings, const HostHardware& hw, Func&& func) {
    const auto entry = [&func]<typename Type, bool ranged>(SwitchableSetting<Type, ranged>& setting,
                                                           std::type_identity_t<Type> value) {
        func(setting, value);
    };
    const HardwareTier tier = ClassifyHardware(hw);
    const bool low = tier == HardwareTier::Low;

    entry(settings.resolution_setup, ResolutionSetup::Res1X);
    entry(settings.use_docked_mode,
          hw.integrated && low ? ConsoleMode::Handheld : ConsoleMode::Docked);
    entry(settings.scaling_filter, low ? ScalingFilter::Bilinear : ScalingFilter::Fsr);
    entry(settings.nvidia_dlss, hw.dlss);
    entry(settings.amd_fsr4, hw.fsr4);
    entry(settings.anti_aliasing, hw.dlss ? AntiAliasing::None
                                  : low   ? AntiAliasing::Fxaa
                                          : AntiAliasing::Smaa);
    entry(settings.max_anisotropy, low ? AnisotropyMode::Automatic : AnisotropyMode::X16);
    entry(settings.mkw_60fps, tier == HardwareTier::High);
}

} // namespace Settings
