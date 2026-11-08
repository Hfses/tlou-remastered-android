#!/usr/bin/env python3
"""Guard version-scoped one-frame guest/GPU pipelining against future regressions."""
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
GAME = ROOT / "android-launcher/app/src/main/java/ro/cobrabm/fexdroid/GameActivity.java"
SETTINGS = ROOT / "android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java"
LIVERPOOL = ROOT / "gow3_pc/gpu/shadps4/video_core/amdgpu/liverpool.cpp"
LIVERPOOL_H = ROOT / "gow3_pc/gpu/shadps4/video_core/amdgpu/liverpool.h"
GNM = ROOT / "gow3_pc/gpu/shadps4/core/libraries/gnmdriver/gnmdriver.cpp"

class FrameInFlight111(unittest.TestCase):
    def test_only_update_111_enables_frame_pipelining(self):
        game = GAME.read_text(encoding="utf-8")
        settings = SETTINGS.read_text(encoding="utf-8")
        self.assertEqual(game.count('env.put("GOW3_FRAMES_IN_FLIGHT","1");'), 1)
        guarded = game.index('if("01.11".equals(gameVersion)){')
        enabled = game.index('env.put("GOW3_FRAMES_IN_FLIGHT","1");')
        self.assertLess(guarded, enabled)
        self.assertIn('env.put("GOW3_DMEM_REPORT_MB","4608");', game[enabled:enabled + 1500])
        self.assertNotIn('env.put("GOW3_FRAMES_IN_FLIGHT"', settings)
        self.assertIn('env.getOrDefault("GOW3_FRAMES_IN_FLIGHT","0")', settings)
        self.assertIn('env.put("GOW3_PREP_WORKERS","0");', settings)
        self.assertIn('env.put("GOW3_EOP_READBACK","1");', settings)

    def test_native_frame_retirement_preserves_fence_order_and_deadlock_recovery(self):
        liv = LIVERPOOL.read_text(encoding="utf-8")
        hdr = LIVERPOOL_H.read_text(encoding="utf-8")
        gnm = GNM.read_text(encoding="utf-8")
        self.assertIn('const char* env = std::getenv("GOW3_FRAMES_IN_FLIGHT");', hdr)
        self.assertIn("if (!env || (copy && copy[0] == '1'))", hdr)
        self.assertIn('rasterizer.SignalAfterHostCopies(', liv)
        self.assertIn('retire.liverpool->MarkFrameRetired(retire.frame);', liv)
        self.assertIn('std::chrono::milliseconds{500}', liv)
        self.assertIn('liverpool->WaitFramesRetired(frames_submitted - in_flight)', gnm)
        self.assertNotIn('GOW3_SUBMIT_NO_WAIT', GAME.read_text(encoding="utf-8"))

if __name__ == "__main__":
    unittest.main()
