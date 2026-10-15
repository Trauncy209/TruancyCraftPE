# Weather compatibility and fidelity evidence

Audit snapshot: 2026-10-15. This is a development audit, **not a 1:1 fidelity or
release-readiness claim**. The visual goal is attractive, readable rain, snow,
lightning and storms that feel native to TCPE. Passing simulation tests alone
does not meet that visual requirement.

Pinned behavioral reference:
https://github.com/Moxie-Archive/4jcraft/tree/529a366ad52a9b66a2ca887647a9abdd9500a0fc

## Provenance and implementation scope

The pinned reference's root LICENSE contains GPLv3. Repository licensing does
not establish provenance or redistribution rights for every inherited console
asset. Weather logic and geometry here independently express inspected behavior;
no 4jcraft source text, textures or audio have been copied. New independent
helpers include WeatherState, WeatherEcology, WeatherLightning/Runtime/Geometry,
WeatherPrecipitationPolicy, WeatherEffects and WeatherCelestialPolicy.

Rain/snow textures already existed in TCPE and are unchanged:

| Existing asset | SHA-256 |
| --- | --- |
| `data/images/environment/rain.png` | `0ca0d08c4d387f5ab5d16b860ceedf6b03a629ce8df4b2510e0e0035bbbacfa5` |
| `data/images/environment/snow.png` | `c03f30066db0275ba26cce615c18d79ad035317ed3089d84f4f3a295af016b2f` |

Rain/thunder use the existing Java sound bank imported from the user's supplied
assets; impact uses existing TCPE particle art and native explosion audio.
The added charged-Creeper texture comes from the user-provided Java 1.21.4
archive, not 4jcraft; its exact entry/hash and retained rights are documented in
[POWERED-CREEPER-ASSET.md](POWERED-CREEPER-ASSET.md). User authorization to reuse
supplied assets is not a new license grant from Mojang/Microsoft. No root
LICENSE was present in the TCPE worktree at this audit; these notes do not resolve
inherited code/asset licensing or grant additional distribution rights. Preserve
these provenance notes in source/build deliverables.

## Pinned behavioral rules and deliberate adaptations

- Reference rain and thunder timers are independent. Nonpositive timers are
  rescheduled without toggling; a positive timer decrements, toggling at zero.
  Clear delays are 12,000–179,999 ticks; wet duration 12,000–23,999; thunder
  duration 3,600–15,599. Intensities move by 0.01/tick, interpolate per frame,
  and effective thunder multiplies raw thunder by rain. Wet/storm thresholds
  are >0.2/>0.9. Ceiling dimensions skip scheduling. Loading saved targets
  restores their full initial intensities, as observed in the reference.
- Reference days contain 24,000 ticks; TCPE retains its native 19,200-tick day.
  Dimension::getTimeOfDay proves native noon at 4,800 and midnight at 14,400.
  Weather timer ranges remain reference tick counts, not scaled TCPE days.
- Rain/snow use loaded columns, motion/liquid roof heights, reference-derived
  column randomization, rain UV motion, snow Gaussian drift, distance opacity
  and the rain upper-edge fade. Reference view radii are 9/7/5/5 for 1/2/3/4
  local players. TCPE currently renders one local view; reduced graphics uses
  radius 5 as a phone-cost adaptation. Degenerate center-column geometry is
  skipped to avoid reference 0/0 producing nonfinite GLES vertices.
- Native biome labels map taiga to 0.05, other snowy biomes to zero and warm
  biomes to 1 for the reference 0.15 precipitation/freezing threshold. This
  preserves TCPE biomes; it is not an exact reference biome-temperature map.
  The inspected reference scaleTemp returns its input at every height.
- Rain impact attempts scale with 100 × intensity² and reduced settings.
  Lava emits smoke; other exposed surfaces emit drops. Rain ambience has
  roof-dependent volume/pitch. TCPE's effect RNG and impact particle trajectory
  are independently implemented, not a claim of identical random sequences.
