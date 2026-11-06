// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

// TLOU Android: bounded shader-independent safety net for GPU-written compute
// outputs, to complement the exact well-known physics-result readbacks.
//
// A buffer qualifies by actual GPU writer/binding metadata, not by shader hash.
// Readbacks are queued at the existing EOP fence; the CPU only waits for its
// read-protected pages when necessary. This is NOT global Precise Readback 2.
// Restrict copies to at most two 4-KiB pages, and reserve at least half of the
// 256 EOP slots for established shader-specific results.
namespace TlouAutoReadback {
constexpr bool Eligible(bool compute, bool gpu_written, bool known_result,
                        std::uint32_t stride, std::uint64_t records,
                        std::uint64_t bytes, std::uint32_t event_slots) noexcept {
    if (!compute || !gpu_written || known_result || event_slots >= 128 ||
        bytes < 256 || bytes > 8192 || stride < 4 || stride > 64 ||
        (stride & 3) != 0 || records == 0)
        return false;
    // The V# descriptor must actually cover the bytes we plan to copy. A
    // format-dependent element size may be larger than its stride; allow
    // that, but never accept a descriptor whose declared range is shorter.
    return records <= UINT64_MAX / stride && records * stride >= bytes;
}

// TLOU CUSA00552 v1.00, Adreno 830 logs Nov 6:
//   producers at slot 5/6, raw DWORD output, stride 4 and 4272/4368/6480
//   bytes. Both hash-identified and not-yet-identified physics results have
//   this *descriptor family*. Use the layout, not an ever-growing shader list.
// Unlike Full, Balanced does NOT queue unrelated 256 B - 8 KiB compute
// outputs and leaves unrelated async readbacks unprotected.
// The known exact shader results stay handled by the existing readback path.
constexpr bool EligibleBalanced(bool compute, bool gpu_written, bool known_result,
                                std::uint32_t slot, std::uint32_t stride,
                                std::uint64_t records, std::uint64_t bytes,
                                std::uint32_t event_slots) noexcept {
    return (slot == 5 || slot == 6) && stride == 4 &&
           bytes == records * 4 && bytes >= 4096 && bytes <= 8192 &&
           Eligible(compute, gpu_written, known_result, stride, records, bytes,
                    event_slots) && event_slots < 64;
}
}
