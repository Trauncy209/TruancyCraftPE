# Native GLES3 renderer performance audit

Measured on the laptop's Radeon 610M integrated GPU. The RTX GPU was not used for these benchmarks. Each matched pair uses the same copied native Infinite/Extreme world, view distance, Fancy graphics and simulation settings. Hidden hardware windows, uncapped rendering, VSync off,15 seconds warmup and30 seconds measured. The audio backend was silent in both variants. Other apps stayed open; the CPU miner was temporarily paused and then resumed.

| Scene | Before | Optimized | Improvement |1% low before → after |
|---|---:|---:|---:|---:|
| 854×480, Short distance |614 FPS |819 FPS |33% |81 → 110 FPS |
| 1920×1080, Normal distance |115 FPS |166 FPS |45% |31 → 41 FPS |

These are stationary laptop results, not measured phone FPS or combat/streaming results. Separate reverse-order repetitions of the CPU optimizations averaged 589 → 775 FPS at 854×480; absolute results vary with laptop load and temperature.1% low is the reciprocal of the mean frame time of the slowest 1% of frames.

## Changes

- Reuse CPU terrain render lists on the rendering thread, replacing repeated allocation and construction of thousands of descriptors per pass. Reset reused lists correctly when they shrink or become empty.
- Cache uniform locations and values in fixed slots per shader program, removing hot-path string creation and hash lookups.
- Prepare lighting and normal matrices when the lighting path uses them.
- Use an exact unlit/smooth/textured/identity-UV shader for the common case, retaining the full shader for other states. Each program has independent cached uniforms.

## Validation

64 GPU state-transition frames, including context reset, match the baseline byte-for-byte. A separate GPU test compares reused terrain lists with direct draws across 2/1/empty transitions. Chunk-boundary faces, batched inventory fallback icons, Unicode title/fade checks, and the XP/hearts HUD pass. Legacy UI fixtures were updated to create GLES3 contexts and use the in-game alpha-test state. Linux and both Android ABIs build; local Android test APKs retain the release signature and 16 KiB alignment. Phone runtime/FPS validation remains pending.

Raw measurements and source hashes: [verification report](../verification/performance-2026-11-19.json). The public 0.5.25.5 prerelease is unchanged; these changes are local performance test builds.
