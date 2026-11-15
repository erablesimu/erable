// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>

#include "common/common_types.h"
#include "common/scratch_buffer.h"
#include "common/thread_worker.h"

namespace Tegra::Texture {

inline constexpr size_t ParallelUnswizzleMinBytes = 2 * 1024 * 1024;

class UnswizzleWorker {
public:
    bool TryUnswizzle(std::span<u8> output, std::span<const u8> input, u32 bytes_per_pixel,
                      u32 width, u32 height, u32 depth, u32 block_height, u32 block_depth,
                      u32 stride_alignment, bool native_tiling, bool verify,
                      Common::ThreadWorker* workers = nullptr);

    /// The caller's input is a copy of its own (not guest memory the emulated CPU may write
    /// meanwhile): the threads read it directly instead of from a snapshot. Reset after each use.
    void SetInputPrivate(bool input_private_) noexcept {
        input_private = input_private_;
    }

private:
    Common::ScratchBuffer<u8> owned;
    bool input_private = false;
};

} // namespace Tegra::Texture
