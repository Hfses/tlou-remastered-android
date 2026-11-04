"""Host compile/run tests for the conservative SotC GENERAL fence-epoch port."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "gow3_pc/gpu/shadps4/video_core/texture_cache/general_epoch_plan.h"
SOURCE = r"""
#include <cassert>
#include <cstdint>
#include <limits>
#include "general_epoch_plan.h"

int main() {
    using VideoCore::GeneralEpoch::ShouldSkip;
    std::uint64_t epoch = std::numeric_limits<std::uint64_t>::max();
    // First same-GENERAL write always emits, then duplicates can be elided.
    assert(!ShouldSkip(true, true, true, true, true, true, 10, epoch));
    assert(epoch == 10);
    assert(ShouldSkip(true, true, true, true, true, true, 10, epoch));
    assert(!ShouldSkip(true, true, true, true, true, true, 11, epoch));
    assert(ShouldSkip(true, true, true, true, true, true, 11, epoch));
    // Read-only, partial, distinct stage, access or layout re-arm the barrier.
    assert(!ShouldSkip(true, true, true, true, true, false, 11, epoch));
    assert(!ShouldSkip(true, true, true, true, true, true, 11, epoch));
    assert(!ShouldSkip(true, false, true, true, true, true, 11, epoch));
    assert(!ShouldSkip(true, true, true, true, true, true, 11, epoch));
    assert(!ShouldSkip(true, true, true, true, false, true, 11, epoch));
    assert(!ShouldSkip(true, true, true, true, true, true, 11, epoch));
    assert(!ShouldSkip(true, true, true, false, true, true, 11, epoch));
    assert(!ShouldSkip(true, true, true, true, true, true, 11, epoch));
    assert(!ShouldSkip(true, true, false, true, true, true, 11, epoch));
    assert(!ShouldSkip(true, true, true, true, true, true, 11, epoch));
    // A new backing starts unsynchronized, and the switch OFF never skips.
    std::uint64_t new_backing = std::numeric_limits<std::uint64_t>::max();
    assert(!ShouldSkip(true, true, true, true, true, true, 11, new_backing));
    assert(!ShouldSkip(false, true, true, true, true, true, 11, new_backing));
    assert(!ShouldSkip(true, true, true, true, true, true, 11, new_backing));
}
"""

class GeneralEpochGuard(unittest.TestCase):
    def test_epoch_fence_partial_stage_and_access_invalidation(self):
        compiler = shutil.which("c++")
        if compiler is None:
            self.skipTest("C++ compiler unavailable")
        self.assertTrue(HEADER.exists())
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder) / "general_epoch.cpp"
            exe = Path(folder) / "general_epoch"
            source.write_text(SOURCE)
            subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra",
                            "-Werror", "-I", str(HEADER.parent), str(source),
                            "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

if __name__ == "__main__":
    unittest.main()