- One eligible loaded chunk has a lightning trial of 1/100,000 each tick;
  freeze/snow has a 1/16 trial. Source-water freezing requires light <10,
  temperature <=0.15 and a non-water horizontal neighbor. Freezing can happen
  while clear; snow requires wet weather, air and motion-blocking non-ice
  support. Leaf support is independent of fancy graphics.
- Lightning begins life 2 and has exactly one additional flash with a fresh
  geometry seed. Active ticks request sky illumination; initial thunder/impact
  audio is emitted once. Hit bounds are x/z ±3, y −3..+9. Authority applies
  fire-immune-aware burn(5), fire duration transition, Pig→PigZombie and powered
  Creeper behavior. Initial fire needs difficulty >=2 and tracked/loaded
  neighborhoods; the additional flash has its separate permission rule.
- Tracking conservatively means loaded chunks within four chunks of an active
  player; TCPE's world RNG/column selection differ from the reference weather
  LCG. Matching seeds therefore do not promise identical strike times/places.
  Reliable ordered strike/reflash/end events with deduplication are a native
  protocol adaptation, not copied reference packets.
- Native clear sky/cloud palette and base explosion scale are retained, with
  reference-derived relative rain/thunder attenuation. Celestial visibility
  fades with rain. These are explicit TCPE compatibility choices, not a claim
  of matching every reference sky pixel or world explosion radius.

## Requirement audit against current evidence

Paths beginning `work/` below are retained local reproducibility evidence, not
files guaranteed to be packaged into a public release.

