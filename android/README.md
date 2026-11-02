# The Last of Us Remastered — Android/Bionic

**Status: stage 1 (game preparation). No Android APK or gameplay has been validated.**

This port must preserve the TLOU-specific runtime and Vulkan changes already in the repository. We use the other Android projects for portable infrastructure, not as a substitute for TLOU graphics.

## Integration sources

| Existing port | What to reuse | Status |
|---|---|---|
| erickmendes22/gow3_android_daedalus_fork, branch android | ARM64 FEX ABI, Android/Bionic build, native renderer bridge, driver selection, PKG, launcher | Native adaptation pending |
| erickmendes22/uncharted-android | Naughty Dog engine compatibility, worker scheduling, ARM64 graphics and input patterns | Source review pending |
| erickmendes22/iss_android | PS4 imports, controller integration and game-specific frontend patterns | Source review pending |
| This repository | CUSA00552 prepared image, verified patch definitions, TLOU graphics and runtime fixes | Stage 1 implemented |

## Implemented

android/tlou_bionic.py is a Python 3 standard-library game preparation tool. It checks TITLE_ID=CUSA00552 in both base and update, makes a read-only overlay in app-private storage, uses this repository's own prepare.py, link_modules.py and content_profile.py, and writes the boot-linked.bin and content.bin files. It invokes the existing byte-signature-checked G3PATCH2 generator and never applies a patch for an unknown executable version. The optional flashlight patch needs --flashlight.

Example using a legally dumped game:

    python3 android/tlou_bionic.py --source . \
      --game /path/to/CUSA00552 \
      --update /path/to/CUSA00552-patch \
      --out /path/to/private-preparation

The --out directory must support symlinks (app-private Android storage). Input game folders are not modified. If an update has dvdroot_ps4 content, this tool refuses it until native app0 content overlay support is ready, rather than running a mixed game/update.

## Native boot blockers (do not conceal)

1. The TLOU loader in gow3_pc/src/probe.c is currently x86-64-only, with direct guest calls and low-image/red-zone handling. The Bionic GOW3 loader uses guest_cpu.h and FEX, but it lacks the TLOU-specific options --low-image, --no-game-hooks, --guest-path, --app0-overlay and --red-zone. Those must be reconciled before a legitimate ARM64 boot.
2. The renderer under gow3_pc/gpu/ is TLOU-specific and needs Android Vulkan surface/presentation, driver loading, readback, shader and video-decoding integration. Replacing it wholesale with the GOW3 renderer would regress TLOU fixes.
3. Android activity, runtime packaging, native ELF link checks and signed APK creation are still pending.

**Never publish an Android APK that is merely a rebranded GOW3 binary.** The first useful APK milestone needs the actual TLOU ARM64 loader/renderer to compile and pass native-link validation.

## No-game tests

    python3 -m unittest discover -s android/tests -v
    python3 -m compileall -q android

Tests create tiny synthetic SFO files and do not require or include copyrighted game files.
