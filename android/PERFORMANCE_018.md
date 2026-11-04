# TLOU v0.18 — Small buffer-copy recording experiment

## Evidence from the heavy scene
At approximately 36 FPS in Nov 4 logs, the stable scene generated
24–25k upload batches (140–145 MiB) every five seconds, and around
1.0–1.1 s of GPU tick waits. The waits overlap other work and cannot be
removed blindly. Old DMA batching did not demonstrate FPS improvement.

## Implementation
TLOU_INLINE_SINGLE_COPY=1 captures a single vk::BufferCopy region of up to
64 KiB by value in the deferred command closure. The old
Scheduler::RecordData reserve + memcpy is avoided. The exact Vulkan
copy command, dirty-page selection, staging buffer lifetime, barrier
tracking, EndRendering, waits, and command order are preserved.
Multi-region and larger copies take the original path.
An OFF-by-default menu toggle controls the change.
Five-second counters report eligible and actually inlined copies.

A native test verifies size/count limits and deferred lifetime after the
caller's transient region array is mutated and destroyed. The experiment
may reduce CPU-side command-recording work but cannot reduce upload bytes
or mandatory GPU/CPU waits; device FPS improvements are unverified.
Compare ON and OFF in the same scene, GPU profiler OFF, keeping 720p,
Readback 1, Turnip, and the other experimental settings fixed.
