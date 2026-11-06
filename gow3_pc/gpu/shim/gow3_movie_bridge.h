// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// C ABI between the Android TLOU probe's real libSceVideodec implementation
// and the Vulkan texture/shader movie remapper in libgow3gpu.so.
// Only enabled by GOW3_MOVIE_SIZE=WxH for reduced-resolution gameplay.
// Decoder input and original 1080p guest NV12 frames are never modified.
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Returns 1 iff a resized NV12 scratch frame was generated for the supplied
// decoded 1080p source. Otherwise guest video continues using its native data.
// src_uv is the INTERLEAVED UV plane, not separate U and V planes.
int gow3_movie_scale_nv12(uint64_t guest_base,
                          const uint8_t* src_y, const uint8_t* src_uv,
                          uint32_t src_width, uint32_t src_height,
                          int src_y_stride, int src_uv_stride,
                          uint32_t* display_width, uint32_t* display_height);
#ifdef __cplusplus
}
#endif
