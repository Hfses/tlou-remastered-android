// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/amdgpu/tlou_compute_drain.h"
#include <cassert>
#include <cstdio>
using TlouComputeDrain::PacketKind;
using TlouComputeDrain::SkipPreDrain;
using TlouComputeDrain::UseGuardedWait;

static_assert(!SkipPreDrain(PacketKind::MemoryWait, false, false, false));
static_assert(!SkipPreDrain(PacketKind::NestedBuffer, false, false, false));
static_assert(!SkipPreDrain(PacketKind::Other, true, true, true));
static_assert(SkipPreDrain(PacketKind::MemoryWait, true, false, false));
static_assert(SkipPreDrain(PacketKind::NestedBuffer, true, false, false));
static_assert(!SkipPreDrain(PacketKind::MemoryWait, false, false, true));
static_assert(SkipPreDrain(PacketKind::NestedBuffer, false, false, true));
static_assert(SkipPreDrain(PacketKind::MemoryWait, false, true, false));
static_assert(SkipPreDrain(PacketKind::NestedBuffer, false, true, false));
static_assert(UseGuardedWait(true, false) && !UseGuardedWait(false, false));

int main() {
    for (int fast = 0; fast <= 1; ++fast)
      for (int fewer = 0; fewer <= 1; ++fewer)
        for (int ib = 0; ib <= 1; ++ib) {
            assert(SkipPreDrain(PacketKind::NestedBuffer, fast, fewer, ib) ==
                   bool(fast || fewer || ib));
            assert(SkipPreDrain(PacketKind::MemoryWait, fast, fewer, ib) ==
                   bool(fast || fewer));
            assert(!SkipPreDrain(PacketKind::Other, fast, fewer, ib));
            assert(UseGuardedWait(fast, fewer) == bool(fast || fewer));
        }
    std::puts("TLOU compute-only drain gating: PASS");
}
