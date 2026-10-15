# Precipitation noise cache

`WeatherPrecipitationCache` holds 1024 tagged slots (32 × 32, 32768 bytes). Entries contain only position-dependent random terms and the rain phase polynomial. Exact signed seed wrapping, Java-compatible LCG draws, the first float, Gaussian pair and intervening float remain in reference order. Frame time, partial tick, camera position, surface height, intensity and biome classification are never cached.

A moving 19 × 19 precipitation footprint fits the modulo grid without internal collisions. Tags compare the full signed x/z coordinates, so negative coordinates, wraparound and replacement cannot return another column's noise. World switches need no invalidation because entries do not contain world data; `clear()` is available.

Use the cached path for snow. Rain only requires one cheap random float, and benchmarks showed lookup overhead exceeds its cost. An explicit warm/cold branch lets warm columns call the existing sampler with no precomputed argument; cold columns pass `&cache.get(x,z)` after the local-player count.

## Evidence

`tests/weather_precipitation_cache.cpp` compares every output field bitwise between cached and uncached rain/snow samples over negative/positive positions, temperature changes, tick boundaries including signed extrema, modulo collisions and cache reset. It passes, and the original golden precipitation policy fixture still passes.

`tests/benchmark_weather_precipitation_cache.cpp`, compiled with host `c++ -O3 -std=c++14`, measures seven trials of 6000 moving-camera 19 × 19 sample frames. Checksum equality is asserted. On the development laptop during other build work, final median results were:

| Sampler | Uncached | Snow-cache integration | Relative |
| --- | ---: | ---: | ---: |
| Rain | 3.587 µs/frame | 4.068 µs/frame | 0.88× |
| Snow | 13.883 µs/frame | 4.793 µs/frame | 2.90× |

The rain result includes the extra dispatch branch, with an absolute difference under 0.5 µs in this run. An earlier implementation that cached rain itself was slower and is not recommended. These are sampler CPU measurements, not whole-frame, GPU or phone performance. The observed several-millisecond rain/snow frame difference cannot be explained by this small arithmetic saving alone. Live rendering measurements remain required.

Local logs: `work/weather-precipitation-cache-test.log`, `work/weather-precipitation-cache-benchmark.log`.
