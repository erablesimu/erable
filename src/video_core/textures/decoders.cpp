// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <utility>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/bit_util.h"
#include "common/div_ceil.h"
#include "common/logging.h"
#include "common/thread_worker.h"
#include "video_core/gpu.h"
#include "video_core/textures/decoders.h"

namespace Tegra::Texture {
namespace {
constexpr u32 NativeGobOffset(u32 offset, bool native_tiling) {
    if (!native_tiling) return offset;
    const u32 difference = ((offset >> 5) ^ (offset >> 6)) & 1;
    return offset ^ (difference << 5) ^ (difference << 6);
}
template <u32 mask>
constexpr u32 pdep(u32 value) {
    u32 result = 0;
    u32 m = mask;
    for (u32 bit = 1; m; bit += bit) {
        if (value & bit)
            result |= m & (~m + 1);
        m &= m - 1;
    }
    return result;
}

template <u32 mask, u32 incr_amount>
void incrpdep(u32& value) {
    static constexpr u32 swizzled_incr = pdep<mask>(incr_amount);
    value = ((value | ~mask) + swizzled_incr) & mask;
}

// Where each 16-byte unit of a GOB row lies within the GOB, by row (y & 7) and unit (x / 16 & 3).
// The 16 bytes of a unit are contiguous. A GOB's own offset is a multiple of GOB_SIZE, so the
// native tiling's swap of bits 5 and 6 only moves units within it.
std::array<u32, GOB_SIZE_Y * 4> GobUnitOffsets(bool native_tiling) {
    std::array<u32, GOB_SIZE_Y * 4> offsets;
    for (u32 row = 0; row < GOB_SIZE_Y; ++row) {
        for (u32 unit = 0; unit < 4; ++unit) {
            offsets[row * 4 + unit] = NativeGobOffset(
                pdep<SWIZZLE_X_BITS>(unit * 16) | pdep<SWIZZLE_Y_BITS>(row), native_tiling);
        }
    }
    return offsets;
}

// Whole-texture (un)swizzles in bands of block rows on several cores, after Eglantine's
// d470788010 by Takoroka, reverted there (dfe734dce6) over an unexplained hang in one Mario Kart
// World run of eight. On by default; EDEN_NX2_PARALLEL_SWIZZLE=0 turns it off. Differences from
// the original:
// - its own workers, so its waits never mix with the transcode pool's, whose ASTC and BCn
//   conversions wait for that whole pool (ThreadWorker::WaitForRequests) from other threads;
// - no nesting: a band never splits again, and a thread already running bands runs a new
//   swizzle alone;
// - the caller waits on a mutex and condition variable rather than std::atomic::wait, with a
//   one-second watchdog that logs a band still running, and an exception in a band reaches the
//   caller instead of leaving it waiting forever.
bool ParallelSwizzleEnabled() {
    static const bool enabled = [] {
        const char* const value = std::getenv("EDEN_NX2_PARALLEL_SWIZZLE");
        const bool on = value == nullptr || value[0] != '0';
        LOG_INFO(HW_GPU, "Large textures swizzle {}", on ? "in bands" : "on one thread");
        return on;
    }();
    return enabled;
}

Common::ThreadWorker& SwizzleWorkers() {
    static Common::ThreadWorker workers{
        (std::max)(std::thread::hardware_concurrency() / 4, 2U), "ImageSwizzle"};
    return workers;
}

thread_local bool t_running_bands = false;

/// Runs body(0) .. body(count - 1) on the calling thread and on the swizzle workers, and returns
/// once all have run. The caller takes whatever indices no worker has started.
template <typename Body>
void ParallelBands(u32 count, Body&& body) {
    if (count <= 1 || t_running_bands) {
        for (u32 index = 0; index < count; ++index) {
            body(index);
        }
        return;
    }
    struct State {
        std::atomic<u32> next{0};
        std::atomic<u32> done{0};
        u32 count{};
        void* context{};
        void (*call)(void*, u32){};
        std::mutex mutex;
        std::condition_variable finished;
        std::exception_ptr error;
    };
    const auto run = [](State& state) {
        const bool was_running = std::exchange(t_running_bands, true);
        for (u32 index = state.next.fetch_add(1, std::memory_order_relaxed); index < state.count;
             index = state.next.fetch_add(1, std::memory_order_relaxed)) {
            try {
                state.call(state.context, index);
            } catch (...) {
                std::scoped_lock lock{state.mutex};
                if (!state.error) {
                    state.error = std::current_exception();
                }
            }
            if (state.done.fetch_add(1, std::memory_order_acq_rel) + 1 == state.count) {
                std::scoped_lock lock{state.mutex};
                state.finished.notify_all();
            }
        }
        t_running_bands = was_running;
    };
    // Shared: a worker that gets to its task after the caller returned finds no index left.
    const auto state = std::make_shared<State>();
    state->count = count;
    state->context = std::addressof(body);
    state->call = [](void* context, u32 index) {
        (*static_cast<std::remove_reference_t<Body>*>(context))(index);
    };
    Common::ThreadWorker& workers = SwizzleWorkers();
    const u32 helpers = (std::min)(count - 1, 8U);
    for (u32 i = 0; i < helpers; ++i) {
        workers.QueueWork([state, run] { run(*state); });
    }
    run(*state);
    std::unique_lock lock{state->mutex};
    while (state->done.load(std::memory_order_acquire) != count) {
        if (!state->finished.wait_for(lock, std::chrono::seconds{1}, [&state, count] {
                return state->done.load(std::memory_order_acquire) == count;
            })) {
            LOG_WARNING(HW_GPU, "Parallel swizzle: {} of {} bands done after more than a second",
                        state->done.load(std::memory_order_acquire), count);
        }
    }
    if (state->error) {
        std::rethrow_exception(state->error);
    }
}

/// Runs body(line_begin, line_end) over [0, height) in bands of whole block rows (GOB rows times
/// the block height), several at once for large images: each band reads and writes bytes of its
/// own on both the swizzled and the linear side, every slice included.
template <typename Body>
void ForEachLineBand(u64 bytes_per_line, u32 height, u32 depth, u32 block_height, Body&& body) {
    constexpr u64 MinParallelBytes = u64{4} << 20;
    constexpr u64 BandBytes = u64{1} << 20;
    const u64 bytes_per_band_line = (std::max<u64>)(bytes_per_line * depth, 1);
    if (!ParallelSwizzleEnabled() || bytes_per_band_line * height < MinParallelBytes) {
        body(0U, height);
        return;
    }
    const u32 align = 1U << (GOB_SIZE_Y_SHIFT + block_height);
    const u32 rows = static_cast<u32>(
        Common::AlignUp((std::max<u64>)(BandBytes / bytes_per_band_line, 1), align));
    ParallelBands(Common::DivCeil(height, rows), [&](u32 band) {
        body(band * rows, (std::min)(height, (band + 1) * rows));
    });
}

template <bool TO_LINEAR, u32 BYTES_PER_PIXEL>
void SwizzleImpl(std::span<u8> output, std::span<const u8> input, u32 width, u32 height, u32 depth,
                 u32 block_height, u32 block_depth, u32 stride, bool native_tiling,
                 u32 line_begin, u32 line_end) {
    // The origin of the transformation can be configured here, leave it as zero as the current API
    // doesn't expose it.
    static constexpr u32 origin_x = 0;
    static constexpr u32 origin_y = 0;
    static constexpr u32 origin_z = 0;

    // We can configure here a custom pitch
    // As it's not exposed 'width * BYTES_PER_PIXEL' will be the expected pitch.
    const u32 pitch = width * BYTES_PER_PIXEL;

    const u32 gobs_in_x = Common::DivCeilLog2(stride, GOB_SIZE_X_SHIFT);
    const u32 block_size = gobs_in_x << (GOB_SIZE_SHIFT + block_height + block_depth);
    const u32 slice_size =
        Common::DivCeilLog2(height, block_height + GOB_SIZE_Y_SHIFT) * block_size;

    const u32 block_height_mask = (1U << block_height) - 1;
    const u32 block_depth_mask = (1U << block_depth) - 1;
    const u32 x_shift = GOB_SIZE_SHIFT + block_height + block_depth;

    if constexpr (BYTES_PER_PIXEL == 16 && origin_x == 0 && origin_y == 0 && origin_z == 0) {
        // 16-byte units (UnswizzleTexture and SwizzleTexture widen to them when a row allows it):
        // their offsets in a GOB come from a table instead of being computed one by one.
        const auto unit_offsets = GobUnitOffsets(native_tiling);
        const u32 gobs_wide = Common::DivCeil(width, 4U);
        if constexpr (TO_LINEAR) {
            // Swizzling, the writes go to the block linear side: walk it GOB by GOB, in address
            // order, so each 512-byte GOB is filled whole from 8 rows of 64 linear bytes. Row by
            // row, each step wrote 64 bytes to another GOB and a GOB took eight passes far apart.
            // (Unswizzling keeps the row order, which writes the linear side in order.)
            const u32 gobs_high = Common::DivCeilLog2(height, GOB_SIZE_Y_SHIFT);
            const u32 block_gobs = 1U << block_height;
            for (u32 slice = 0; slice < depth; ++slice) {
                const u32 offset_z = (slice >> block_depth) * slice_size +
                                     ((slice & block_depth_mask) << (GOB_SIZE_SHIFT + block_height));
                const u32 slice_linear = slice * pitch * height;
                for (u32 block_y = 0; block_y * block_gobs < gobs_high; ++block_y) {
                    const u32 gob_rows = (std::min)(block_gobs, gobs_high - block_y * block_gobs);
                    for (u32 gob_x = 0; gob_x < gobs_wide; ++gob_x) {
                        const u32 units = (std::min)(4U, width - gob_x * 4);
                        u32 gob_offset = offset_z + block_y * block_size + (gob_x << x_shift);
                        for (u32 gob_y = 0; gob_y < gob_rows; ++gob_y, gob_offset += GOB_SIZE) {
                            const u32 first_line = (block_y * block_gobs + gob_y) * GOB_SIZE_Y;
                            const u32 lines = (std::min)(GOB_SIZE_Y, height - first_line);
                            for (u32 row = 0; row < lines; ++row) {
                                const u32* const row_offsets = &unit_offsets[row * 4];
                                const u8* const src =
                                    &input[slice_linear + (first_line + row) * pitch + gob_x * 64];
                                for (u32 unit = 0; unit < units; ++unit) {
                                    std::memcpy(&output[gob_offset + row_offsets[unit]],
                                                src + unit * 16, 16);
                                }
                            }
                        }
                    }
                }
            }
            return;
        }
        for (u32 slice = 0; slice < depth; ++slice) {
            const u32 offset_z = (slice >> block_depth) * slice_size +
                                 ((slice & block_depth_mask) << (GOB_SIZE_SHIFT + block_height));
            for (u32 line = line_begin; line < line_end; ++line) {
                const u32 block_y = line >> GOB_SIZE_Y_SHIFT;
                const u32 offset_y = (block_y >> block_height) * block_size +
                                     ((block_y & block_height_mask) << GOB_SIZE_SHIFT);
                const u32* const row_offsets = &unit_offsets[(line & (GOB_SIZE_Y - 1)) * 4];
                const u32 row_linear = slice * pitch * height + line * pitch;
                for (u32 gob_x = 0; gob_x < gobs_wide; ++gob_x) {
                    const u32 gob_offset = offset_z + offset_y + (gob_x << x_shift);
                    const u32 units = (std::min)(4U, width - gob_x * 4);
                    for (u32 unit = 0; unit < units; ++unit) {
                        const u32 swizzled_offset = gob_offset + row_offsets[unit];
                        const u32 unswizzled_offset = row_linear + (gob_x * 4 + unit) * 16;
                        u8* const dst = &output[TO_LINEAR ? swizzled_offset : unswizzled_offset];
                        const u8* const src =
                            &input[TO_LINEAR ? unswizzled_offset : swizzled_offset];
                        std::memcpy(dst, src, 16);
                    }
                }
            }
        }
        return;
    }

    for (u32 slice = 0; slice < depth; ++slice) {
        const u32 z = slice + origin_z;
        const u32 offset_z = (z >> block_depth) * slice_size +
                             ((z & block_depth_mask) << (GOB_SIZE_SHIFT + block_height));
        for (u32 line = line_begin; line < line_end; ++line) {
            const u32 y = line + origin_y;
            const u32 swizzled_y = pdep<SWIZZLE_Y_BITS>(y);

            const u32 block_y = y >> GOB_SIZE_Y_SHIFT;
            const u32 offset_y = (block_y >> block_height) * block_size +
                                 ((block_y & block_height_mask) << GOB_SIZE_SHIFT);

            u32 swizzled_x = pdep<SWIZZLE_X_BITS>(origin_x * BYTES_PER_PIXEL);
            for (u32 column = 0; column < width;
                 ++column, incrpdep<SWIZZLE_X_BITS, BYTES_PER_PIXEL>(swizzled_x)) {
                const u32 x = (column + origin_x) * BYTES_PER_PIXEL;
                const u32 offset_x = (x >> GOB_SIZE_X_SHIFT) << x_shift;

                const u32 base_swizzled_offset = offset_z + offset_y + offset_x;
                const u32 swizzled_offset = NativeGobOffset(base_swizzled_offset + (swizzled_x | swizzled_y), native_tiling);

                const u32 unswizzled_offset =
                    slice * pitch * height + line * pitch + column * BYTES_PER_PIXEL;

                u8* const dst = &output[TO_LINEAR ? swizzled_offset : unswizzled_offset];
                const u8* const src = &input[TO_LINEAR ? unswizzled_offset : swizzled_offset];

                std::memcpy(dst, src, BYTES_PER_PIXEL);
            }
        }
    }
}

template <bool TO_LINEAR, u32 BYTES_PER_PIXEL>
void SwizzleSubrectImpl(std::span<u8> output, std::span<const u8> input, u32 width, u32 height,
                        u32 depth, u32 origin_x, u32 origin_y, u32 extent_x, u32 num_lines,
                        u32 block_height, u32 block_depth, u32 pitch_linear, bool native_tiling) {
    // The origin of the transformation can be configured here, leave it as zero as the current API
    // doesn't expose it.
    static constexpr u32 origin_z = 0;

    // We can configure here a custom pitch
    // As it's not exposed 'width * BYTES_PER_PIXEL' will be the expected pitch.
    const u32 pitch = pitch_linear;
    const u32 stride = Common::AlignUpLog2(width * BYTES_PER_PIXEL, GOB_SIZE_X_SHIFT);

    const u32 gobs_in_x = Common::DivCeilLog2(stride, GOB_SIZE_X_SHIFT);
    const u32 block_size = gobs_in_x << (GOB_SIZE_SHIFT + block_height + block_depth);
    const u32 slice_size =
        Common::DivCeilLog2(height, block_height + GOB_SIZE_Y_SHIFT) * block_size;

    const u32 block_height_mask = (1U << block_height) - 1;
    const u32 block_depth_mask = (1U << block_depth) - 1;
    const u32 x_shift = GOB_SIZE_SHIFT + block_height + block_depth;

    u32 unprocessed_lines = num_lines;
    u32 extent_y = (std::min)(num_lines, height - origin_y);
    // Power of two pixels never straddle a 16-byte unit: copy each row in runs of whole units
    // (and the partial ones at its ends) from the unit table.
    constexpr bool unit_runs = std::has_single_bit(BYTES_PER_PIXEL);
    [[maybe_unused]] const auto unit_offsets = GobUnitOffsets(native_tiling);

    for (u32 slice = 0; slice < depth; ++slice) {
        const u32 z = slice + origin_z;
        const u32 offset_z = (z >> block_depth) * slice_size +
                             ((z & block_depth_mask) << (GOB_SIZE_SHIFT + block_height));
        const u32 lines_in_y = (std::min)(unprocessed_lines, extent_y);
        for (u32 line = 0; line < lines_in_y; ++line) {
            const u32 y = line + origin_y;
            const u32 swizzled_y = pdep<SWIZZLE_Y_BITS>(y);

            const u32 block_y = y >> GOB_SIZE_Y_SHIFT;
            const u32 offset_y = (block_y >> block_height) * block_size +
                                 ((block_y & block_height_mask) << GOB_SIZE_SHIFT);

            if constexpr (unit_runs) {
                const u32* const row_offsets = &unit_offsets[(y & (GOB_SIZE_Y - 1)) * 4];
                const u32 x_begin = origin_x * BYTES_PER_PIXEL;
                const u32 x_end = (origin_x + extent_x) * BYTES_PER_PIXEL;
                const u32 row_linear = slice * pitch * height + line * pitch;
                for (u32 x = x_begin; x < x_end;) {
                    const u32 run = (std::min)(16 - (x & 15), x_end - x);
                    const u32 swizzled_offset = offset_z + offset_y +
                                                ((x >> GOB_SIZE_X_SHIFT) << x_shift) +
                                                row_offsets[(x >> 4) & 3] + (x & 15);
                    const u32 unswizzled_offset = row_linear + (x - x_begin);
                    u8* const dst = &output[TO_LINEAR ? swizzled_offset : unswizzled_offset];
                    const u8* const src = &input[TO_LINEAR ? unswizzled_offset : swizzled_offset];
                    std::memcpy(dst, src, run);
                    x += run;
                }
                continue;
            }

            u32 swizzled_x = pdep<SWIZZLE_X_BITS>(origin_x * BYTES_PER_PIXEL);
            for (u32 column = 0; column < extent_x;
                 ++column, incrpdep<SWIZZLE_X_BITS, BYTES_PER_PIXEL>(swizzled_x)) {
                const u32 x = (column + origin_x) * BYTES_PER_PIXEL;
                const u32 offset_x = (x >> GOB_SIZE_X_SHIFT) << x_shift;

                const u32 base_swizzled_offset = offset_z + offset_y + offset_x;
                const u32 swizzled_offset = NativeGobOffset(base_swizzled_offset + (swizzled_x | swizzled_y), native_tiling);

                const u32 unswizzled_offset =
                    slice * pitch * height + line * pitch + column * BYTES_PER_PIXEL;

                u8* const dst = &output[TO_LINEAR ? swizzled_offset : unswizzled_offset];
                const u8* const src = &input[TO_LINEAR ? unswizzled_offset : swizzled_offset];

                std::memcpy(dst, src, BYTES_PER_PIXEL);
            }
        }
        unprocessed_lines -= lines_in_y;
        if (unprocessed_lines == 0) {
            return;
        }
    }
}

template <bool TO_LINEAR>
void Swizzle(std::span<u8> output, std::span<const u8> input, u32 bytes_per_pixel, u32 width,
             u32 height, u32 depth, u32 block_height, u32 block_depth, u32 stride_alignment, bool native_tiling) {
    const auto run = [&](u32 line_begin, u32 line_end) {
        switch (bytes_per_pixel) {
#define BPP_CASE(x)                                                                                \
    case x:                                                                                        \
        return SwizzleImpl<TO_LINEAR, x>(output, input, width, height, depth, block_height,        \
                                         block_depth, stride_alignment, native_tiling, line_begin, \
                                         line_end);
            BPP_CASE(1)
            BPP_CASE(2)
            BPP_CASE(3)
            BPP_CASE(4)
            BPP_CASE(6)
            BPP_CASE(8)
            BPP_CASE(12)
            BPP_CASE(16)
#undef BPP_CASE
        default:
            ASSERT_MSG(false, "Invalid bytes_per_pixel={}", bytes_per_pixel);
            break;
        }
    };
    ForEachLineBand(u64{width} * bytes_per_pixel, height, depth, block_height, run);
}

} // Anonymous namespace

void UnswizzleTexture(std::span<u8> output, std::span<const u8> input, u32 bytes_per_pixel,
                      u32 width, u32 height, u32 depth, u32 block_height, u32 block_depth,
                      u32 stride_alignment, bool native_tiling) {
    const u32 stride = Common::AlignUpLog2(width, stride_alignment) * bytes_per_pixel;
    const u32 new_bpp = (std::min)(4U, static_cast<u32>(std::countr_zero(width * bytes_per_pixel)));
    width = (width * bytes_per_pixel) >> new_bpp;
    bytes_per_pixel = 1U << new_bpp;
    Swizzle<false>(output, input, bytes_per_pixel, width, height, depth, block_height, block_depth,
                   stride, native_tiling);
}

void SwizzleTexture(std::span<u8> output, std::span<const u8> input, u32 bytes_per_pixel, u32 width,
                    u32 height, u32 depth, u32 block_height, u32 block_depth,
                    u32 stride_alignment, bool native_tiling) {
    const u32 stride = Common::AlignUpLog2(width, stride_alignment) * bytes_per_pixel;
    const u32 new_bpp = (std::min)(4U, static_cast<u32>(std::countr_zero(width * bytes_per_pixel)));
    width = (width * bytes_per_pixel) >> new_bpp;
    bytes_per_pixel = 1U << new_bpp;
    Swizzle<true>(output, input, bytes_per_pixel, width, height, depth, block_height, block_depth,
                  stride, native_tiling);
}

void SwizzleSubrect(std::span<u8> output, std::span<const u8> input, u32 bytes_per_pixel, u32 width,
                    u32 height, u32 depth, u32 origin_x, u32 origin_y, u32 extent_x, u32 extent_y,
                    u32 block_height, u32 block_depth, u32 pitch_linear, bool native_tiling) {
    switch (bytes_per_pixel) {
#define BPP_CASE(x)                                                                                \
    case x:                                                                                        \
        return SwizzleSubrectImpl<true, x>(output, input, width, height, depth, origin_x,          \
                                           origin_y, extent_x, extent_y, block_height,             \
                                           block_depth, pitch_linear, native_tiling);
        BPP_CASE(1)
        BPP_CASE(2)
        BPP_CASE(3)
        BPP_CASE(4)
        BPP_CASE(6)
        BPP_CASE(8)
        BPP_CASE(12)
        BPP_CASE(16)
#undef BPP_CASE
    default:
        ASSERT_MSG(false, "Invalid bytes_per_pixel={}", bytes_per_pixel);
        break;
    }
}

void UnswizzleSubrect(std::span<u8> output, std::span<const u8> input, u32 bytes_per_pixel,
                      u32 width, u32 height, u32 depth, u32 origin_x, u32 origin_y, u32 extent_x,
                      u32 extent_y, u32 block_height, u32 block_depth, u32 pitch_linear, bool native_tiling) {
    switch (bytes_per_pixel) {
#define BPP_CASE(x)                                                                                \
    case x:                                                                                        \
        return SwizzleSubrectImpl<false, x>(output, input, width, height, depth, origin_x,         \
                                            origin_y, extent_x, extent_y, block_height,            \
                                            block_depth, pitch_linear, native_tiling);
        BPP_CASE(1)
        BPP_CASE(2)
        BPP_CASE(3)
        BPP_CASE(4)
        BPP_CASE(6)
        BPP_CASE(8)
        BPP_CASE(12)
        BPP_CASE(16)
#undef BPP_CASE
    default:
        ASSERT_MSG(false, "Invalid bytes_per_pixel={}", bytes_per_pixel);
        break;
    }
}

std::size_t CalculateSize(bool tiled, u32 bytes_per_pixel, u32 width, u32 height, u32 depth,
                          u32 block_height, u32 block_depth) {
    if (tiled) {
        const u32 aligned_width = Common::AlignUpLog2(width * bytes_per_pixel, GOB_SIZE_X_SHIFT);
        const u32 aligned_height = Common::AlignUpLog2(height, GOB_SIZE_Y_SHIFT + block_height);
        const u32 aligned_depth = Common::AlignUpLog2(depth, GOB_SIZE_Z_SHIFT + block_depth);
        return aligned_width * aligned_height * aligned_depth;
    } else {
        return width * height * depth * bytes_per_pixel;
    }
}

u64 GetGOBOffset(u32 width, u32 height, u32 dst_x, u32 dst_y, u32 block_height,
                 u32 bytes_per_pixel) {
    auto div_ceil = [](const u32 x, const u32 y) { return ((x + y - 1) / y); };
    const u32 gobs_in_block = 1 << block_height;
    const u32 y_blocks = GOB_SIZE_Y << block_height;
    const u32 x_per_gob = GOB_SIZE_X / bytes_per_pixel;
    const u32 x_blocks = div_ceil(width, x_per_gob);
    const u32 block_size = GOB_SIZE * gobs_in_block;
    const u32 stride = block_size * x_blocks;
    const u32 base = (dst_y / y_blocks) * stride + (dst_x / x_per_gob) * block_size;
    const u32 relative_y = dst_y % y_blocks;
    return base + (relative_y / GOB_SIZE_Y) * GOB_SIZE;
}

} // namespace Tegra::Texture