| Requirement | Implementation and authoritative evidence | Remaining verification or limitation |
| --- | --- | --- |
| Scheduling, smooth transitions, ceiling skip | `tests/weather_state.cpp` exercises timer boundaries/ranges, interpolation, target transitions, clamping, client/authority split and sleep clearing | No exhaustive long-duration rendered reference comparison |
| NBT fields, legacy saves and malformed snapshots | `tests/weather_storage_packet.cpp`; `work/weather-tests/storage-integrated.log` passes save/load, missing-field defaults, copy semantics and malformed/truncated wire rejection | Graceful-stop timer regression now passes separately; serialization alone was insufficient proof of lifecycle persistence |
| Full world process restart | `tests/weather_process_restart.cpp`, `work/test-weather-process-restart.py` now compare last observed timer against saved NBT before restarting a real temporary server | Test exposed and fixed a shutdown metadata-save omission in native Minecraft::leaveGame. Strict retest with rebuilt Minecraft unit: client 23895 → disk 23880 → restarted client 23820 ticks, targets/intensities preserved. Final canonical server strict storm and command-clear process restarts now pass `process-restart.log`; actual bed-cleared external save → canonical server → late client also passes `bed-process-restart.log`, preserving two distinct remaining clear timers. Server SHA/input hashes are retained in `lifecycle-inputs.json`; earlier broad target-only PASS was insufficient |
| Authority, permissions, console-only operator grants | `/weather clear/rain/thunder [seconds]` requires existing operator permission; client `/op`/`/deop` denied. `work/weather-tests/operator-permissions.log` passes default denial, console grants/revokes and grant persistence | `tests/weather_lightning_network.cpp` / `work/weather-tests/lightning-network.log` now proves connected malicious client weather/bolt packets cannot change server authority or broadcast forged events. `tests/weather_client_source.cpp` / `work/weather-tests/client-source-boundary.log` additionally passes actual client handlers and loopback packets from distinct admitted authority/unrelated peers: GUID rejection precedes weather/strike mutation and initial sound events. Full malformed command matrix remains separate |
| Late joins/reconnects and multiple clients | `tests/weather_network.cpp`; `work/weather-tests/network-run.log` passes repeated authoritative storm snapshots, two-client convergence and replacement-client join during storm | Two rendered client windows/physical phone storm synchronization not yet compared |
| Biome/roof-aware rain and snow | GLES3 precipitation pass builds; `tests/weather_precipitation_policy.cpp` and `work/weather-precipitation-policy-test.log` pass seeded random/UV/direction/opacity/height/temperature policy | Saved ClearQA/RainQA/ThunderQA/SnowQA/NightThunderQA worlds prepared. Parent live visual comparison is in progress; no rendered 4jcraft side-by-side evidence yet |
| Attractive native precipitation/storm presentation | Actual Linux captures exist at `work/weather-rain-visible-smooth.png`, `work/weather-storm-alpha.png`; development trace reports rain=1, thunder=1, 288 rain columns in warm test view | Captures establish rendering occurred, not beauty, snow quality, bolt visibility or acceptance. Compare day/night, moving camera, shelter transitions and mobile readability before sign-off |
| Rain impacts and lava smoke | `WeatherEffects.cpp` runs client-only once/tick using loaded, rain-eligible columns; `tests/weather_impact_policy.cpp` tests attempt and shelter rules | Actual visible impact/lava tests and trajectory comparison pending |
| Rain/thunder sound and mobile mixer limits | Thunder routes through existing canonical Java bank. `work/weather-tests/lightning-audio.log` verifies PCM decode; `work/weather-voice-budget-backend.log` passes real OpenAL null-backend 48-voice cap, reserved effect voices and rain-tail reclaim | Null backend is not listening evidence. Actual roof attenuation, thunder distance/gain, no repeated sound and simultaneous-client playback need audible tests; Android backend/device pending |
| Sky, fog, cloud, light and celestial visibility | Level/GameRenderer/LevelRenderer use native weather attenuation/flash; `tests/weather_celestial_policy.cpp`, `work/weather-celestial-policy-test.log` pass relative celestial rules | Keep TCPE palette as documented. Controlled same-camera day/night clear/rain/thunder visual comparison and actual terrain light response pending |
| Lightning lifecycle, geometry, world/entity effects | `tests/weather_lightning.cpp`, packet and actual-Level tests; `work/weather-tests/lightning-world.log` passes fire placement, damage/immunity, 160-tick fire transition, PigZombie pose, Creeper power persistence, one extra flash/audio, client no-damage/dedup and no chunk generation | `lightning-network.log` now proves real two-client identical spawn/reflash/end ID/seed/coordinates and exactly one extra flash, expired-event exclusion on late join and server rejection-by-no-op of client forgeries. Policy mesh has 224 finite quads. Rendered bolt/flash/powered-overlay and audible simultaneous playback still pending. See `LIGHTNING.md` |
| Freeze/snow mutations and network terrain delivery | `work/weather-tests/ecology-world.log`: 20 snowy columns/512 ticks and 11 frozen columns/2048 clear ticks; client/ceiling worlds unchanged; fancy/fast leaf placement/survival pass. `ecology-network.log`: both clients receive deltas; late chunk matches all 256 tested columns | Network fixture decodes packets, not full rendered phone terrain. Biome mapping and conservative unloaded seams are documented. See `ECOLOGY.md` |
| Other rain block effects | Exposed-rain helper excludes dry/cold biomes and roofs | Actual scheduled fire rain extinction now passes `tests/weather_fire_rain.cpp` / `work/weather-tests/fire-rain.log`: exposed center/cardinal removal, full-roof/dry/cold/clear/client/Java/ceiling exclusion and no unloaded seam generation. Random ticking/spread flags are preserved. Existing native netherrack (id 87) now protects fire from rain/natural expiry; aged fire continues scheduled support/weather checks. Regression covers age15 survival and removal after support replacement. Finite scheduling now clamps both enqueue/dispatch neighborhoods at all edges/corners; Java/infinite guards remain unchanged. TCPE has no reference cauldron/handleRain interface; absent-block/full fire-system parity is not claimed |
| Sleep/weather interaction | ServerLevel successful all-player awakening clears flags/timers and broadcasts; state test confirms gradual fade is preserved | `tests/weather_bed_sleep.cpp` / `work/weather-tests/bed-sleep.log` exercises actual two-Player bed API: one sleeper preserves storm; both sleep 100 ticks, dawn/clear broadcast and smooth fade pass. `tests/weather_bed_saved.cpp` plus `work/test-weather-bed-process-restart.py` additionally verifies actual bed-cleared world save/reload through a fresh canonical dedicated process and real client snapshots, retaining distinct rain/thunder timers. Rendered clicks/mobile inputs and real two-client bed interaction packets remain pending |
| Preserve Java compatibility and old native clients | New simulation/effects/renderers bypass Java worlds; native packet additions are appended, unknown packet factory cases ignored. Dedicated baseline lifecycle tests pass | Actual unchanged released 0.5.24.1 Linux client now joins/renders/moves/chats/mines against final canonical weather server while storm snapshots and natural strike phases broadcast; `work/weather-tests/legacy-client-compatibility.json` records release hash, screens and observer evidence. It intentionally ignores weather packets. Live Java-server gameplay and old Android checks remain separate |
| GLES3 Linux/dedicated builds | `work/weather-tests/final-linux-builds.log` completes final coherent client and dedicated server builds; live isolated Linux weather window exists | Source continues changing; final artifact/source manifest must identify exact inputs. Existing unrelated compiler warnings remain, not blanket-clean claims |
| Android 32/64, release signatures | Both final signed `0.5.24.1-weather-dev` Android ABIs pass certificate, identifier, version, native-input and 16 KiB alignment checks in `outputs/tcpe-java-native/weather-development/android-verification.json`; arm64 installs and launches on SM-F946U Android 16 | User reports “weather all is good on my phone” for installed arm64 build; see `work/android-phone-before-weather/user-weather-feedback.json`. Individual state/audio/gameplay measurements and 32-bit launch remain unverified |
| Phone performance and controlled Linux frame cost | Roof-query microbenchmark: 0.0257 ms for 256 columns/pass on this laptop; two precipitation batches, loaded-only queries, reduced radius and bounded audio voices | Microbenchmark excludes full renderer/fill/audio. An authorized Samsung SM-F946U is connected; physical Android FPS/frame-time, memory, battery/heat and touch controls remain unverified. Automated phone input is paused pending confirmation after its notification panel opened unexpectedly. Controlled Linux clear/rain/snow/storm comparisons also pending |
| 1:1 comparison and release approval | Reference pin and documented adaptations identify what is being compared | Missing visual/audio/gameplay/performance evidence and compatibility differences prohibit 100%/1:1 claims. Weather version and publication still require user agreement; 0.5.24.1 authorization covers movement fixes only |

