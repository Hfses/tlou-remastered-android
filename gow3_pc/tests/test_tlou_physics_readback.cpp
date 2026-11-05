// SPDX-License-Identifier: GPL-2.0-or-later
#include "../gpu/shadps4/video_core/renderer_vulkan/tlou_physics_readback.h"
#include <cassert>
using TlouPhysicsReadback::Shape;

static_assert(Shape(6,48,89,4272));
static_assert(Shape(5,48,135,6480));
static_assert(!Shape(5,48,89,4272));    // Wrong shader output slot.
static_assert(!Shape(6,48,135,6480));  // Wrong shader output slot.
static_assert(!Shape(6,52,89,4628));   // Skinning output, not physics.
static_assert(!Shape(6,48,90,4320));   // Not the recorded physics dispatch.
static_assert(!Shape(6,48,89,4271));   // Partial binding must not be classified.
static_assert(!Shape(6,48,89,65536));  // Broad ring must not be read back.
static_assert(!Shape(6,48,0,0));
int main() {
    for(unsigned slot=0;slot<16;++slot) {
        for(unsigned stride=1;stride<=64;++stride) {
            for(unsigned n=1;n<200;++n) {
                const bool expected=(slot==6 && stride==48 && n==89) ||
                                    (slot==5 && stride==48 && n==135);
                assert(Shape(slot,stride,n,static_cast<unsigned long long>(stride)*n)==expected);
            }
        }
    }
}
