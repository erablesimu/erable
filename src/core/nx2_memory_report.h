// SPDX-FileCopyrightText: Copyright 2026 Erable
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdlib>

#include "common/logging.h"
#include "common/memory_detect.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/hle/kernel/k_memory_manager.h"
#include "core/hle/kernel/kernel.h"

namespace Core::Nx2MemoryReport {

/// EDEN_NX2_MEM_LOG: whether the report below is written.
[[nodiscard]] inline bool Enabled() {
    static const bool enabled = std::getenv("EDEN_NX2_MEM_LOG") != nullptr;
    return enabled;
}

/// How much of the emulated RAM the guest has taken from its pools, next to what the host has
/// committed for it up front (all of it, see HostMemory) and the commit left on the system. The
/// gap between what the guest uses and the backing is what committing it lazily gives back.
inline void Log(Core::System& system) {
    if (!Enabled()) {
        return;
    }
    using Pool = Kernel::KMemoryManager::Pool;
    auto& manager = system.Kernel().MemoryManager();
    constexpr std::array<Pool, 4> pools{Pool::Application, Pool::Applet, Pool::System,
                                        Pool::SystemNonSecure};
    constexpr std::array<const char*, 4> names{"application", "applet", "system", "system unsafe"};
    u64 used_total = 0;
    std::array<u64, 4> used{};
    std::array<u64, 4> size{};
    for (size_t i = 0; i < pools.size(); ++i) {
        size[i] = manager.GetSize(pools[i]);
        used[i] = size[i] - (std::min)(size[i], manager.GetFreeSize(pools[i]));
        used_total += used[i];
    }
    static u64 peak = 0;
    peak = (std::max)(peak, used_total);
    const u64 backing = system.DeviceMemory().buffer.BackingSize();
    const u64 committed = system.DeviceMemory().buffer.CommittedBytes();
    const auto process = Common::GetProcessMemoryUsage();
    LOG_INFO(Kernel,
             "Nx2Memory guest_used_mib={} guest_peak_mib={} backing_mib={} backing_committed_mib={} "
             "pools={}:{}/{},{}:{}/{},{}:{}/{},{}:{}/{} process_private_mib={} "
             "working_set_mib={} system_commit_mib={} system_limit_mib={}",
             used_total >> 20, peak >> 20, backing >> 20, committed >> 20, names[0], used[0] >> 20, size[0] >> 20,
             names[1], used[1] >> 20, size[1] >> 20, names[2], used[2] >> 20, size[2] >> 20,
             names[3], used[3] >> 20, size[3] >> 20, process.commit >> 20,
             process.working_set >> 20, process.system_commit >> 20,
             process.system_commit_limit >> 20);
}

} // namespace Core::Nx2MemoryReport