## Required remaining checks

1. Retain the verified canonical lifecycle/input evidence with deliverables:
   strict storm and command-clear process restarts, plus actual bed-cleared
   world save → new canonical server → real late-client clear state all pass.
   Physical Android world leave/reload is still part of device QA.
2. Extend the passing actual two-Player bed API and persisted-clear tests to
   rendered singleplayer and two real native clients. API wake timing/fade,
   saved clear flags and both countdowns across process reload already pass.
3. Preserve the passing security evidence with final source/input hashes:
   connected-client→server forgery rejection, actual client GUID-handler tests,
   unrelated/authority loopback transport, invalid packet checks and duplicate/
   retired event sound suppression. These count Level sound events, not audible
   hardware playback or arbitrary malformed-command fuzzing.
4. Complete same-camera clear/rain/thunder daytime and nighttime captures plus
   snow, glass/solid shelter, moving camera, impacts/lava, lightning and charged
   Creeper captures; compare against the pinned reference and assess the user's
   visual-quality requirement explicitly. Keep reference comparisons separate
   from intentional TCPE palette/biome adaptations.
5. Listen to actual rain under/outside roofs, near/far thunder and simultaneous
   sound effects on Linux and Android; verify mixer budgets preserve gameplay.
6. Measure controlled clear/rain/snow/storm Linux frame times and memory, then
   equivalent physical Android 32/64 scenarios and mobile input. A connected device and successful installation do not themselves establish performance.
7. Preserve verified final weather Android builds/signatures and source/input hashes
   and notices, and retest native multiplayer plus Java compatibility. Choose a
   later weather version with the user before any public weather release.

Initial tests and QA worlds are isolated. Do not alter public TCSMP or close the
user's game merely to obtain evidence. No complete-fidelity claim or weather
publication is authorized by this audit.
