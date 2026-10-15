# Integrated weather review

Review of development source on 2026-10-15 against pinned reference `529a366ad52a9b66a2ca887647a9abdd9500a0fc`; this is not a fidelity completion claim.

## Confirmed by inspection and focused checks

- Native precipitation and transient lightning are called after world geometry and before the held-item pass; both bypass Java worlds. Rain/snow batches reset their geometry offset, restore the prior shade mode and reset alpha/depth/blend/culling state for the next pass. `GLES3Compat::getInteger` implements `GL_SHADE_MODEL`, so the query does not invoke an unsupported GLES3 enum.
- Current smooth-rain screenshot (`work/weather-rain-visible-smooth.png`) visibly contains rain streaks through open areas, unlike the earlier missing/flat-shaded result. This does not prove motion quality, snow appearance or physical-phone performance.
- Rebuilt and reran `weather_precipitation_policy`, `weather_celestial_policy`, `weather_sound_voice_budget` and `weather_impact_policy` fixtures: all passed. Their scope is policy/math, not whole-game rendering.
- Native weather commands check connected player identity and the existing operator allowlist before mutation. In-game `/op` and `/deop` are refused. Server weather/lightning packet callbacks default to no-op; client weather updates require the connected server GUID. Existing username-only operator trust is unchanged.
- Audio uses the bundled `minecraft:weather.rain` event; all eight WAV variants were verified as mono PCM 44.1 kHz. Longest is 2.1920181405895693 seconds, so sheltered half-pitch playback at the earliest 10 Hz cadence needs at most 44 simultaneous tails. Requested 48-rain/64-total budget fits that case. Actual OpenAL null-backend fixture passed; Android Java/D8 compilation is not physical-device evidence.
- Java disconnection resets server-pack sound state through the existing sound-bank reset. Native renderer/effects checks bypass Java weather. Full interactive world-switch regression is still needed.

## Findings to resolve or explicitly retain as fidelity limits

1. `WeatherEffects.cpp` reads shared `Tile::yy1` without updating the shape at the sampled metadata/location. Upper/lower slab rain impacts can use stale bounds from another block.
2. Rain impact particles currently enable collision. The pinned reference deliberately disables particle physics and uses a small occupancy/liquid removal check. Current collision-heavy drops add avoidable per-particle work on phones and have different removal behavior.
3. Rain impact lifetime is currently 8–15 ticks rather than the reference distribution from `8/(U*.8+.2)` (roughly 8–40 ticks); the impact sampler uses TCPE's Mersenne Twister rather than the reference 48-bit random process. Geometry's separate precipitation sampler has the reference random process, but that does not establish impact-particle equivalence.
4. Biome temperatures in the render integration map TCPE cold/warm classes to representative values. The reference has more biome-specific temperature data. The reference `scaleTemp` is identity at this pin, so adding altitude cooling would introduce another mismatch.

No running game/server or public release was changed by this review.

## Impact follow-up

The three impact findings above were addressed in development source after review:

- Sampled blocks now update their metadata-dependent shape before reading the top surface. The helper tests distinguish lower-slab and full-height impact positions.
- Drops use 0.01 bounds, collision-free movement, gravity 0.06/tick, 0.98 drag and a direct solid/liquid occupancy check, matching the reference behavior model. Missing chunks expire particles instead of causing generation.
- Lifetime now follows `8/(U*.8+.2)`. Column/reservoir sampling uses the reference-compatible 48-bit random generator with signed 32-bit tick-seed wrapping. Particle appearance randomness is separate from column sampling. `GameRenderer` should supply its `_tick` through the new overload so the effect sampler shares the reference tick timeline.
- Reference inspection additionally established that impact intensity is divided by `activePlayers()+1`; a single active viewport therefore makes 25 impact attempts at full fancy rain or 6 on fast graphics. This does not reduce precipitation-quad density or change the earliest ambience cadence.

Updated `tests/weather_impact_policy.cpp` passes lifetime extrema, slab heights, occupancy/liquid thresholds, bounded random known answers, seed wrapping, intensity/player-count scaling and sheltered/exposed policy. The updated translation unit compiles. Actual particle visual comparison and phone frame-cost measurements remain pending. TCPE's separate per-particle random stream is still its native stream, so this is not a claim of identical random particle positions/appearances across implementations.
