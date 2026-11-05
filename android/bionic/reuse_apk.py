#!/usr/bin/env python3
"""Reuse only the native/installer payload of a pinned, successfully verified APK."""
from pathlib import Path, PurePosixPath
import hashlib
import subprocess
import sys
import zipfile

BASE = "bbad8cb42cea3fd79a995c89b7a9004b8ca75821"
APK_SHA = "9b240d856718eaf13c12e3742e80018afa9a74a6897b1498390276e9a1f4dac0"
ROOT = Path(__file__).resolve().parents[2]
PACKAGING_ONLY = {"android/bionic/reuse_apk.py", "android/tests/test_launcher_reuse.py"}


def unchanged():
    paths = subprocess.check_output([
        "git", "diff", "--name-only", BASE, "HEAD", "--", "gow3_pc", "ci",
        "android", "tools", ".gitmodules",
        "android-launcher/app/src/main/java/ro/cobrabm/fexdroid/Gow3Present.java"], cwd=ROOT, text=True).splitlines()
    return not (set(paths) - PACKAGING_ONLY)


def payload(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError("Unsafe APK path")
    if name.startswith("lib/arm64-v8a/"):
        return Path("apk-jni") / Path(name).relative_to("lib")
    if name == "assets/tools.zip" or name.startswith(("assets/runtime/", "assets/drivers/")):
        return Path("apk-assets") / Path(name).relative_to("assets")
    return None


def stage(apk):
    if not unchanged():
        raise ValueError("Native or installer source changed: full build required")
    if hashlib.sha256(apk.read_bytes()).hexdigest() != APK_SHA:
        raise ValueError("APK does not match pinned successful source build")
    with zipfile.ZipFile(apk) as z:
        for item in z.infolist():
            target = payload(item.filename)
            if target is None or item.is_dir():
                continue
            dest = ROOT / "out/android-bionic" / target
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(z.read(item))
    print("Verified native runtime, installer and drivers staged; UI assets come from current sources")


if __name__ == "__main__":
    if sys.argv[1:] == ["--check"]:
        print(str(unchanged()).lower())
    else:
        stage(Path(sys.argv[1]))
