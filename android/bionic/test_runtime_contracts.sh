#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
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

"$test_dir/tlou-compute-split"

g++ -std=c++20 -O2 -Wall -Wextra -Werror gow3_pc/tests/test_tlou_write_tick.cpp -o "$test_dir/tlou-write-tick"
"$test_dir/tlou-write-tick"

# TLOU must never restore graphics-breaking Readback 2 from a legacy preference.
grep -Fq 'env.put("GOW3_READBACKS","1");' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortSettings.java
if grep -Fq '"2 · Preciso' android-launcher/app/src/main/java/ro/cobrabm/fexdroid/PortMenu.java; then
    echo 'ERROR: TLOU Precise readback was reintroduced' >&2
    exit 1
fi
