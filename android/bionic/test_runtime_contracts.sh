#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
python3 gow3_pc/tests/test_draw_workers.py
python3 gow3_pc/tests/test_android_save_choice.py
cc -std=c11 -O2 -Wall -Wextra -Werror -pthread -DGOW3_ANDROID_CHOICES_TEST \
  -I gow3_pc -I gow3_pc/src gow3_pc/tests/test_services.c -lm -o "$test_dir/android-save-services"
"$test_dir/android-save-services"
# Exercise real AppContent dispatch, including regional nonzero service labels.
cc -std=c11 -D_GNU_SOURCE -O2 -Wall -Wextra -Werror -I gow3_pc -I gow3_pc/src \
  gow3_pc/tests/test_content.c gow3_pc/src/runtime_content.c -o "$test_dir/content"
"$test_dir/content"
"$test_dir/content" --unknown
if "$test_dir/content" --missing; then
  echo 'ERROR: missing AppContent profile was accepted' >&2; exit 1
else
  test "$?" -eq 21
fi
for contract in tsc_clock fex_clock services; do
  cc -std=c11 -O2 -Wall -Wextra -Werror -pthread -I gow3_pc -I gow3_pc/src \
    "gow3_pc/tests/test_${contract}.c" -lm -o "$test_dir/$contract"
  "$test_dir/$contract"
done
g++ -std=c++17 -O2 -Wall -Wextra -Werror -pthread -I gow3_pc -I gow3_pc/src \
  gow3_pc/tests/test_fiber_bionic.cpp -o "$test_dir/fibers"
"$test_dir/fibers"

g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_image_copy_plan.cpp -o "$test_dir/image-copies"
"$test_dir/image-copies"

g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread gow3_pc/tests/test_cache_io_queue.cpp -o "$test_dir/cache-io"
"$test_dir/cache-io"

cc -std=c11 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_crash_guest_dump.c -o "$test_dir/crash-dump"
"$test_dir/crash-dump"

cc -std=c11 -O2 -Wall -Wextra -Werror -pthread gow3_pc/tests/test_android_cpu_policy.c -o "$test_dir/cpu-policy"
"$test_dir/cpu-policy"

g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_mapped_readback.cpp -o "$test_dir/mapped-readback"
"$test_dir/mapped-readback"

g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_tlou_compute_split.cpp -o "$test_dir/tlou-compute-split"

g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_tlou_inline_copy_record.cpp -o "$test_dir/tlou-inline-copy"

"$test_dir/tlou-inline-copy"
"$test_dir/tlou-compute-split"

# Producer-tick and late CPU-write safety gates for temporal SRT downloads.
g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_tlou_hot_srt_readback.cpp -o "$test_dir/tlou-hot-srt"

# Check Android's narrow fallback for TLOU 01.00 physics result layouts.
# The full APK build must fail if this changes or accidentally matches skinning.
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  gow3_pc/tests/test_tlou_physics_readback.cpp -o "$test_dir/tlou-physics-readback"

# Exercise all small GPU-writable compute shapes, without shader-specific
# identity and without falling back to global precise readbacks.
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  gow3_pc/tests/test_tlou_auto_readback.cpp -o "$test_dir/tlou-auto-readback"
"$test_dir/tlou-auto-readback"

# All 720/540/360 render choices preserve the 1080p logical aim/HUD and
# lower only the host Vulkan scene target via the existing upscaler.
grep -Fq 'options.put("resolution","original");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
grep -Fq 'env.put("GOW3_UPSCALER","fsr3");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
grep -Fq 'env.put("TLOU_AUTO_SMALL_CS_READBACK",geometryMode);' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
grep -Fq 'final String geometry=p.getString("geometry_sync","balanced");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
grep -Fq 'env.remove("GOW3_ASYNC_READBACK");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
grep -Fq 'EligibleBalanced(' gow3_pc/gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp
grep -Fq 'env.put("GOW3_ASYNC_READBACK","1");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java

# CUSA00552 01.00: do not regress to the PC-only hash/stride fallback or
# accidentally enable Readback 2 to compensate for stale physics results.
grep -Fq '0x697cfcc4ull' gow3_pc/gpu/shadps4/video_core/renderer_vulkan/tlou_physics_readback.h
grep -Fq 'TlouAndroidPhysicsShape(stage.pgm_hash' gow3_pc/gpu/shadps4/video_core/renderer_vulkan/vk_rasterizer.cpp
grep -Fq 'env.put("GOW3_READBACKS","1");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
"$test_dir/tlou-physics-readback"
"$test_dir/tlou-hot-srt"

# Distinguish fewer wait API calls from actual modeled producer stalls.
# Includes adverse locality, GPU rewrites, unused fields and late CPU writes.
# Timing is informational; deterministic counters gate claims, not noisy speed.
g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_tlou_srt_cost.cpp -o "$test_dir/tlou-srt-cost"
"$test_dir/tlou-srt-cost"

g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread gow3_pc/tests/test_cpu_phase_sample.cpp -o "$test_dir/cpu-phase-sample"
"$test_dir/cpu-phase-sample" --correctness-only

g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_tlou_srt_batch_plan.cpp -o "$test_dir/tlou-srt-batch"
"$test_dir/tlou-srt-batch"

# TLOU must never restore graphics-breaking Readback 2 from a legacy preference.
grep -Fq 'env.put("GOW3_READBACKS","1");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
if grep -Fq '"2 · Preciso' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortMenu.java; then
    echo 'ERROR: TLOU Precise readback was reintroduced' >&2
    exit 1
fi

# v0.31 opt-in paths: exact memory bytes, packet-local bindings and copy completion.
cc -std=c11 -O2 -Wall -Wextra -Werror -pthread \
  gow3_pc/tests/test_memory_walk_cost.c -o "$test_dir/memory-walk"
"$test_dir/memory-walk" --correctness-only
for test in ring_binding_cursor tlou_host_copy_batch; do
  g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread -I gow3_pc/gpu/shim -I gow3_pc/gpu/shadps4 \
    "gow3_pc/tests/test_${test}.cpp" -o "$test_dir/$test"
  "$test_dir/$test" --correctness-only
done
python3 gow3_pc/tests/extract_host_copy_chunks.py "$test_dir"
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread \
  -I "$test_dir" -I gow3_pc/gpu/shim -I gow3_pc/gpu/shadps4 \
  gow3_pc/tests/test_tlou_threaded_record_chunk.cpp -o "$test_dir/threaded-copy"
"$test_dir/threaded-copy" --correctness-only
