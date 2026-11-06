"""Synthetic ELF tests for guarded base-executable patches; no game bytes shipped."""
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tlou_patches as profiles
from test_tlou_bionic import m


class BaseExecutablePatches(unittest.TestCase):
    def elf(self, path):
        end = max(at + len(original) // 2 for changes in profiles.BASE_PATCHES.values()
                  for at, original, _ in changes)
        raw = bytearray(end + 0x1000)
        struct.pack_into("<Q", raw, 0x20, 0x40)
        struct.pack_into("<HH", raw, 0x36, 56, 1)
        struct.pack_into("<IIQQQQQQ", raw, 0x40, 1, 5, 0x1000, 0, 0, end, end, 0x1000)
        for changes in profiles.BASE_PATCHES.values():
            for at, original, _ in changes:
                raw[at+0x1000:at+0x1000+len(original)//2] = bytes.fromhex(original)
        path.write_bytes(raw)

    def test_base_changes_and_rip_constants(self):
        with tempfile.TemporaryDirectory() as temp:
            elf, out = Path(temp)/"eboot.elf", Path(temp)/"patches.bin"
            self.elf(elf)
            patcher = m.load_patch_module(Path(__file__).resolve().parents[2])
            patcher.image_patches(elf, out, profiles.BASE_PATCHES)
            self.assertEqual(struct.unpack_from("<Q", out.read_bytes(), 16)[0], 9)
            for at, _, replacement in profiles.BASE_PATCHES["frame-timeout"][:2]:
                value = bytes.fromhex(replacement)
                target = at + 8 + struct.unpack_from("<i", value, 4)[0]
                raw = elf.read_bytes()
                fmt = "f" if at == 0x99285A else "d"
                self.assertEqual(struct.unpack_from("<"+fmt, raw, target+0x1000)[0], 20.0)

    def test_crash_trampoline_targets_and_padding_guard(self):
        changes=profiles.BASE_PATCHES['null-component-transforms']
        entries={at:(bytes.fromhex(old),bytes.fromhex(new) if new else None) for at,old,new in changes}
        old,stub=entries[0xf6be63]
        self.assertEqual(old,b'\x90'*13)
        self.assertEqual(0xf6be63+5+struct.unpack('b',stub[4:5])[0],0xf6be62)
        self.assertEqual(0xf6be63+13+struct.unpack('b',stub[-1:])[0],0xf6be76)
        jump=entries[0xf6be70][1]
        self.assertEqual(0xf6be70+5+struct.unpack('<i',jump[1:5])[0],0xf6be63)

    def test_mismatching_worker_guard_rejects_entire_sleep_patch(self):
        with tempfile.TemporaryDirectory() as temp:
            elf, out = Path(temp)/"eboot.elf", Path(temp)/"patches.bin"
            self.elf(elf)
            with elf.open("r+b") as f:
                f.seek(0xAACA40+0x1000)
                f.write(b"\x90")
            patcher = m.load_patch_module(Path(__file__).resolve().parents[2])
            patcher.image_patches(elf, out, profiles.BASE_PATCHES)
            self.assertEqual(struct.unpack_from("<Q", out.read_bytes(), 16)[0], 6)

    def test_low_resolution_game_hud_aim_share_one_coordinate_space(self):
        # All gameplay and HUD constructors use the requested scene size.
        # Preserve the native 1920x1080 video/movie allocation branches.
        for profile, (width, height) in profiles.LOW_RES_SCENE.items():
            baseline = dict((at, (original, replacement))
                            for at, original, replacement in
                            profiles.PERFORMANCE_RESOLUTIONS[profile])
            patches = profiles.scene_aim_resolution_patches(profile)
            self.assertEqual(len(patches), len(baseline))
            changed = {at for at, old, new in patches if new != baseline[at][1]}
            self.assertEqual(changed, {*profiles.GAME_UI_NATIVE_OVERRIDES,
                                       profiles.AIM_NORMALIZATION_ADDRESS})
            for at, original, replacement in patches:
                self.assertEqual(original, baseline[at][0])
                self.assertEqual(len(replacement), len(original))
                if at in profiles.GAME_UI_NATIVE_OVERRIDES:
                    self.assertIn(struct.pack("<I", width).hex(), replacement)
                    self.assertIn(struct.pack("<I", height).hex(), replacement)
                    self.assertNotIn("80070000", replacement)
                    self.assertNotIn("38040000", replacement)
            b = bytes.fromhex(dict((at, new) for at, _, new in patches)[
                profiles.AIM_NORMALIZATION_ADDRESS])
            self.assertAlmostEqual(struct.unpack_from("<f", b, 1)[0], 1/height, places=8)
            self.assertAlmostEqual(struct.unpack_from("<f", b, 10)[0], 1/width, places=8)
            for address in profiles.MOVIE_NATIVE_ALLOCATION_SITES:
                self.assertEqual(dict((at, new) for at, _, new in patches)[address],
                                 baseline[address][1])
            self.assertEqual(profiles.selected_options({"resolution":profile})
                             ["resolution-"+profile], patches)
        self.assertNotIn("resolution-original", profiles.selected_options({"resolution":"original"}))

    def test_low_resolution_rejects_unrecognized_hud_patch(self):
        address = next(iter(profiles.GAME_UI_NATIVE_OVERRIDES))
        original = profiles.GAME_UI_NATIVE_OVERRIDES[address]
        try:
            profiles.GAME_UI_NATIVE_OVERRIDES[address] = "9090909090"
            with self.assertRaises(ValueError):
                profiles.scene_aim_resolution_patches("720")
        finally:
            profiles.GAME_UI_NATIVE_OVERRIDES[address] = original


    def test_job_worker_wait_regression_is_device_scoped(self):
        default=profiles.base_patches_for({})
        conservative=profiles.base_patches_for({"job_sleep":False})
        self.assertIn("job-sleep",default)
        self.assertNotIn("job-sleep",conservative)
        self.assertEqual(set(default)-set(conservative),{"job-sleep"})
        self.assertEqual(profiles.base_patches_for({"job_sleep":True}),default)
        with self.assertRaises(ValueError):
            profiles.base_patches_for({"job_sleep":"false"})

    def test_unknown_executable_keeps_existing_version_guards(self):
        with tempfile.TemporaryDirectory() as temp:
            eboot = Path(temp)/"eboot.bin"
            eboot.write_bytes(b"another executable")
            fallback = {"test": []}
            self.assertIs(profiles.select(eboot, fallback), fallback)


if __name__ == "__main__":
    unittest.main()
