"""Host verification of SotC PM4-NOP fast path classification (TLOU Android)."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "gow3_pc/gpu/shadps4/video_core/amdgpu/tlou_cp_nop_fast.h"
SOURCE = r"""
#include <cassert>
#include "tlou_cp_nop_fast.h"
int main() {
    using AmdGpu::TLOU::InertNop;
    assert(InertNop(true, false, false, false));  // Empty NOP
    assert(InertNop(true, false, true, false));   // Arbitrary payload, ignored
    assert(!InertNop(true, false, true, true));   // Flip IRQ, must execute
    assert(!InertNop(true, true, false, false));  // Marker-capture mode
    assert(!InertNop(true, true, true, false));
    assert(!InertNop(false, false, true, false)); // Truncated command
}
"""

class CpNopFast(unittest.TestCase):
    def test_guard_preserves_flip_and_markers(self):
        cxx = shutil.which("c++")
        if cxx is None:
            self.skipTest("No C++ compiler")
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "cp_nop.cpp"
            binary = Path(directory) / "cp_nop"
            source.write_text(SOURCE)
            subprocess.run([cxx, "-std=c++20", "-O2", "-Wall", "-Wextra",
                            "-Werror", "-I", str(HEADER.parent), str(source),
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == "__main__":
    unittest.main()
