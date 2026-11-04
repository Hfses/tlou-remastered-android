# TLOU v0.22 — remove no-gain experiments, test fine GPU dependencies

Baseline: v0.21 heavy scene 35.7 / 36.6 / 36.2 FPS at 720p,
18–19k coherent mapped readbacks and 24–25k uploads per 5 seconds.
SRT grouped zero independent tables in the heavy scene. It, the
inline-copy recording experiment and their native tests were removed.
The recent inactive Android A/B switches (split64, mapped fault reads,
stack snapshot, write wait relocation, guest landing, 512-byte static
threshold) were removed from the user menu and launch environment.
Proven direct coherent mapped readbacks and Readback 1 remain.

Potential root cause: large GPU writes between 64 KiB and 1 MiB used
to stamp one producer tick across an entire 1 MiB window. Each tiny
readback in the window then waited for its newest producer, even if
that write touched a different 64 KiB block. The new
TLOU_FINE_GPU_TICKS experiment tracks those medium GPU writes at
64 KiB granularity. Small <=64 KiB writes keep 4 KiB page ticks.
Writes >1 MiB keep mandatory 1 MiB fallback ticks.
Readback uses max(page tick, exact 64 KiB write tick,
oversized-write fallback). The original coarse path is unchanged
when the option is OFF.

All memory protection, GPU barriers, full producer-tick waits,
readback merges, staging fallbacks and original GPU writer tracking
remain. A five-second counter records fine-pages,
coarse-overwait-pages (cases where the 1 MiB tick was unnecessarily
newer) and huge-fallback-pages (large writes still requiring coarse
dependence). This is a candidate optimization, not a verified FPS
improvement; do not repeat the existing OFF baseline.

The native test checks cross-boundary writes, unrelated medium
writes in the same 1 MiB window, 4 KiB overrides and huge writes.
If the device counters show no over-waits in the heavy scene,
the next work must focus on GPU execution rather than adding more
micro-optimization switches.
