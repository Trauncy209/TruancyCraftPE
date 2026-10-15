# Native TCPE dedicated server — alpha

A standalone Linux host for TCPE's native multiplayer. It runs the existing TCPE world and networking code without opening the game, a window, graphics context or audio device. Java networking and Rust are not required by this server.

Use the matching native-client test builds. In TCPE, choose **Servers → Add → Type: TCPE**, enter the server computer's IPv4 address or hostname, and leave the port at **25565**. This server accepts native TCPE clients; Java Edition servers remain a separate client connection type.

## Start a server

Extract the Linux server archive and run:

```sh
./start-server.sh
```

The default is a Survival world named `world`, with space for 16 connected players. The server listens on UDP 25565 for gameplay and TCP 25565 for its list entry. Allow both protocols through the host firewall; hosting across the internet also requires the appropriate router forwarding.

To choose a name, world, port and player limit:

```sh
./start-server.sh --name "Our TCPE world" --world friends --port 25565 --max-players 8
```

Use `--creative` to create a Creative world, `--seed NUMBER` for a new world's seed, and `--directory PATH` to choose the directory containing saved worlds. Existing worlds keep their saved seed and game mode. The initial world generator is TCPE's original finite generator.

The server creates the selected world folder and a sibling `.lock` file. A second process cannot open the same world while it is running. Keep the world folder when updating the executable.

## Icons and saving

Place a **64 × 64 PNG**, named `server-icon.png`, beside `start-server.sh` before starting. Its size must be at most 64 KiB. The listing shows the server name, connected player count and supplied icon. TCPE uses its default icon when no custom image is supplied.

Worlds save automatically every 60 seconds. Set `--autosave SECONDS` to change this (10–3600 seconds).

The console accepts `save`, `list`, `help` and `stop`. **Ctrl+C and SIGTERM save the world and stop cleanly.** Closing standard input does not stop a background server. Use `./start-server.sh --help` for all options.

## Build from source

Requires Linux, CMake 3.21 or later, a C++14 compiler, the platform threading library and zlib development files. The server-only configuration does not fetch graphics dependencies or require a Rust library. In the downloaded server folder, first enter the included `source` directory; in a full checkout, use the repository root.

```sh
cmake -S . -B build-server -G "Unix Makefiles" -DTCPE_DEDICATED_SERVER=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-server -j1
./build-server/tcpe-server --help
python3 tools/test-dedicated-server.py --build build-server
```

The downloadable x86_64 build is compiled on Ubuntu 24.04. Source and test evidence are kept with the local server checkpoint. This alpha has not been published as a GitHub release.

## Compatibility and verification

- [x] Headless startup with no graphics, audio or Java networking libraries.
- [x] Actual dedicated Survival world generation and loading.
- [x] Two native protocol clients log in and receive distinct player identities, player visibility and mobs.
- [x] Movement, chat, block updates and chunk deltas pass through the real world handlers.
- [x] Listing changes from 0/2 to 2/2 and supplies the configured PNG.
- [x] Player capacity and duplicate-world access are rejected correctly.
- [x] Autosave, console save/list/stop and signal shutdown.
- [x] The original seed and changed block survive a full process restart.
- [x] Invalid settings are rejected before hosting.
- [x] The legacy ACK timestamp sentinel uses a defined integer conversion, with a regression check under optimized builds.
- [ ] Physical phone and rendered Linux-client acceptance across inventory, crafting, containers, combat and longer play sessions.

These checks exercise actual native server world handlers using protocol clients. They do not establish complete gameplay fidelity or production readiness.
