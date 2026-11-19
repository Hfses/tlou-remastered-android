"""Regression tests for automatic temporary APK signing; no game assets required."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "android/bionic/restore_signing.sh"


class ApkSigningFallbackTests(unittest.TestCase):
    def test_script_syntax_and_no_legacy_cache_required(self):
        subprocess.run(["bash", "-n", str(SOURCE)], check=True)
        source = SOURCE.read_text()
        self.assertIn("TLOU_APK_SIGNING_MODE", source)
        self.assertIn("keytool -genkeypair", source)
        self.assertIn("temporary-signing.flag", source)

    def _sandbox(self, tmp):
        script = Path(tmp) / "android/bionic/restore_signing.sh"
        script.parent.mkdir(parents=True)
        script.write_bytes(SOURCE.read_bytes())
        return script

    def _run(self, script, mode):
        env = dict(os.environ)
        env.pop("TLOU_SIGNING_JKS_BASE64", None)
        env["TLOU_APK_SIGNING_MODE"] = mode
        return subprocess.run(["bash", str(script)], env=env, text=True,
                              capture_output=True)

    def test_strict_original_refuses_missing_key(self):
        with tempfile.TemporaryDirectory() as temp:
            script = self._sandbox(temp)
            run = self._run(script, "original")
            self.assertEqual(run.returncode, 8, run.stderr)
            self.assertFalse((Path(temp)/"out/android-bionic/experimental-development.jks").exists())

    @unittest.skipUnless(shutil.which("keytool"), "JDK keytool is not available")
    def test_auto_generates_installable_temporary_key_and_cannot_be_strict(self):
        with tempfile.TemporaryDirectory() as temp:
            script = self._sandbox(temp)
            first = self._run(script, "auto")
            self.assertEqual(first.returncode, 0, first.stderr)
            key = Path(temp)/"out/android-bionic/experimental-development.jks"
            marker = Path(temp)/"out/android-bionic/temporary-signing.flag"
            self.assertGreater(key.stat().st_size, 0)
            self.assertTrue(marker.is_file())
            check = subprocess.run(["keytool", "-list", "-keystore", str(key),
                                    "-alias", "tlou-bionic",
                                    "-storepass", "TLOU-Bionic-development"],
                                   capture_output=True, text=True)
            self.assertEqual(check.returncode, 0, check.stderr)
            second = self._run(script, "auto")
            self.assertEqual(second.returncode, 0, second.stderr)
            third = self._run(script, "original")
            self.assertEqual(third.returncode, 8, third.stderr)
            self.assertIn("temporary", third.stderr)


if __name__ == "__main__":
    unittest.main()
