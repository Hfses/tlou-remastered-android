"""Host validation for conservative deferred Vulkan buffer-copy merging."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "gow3_pc/gpu/shadps4/video_core/renderer_vulkan/vk_copy_batch.h"

SOURCE = r"""
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <span>
#include "vk_copy_batch.h"
struct Region { std::uint64_t srcOffset, dstOffset, size; };
int main() {
    using Vulkan::CopyBatch::DestinationsDisjoint;
    std::array<Region, 3> old{{{0, 100, 20}, {32, 160, 32}, {64, 300, 80}}};
    std::span<const Region> prefix(old.data(), 2);
    assert(DestinationsDisjoint(prefix, Region{96, 120, 20}));
    assert(DestinationsDisjoint(prefix, Region{96, 140, 20}));
    assert(DestinationsDisjoint(prefix, Region{96, 192, 20}));
    assert(!DestinationsDisjoint(prefix, Region{96, 119, 20}));
    assert(!DestinationsDisjoint(prefix, Region{96, 170, 5}));
    assert(!DestinationsDisjoint(prefix, Region{96, 200, 0}));
    assert(!DestinationsDisjoint(prefix, Region{96, std::numeric_limits<std::uint64_t>::max() - 1, 4}));
    assert(!DestinationsDisjoint(std::span<const Region>(old), Region{96, 320, 20}));
    assert(DestinationsDisjoint(std::span<const Region>{}, Region{96, 0, 20}));
}
"""

class CopyBatchSafety(unittest.TestCase):
    def test_disjoint_destinations_and_overflow(self):
        compiler = shutil.which("c++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        self.assertTrue(HEADER.exists())
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / "copy_batch.cpp"
            binary = Path(tmp) / "copy_batch"
            source.write_text(SOURCE)
            subprocess.run([compiler, "-std=c++20", "-O2", "-Wall", "-Wextra",
                            "-Werror", "-I", str(HEADER.parent), str(source),
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    unittest.main()
