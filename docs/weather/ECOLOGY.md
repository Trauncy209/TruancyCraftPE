# Weather ecology evidence

Behavioral reference is Moxie-Archive/4jcraft dev commit
529a366ad52a9b66a2ca887647a9abdd9500a0fc. This component is an independent
implementation. No reference source, textures or sounds were copied.

Reference observations: ServerLevel's eligible chunk loop samples freeze/snow
once with probability 1/16 per tick. It chooses a horizontal column and uses its
highest rain-blocking surface. Freezing can occur without rain; snow needs the
raining threshold. Both require temperature <=0.15 and block light <10. Freezing
requires source water (metadata zero) and a non-water horizontal neighbor. Snow
requires air, valid snow support, motion-blocking non-ice support. Neither stacks
additional layers onto an existing snow tile. Rain block hooks run on the exposed
support block in rain-enabled biomes.

TCPE integration uses the already-polled, already-loaded simulation chunk set.
It does not load/generate neighbors for a weather check. Source water on an
unloaded chunk seam waits until all four neighbors are loaded, rather than using
an empty-chunk boundary as an artificial shore. Ceiling dimensions, Java server
worlds and non-authoritative native clients cannot change terrain through this
component. Existing setTile notification/persistence paths carry real changes.

TCPE's historical biome generator exposes climate noise and biome labels, not
the reference biome temperature table. Snowy TCPE biome labels map to freezing
climate; taiga maps to 0.05, other snowy biomes to zero, and warm biomes to 1.
Only the 0.15 threshold is used. This deliberately preserves TCPE's existing
biome appearance; it is not evidence of a full 4jcraft biome/altitude match.
Rain exposure excludes cold and dry biomes and checks roofs. TCPE has no cauldron
or generic block handleRain interface, so absent block types cannot yet match
those reference rain hooks. Fire's random ticking is explicitly disabled by inherited TCPE code, but its
onPlace schedules ordinary ticks. Rain extinction is now integrated into those
existing ticks without enabling random fire spread. Reference exposure includes
the fire's column and four horizontal neighbors; solid roofs shelter all five.
It is authority/native-only and excludes dry, cold and ceiling dimensions.
Existing native netherrack (Tile::netherrack, id 87) now provides reference
infinite-fire support instead of the inherited hardcoded false flag. Fire
continues scheduled checks after age 15, so replacing netherrack with ordinary
support can still extinguish it in rain. No unsupported End-dimension behavior
was invented; full fire-system parity is not implied.

Tests: `tests/weather_ecology.cpp` checks threshold boundaries and every rule
predicate. `tests/weather_ecology_world.cpp` checks the actual native Level
queries for source metadata, pool edges, warm/cold biomes, block light, snow
support, ice exclusion, roofs, dry/cold rain exclusion and finite bounds.
`work/build-weather-ecology-test.py` links that fixture with the completed native
server objects without invoking a full build. Its output is retained in
`work/weather-tests/ecology-world.log`. These checks are terrain-rule evidence;
they do not prove rendered snow, multiplayer notification or phone performance.

Actual deterministic Level fixture results: 512 authoritative cold rain chunk ticks
created snow on 20 columns; client and ceiling passes changed none. Another
2,048 cold clear chunk ticks froze 11 source-water columns without adding snow.
Assertions confirm neighboring chunks stayed unloaded through both mutation
passes, including setTile neighbor notifications.

## Real native networking check

`tests/weather_ecology_network.cpp` attaches the real server network listener to
an isolated Level and connects two RakNet clients. The same ecology mutation
passes produced 20 snow and 11 ice UpdateBlock messages for each client. A third
client joined afterwards, requested chunk (2,2), decoded its block payload and
matched all 256 tested columns to authoritative terrain, including 11 ice tiles.
The log is `work/weather-tests/ecology-network.log`. This checks wire delivery
and late-join terrain contents, not a rendered phone client. The fixture links
coherent local Packet/ServerSideNetworkHandler objects because the development
lightning callback changes the callback vtable relative to earlier server
objects; mixing those versions is not a valid network test.

## Roof cache constraints

Current top-rain-height scans use material motion/liquid predicates, whereas
LevelChunk's historical heightmap uses light attenuation. Glass can shelter
rain while admitting skylight, so substituting the light heightmap is incorrect.
Any persistent roof cache must invalidate on actual chunk block writes, including
setTile/setTileAndData, setTileRaw and bulk chunk-data rebuilds. Listening only to
LevelListener::tileChanged misses client chunk ingestion through setTileNoUpdate.
The cache must be owned by the chunk or include a generation/revision identity
so unloaded/replaced chunks cannot reuse stale roof heights.

CPU-only roof microbenchmark on this Linux laptop: 1,000 passes over 256 loaded
columns with their surface at y=100 averaged 0.0257 ms/pass. Glass placed at
125 raised the roof query to 126, and removal restored 101. The standalone
fixture is `work/weather-tests/ecology-roof-benchmark.cpp`; its log is retained
beside it. This measures query work only, not GLES fill, FPS or phone cost.

Additional reference detail: TopSnowTile allows leaf support independently of
leaf graphics mode. Inherited TCPE TopSnowTile consulted isSolidRender,
which changes with fancy leaves. Its support predicate now explicitly permits
leaves. The actual Level regression rebuilds Tile.cpp, exercises placement and
neighbor survival with both fancy settings, and checks ecology snow eligibility. Reference cauldron handleRain
runs a 1/20 attempt and increases fill metadata up to 3; TCPE has no cauldron
block to receive that event yet.

Actual scheduled-fire evidence: `tests/weather_fire_rain.cpp` uses real Level,
LevelChunk, onPlace scheduling and tickPendingTicks with isolated blank chunks.
Exposed and open-side fires extinguish; fully roofed, dry, cold, clear, client,
Java and ceiling cases retain fire through the tested tick. A neighboring chunk
unloaded after scheduling causes the tick to be skipped without chunk creation.
`work/build-weather-fire-rain-test.py` rebuilds fixture-local Tile.cpp so its
inline FireTile vtable matches current code. Log: `work/weather-tests/fire-rain.log`.

Follow-up finite-edge regression: scheduled rain fire executes at x/z 0 and 255,
including both tested corners, with no loaded chunks outside the finite world.
Both addToTickNextTick and tickPendingTicks share the clipped finite
neighborhood; clipping dispatch alone would never enqueue an edge callback.
Java and infinite worlds retain their full neighborhood requirement, explicitly
tested by missing outside chunks. The fixture also proves netherrack fire
survives rain through age 15 and extinguishes after support changes to stone.
The same fire-rain log records these passing checks.
