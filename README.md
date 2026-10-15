# TruancyCraftPE

TruancyCraftPE is a community restoration and expansion of **Minecraft: Pocket Edition 0.6.1 alpha**.

This project is about keeping the soul of old MCPE while pushing it forward on modern devices.

**Our philosophy:** this should be a **community-made Minecraft, not corporate slop**.

We want to preserve what made early Pocket Edition special, fix what was broken, and steadily build a better version of it in public.

[![Download Latest Release](https://img.shields.io/badge/Download-Latest%20Release-2ea44f?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/releases/latest)
[![Join the Discord](https://img.shields.io/badge/Join%20the%20Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/zeuspvp)
[![Release](https://img.shields.io/github/v/release/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Trauncy209/TruancyCraftPE/total?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/releases)
[![Stars](https://img.shields.io/github/stars/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/stargazers)
[![Issues](https://img.shields.io/github/issues/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/issues)
[![Last Commit](https://img.shields.io/github/last-commit/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/commits/main)

**v0.5.25.1 · Native server fix** · [Previous release: v0.5.25](https://github.com/Trauncy209/TruancyCraftPE/releases/tag/v0.5.25) · [Checksums & signing details](verification/README.md)

Pick the build for your device:

| Your device | Download |
| :--- | :--- |
| Android 64-bit — recommended for most newer phones | [Android 64-bit APK](https://github.com/Trauncy209/TruancyCraftPE/releases/download/v0.5.25.1/TruancyCraftPE-0.5.25.1-Android-arm64-v8a.apk) |
| Android 32-bit — older ARM phones | [Android 32-bit APK](https://github.com/Trauncy209/TruancyCraftPE/releases/download/v0.5.25.1/TruancyCraftPE-0.5.25.1-Android-armeabi-v7a.apk) |
| Linux 64-bit — x86_64 desktop/laptop | [Linux archive](https://github.com/Trauncy209/TruancyCraftPE/releases/download/v0.5.25.1/TruancyCraftPE-0.5.25.1-Linux-x86_64.tar.gz) |
| Host your own native TCPE server — Linux x86_64 | [Server download](https://github.com/Trauncy209/TruancyCraftPE/releases/download/v0.5.25.1/TruancyCraftPE-Server-0.5.25.1-Linux-x86_64.tar.gz) |

On Linux, extract the archive and run `launch.sh`. Keep the included `data` and `lib` folders beside the game.

Both Android APKs and Linux require **GLES3-capable graphics**. The GLES3 shader backend supports TCPE's existing drawing calls; the game is still native TCPE with Rust handling Java networking.

**Join the community:** https://discord.gg/zeuspvp

If you want updates as they drop, **star the repo, join the Discord, and turn on notifications/watch it** — releases are frequent and you do not want to miss out.


## Native TCPE servers

Our **[native server software](https://github.com/Trauncy209/TruancyCraftPE-Server)** is here. Come play **TCSMP** at `45.41.206.239:25566` — add it as **Type: TCPE**. Native TruancyCraftPE survival with infinite Extreme Terrain and custom world generation, hosted through Chicago. Server setup and alpha limitations are in the [server repo](https://github.com/Trauncy209/TruancyCraftPE-Server).

v0.5.25 brings **rain, snow and thunderstorms** to singleplayer and native TCPE servers: smooth weather changes, lightning, rain sounds, snow buildup and freezing. Weather saves with the world and stays in sync when players join. Singleplayer owners and server operators can use `/weather clear|rain|thunder [seconds]`; multiplayer operator grants stay console-only. Same Pocket Edition feel, with a world that does a little more.

v0.5.25.1 fixes native infinite-server joins that could hang on **Locating server**. Clients now receive the server's full terrain, including custom generation. Use the matching client and server.

Native multiplayer is still alpha. **Microsoft login is not working yet**; username-only Java servers remain supported.

## Come say hi

I'm also really busy running our cracked Java network, **[ZeusPvP.net](https://www.zeuspvp.net/)**. Feel free to [join our community](https://discord.gg/zeuspvp), say hi, and mention **TruancyCraftPE**!

**Minecraft server:** `ZeusPvP.net` · **Discord:** [discord.gg/zeuspvp](https://discord.gg/zeuspvp)

## Signed releases

Official APKs now use our own **Truancy209 release certificate**. v0.5.22.1 keeps the v0.5.22 game files unchanged and replaces debug signing. [Hashes, the public certificate, and VirusTotal links](verification/README.md) are published with the downloads.

v0.5.23 brings game changes again. Its Android APKs use the same release key, and the Linux archive has a detached release signature. Checksums and verification notes accompany the downloads. Classic/infinite worlds, terrain options and touch controls carry forward from previous builds.

Coming from an older build? [Read the signing update notes](docs/RELEASE-SIGNING.md) before replacing an installation so your worlds stay safe.

## Screenshots

A look at TruancyCraftPE on mobile. These screenshots are from earlier builds; newer v0.5.23 screens will be added as the game keeps improving.

### Main menu and player setup

<table>
<tr>
<td align="center" width="50%"><img src="docs/screenshots/13-main-menu.jpeg" alt="Main menu screen" width="100%"><br><sub>Main menu with Start Game, Join Game, and Options.</sub></td>
<td align="center" width="50%"><img src="docs/screenshots/14-username-setup.jpeg" alt="Username setup screen" width="100%"><br><sub>First-run username setup for chat and multiplayer identity.</sub></td>
</tr>
</table>

### In-game on mobile

<table>
<tr>
<td align="center" width="50%"><img src="docs/screenshots/01-paused-night-gameplay.jpeg" alt="Paused nighttime gameplay on mobile" width="100%"><br><sub>Pause menu over real nighttime gameplay.</sub></td>
<td align="center" width="50%"><img src="docs/screenshots/02-night-gameplay-hud.jpeg" alt="Nighttime gameplay HUD on mobile" width="100%"><br><sub>Touch HUD, hotbar, hearts, torch lighting, and in-game pause button.</sub></td>
</tr>
</table>

### Multiplayer menus

<table>
<tr>
<td align="center" width="50%"><img src="docs/screenshots/12-servers-browser.jpeg" alt="Servers browser screen" width="100%"><br><sub>Server browser with scanning, Join, Delete, and Add flows.</sub></td>
<td align="center" width="50%"><img src="docs/screenshots/11-server-details.jpeg" alt="Server details add server screen" width="100%"><br><sub>Touch-friendly Add Server / Server Details form.</sub></td>
</tr>
</table>

### Options and settings

<table>
<tr>
<td align="center" width="50%"><img src="docs/screenshots/03-options-audio.jpeg" alt="Audio options screen" width="100%"><br><sub>Audio settings.</sub></td>
<td align="center" width="50%"><img src="docs/screenshots/04-options-graphics.jpeg" alt="Graphics options screen" width="100%"><br><sub>Graphics settings and render-distance controls.</sub></td>
</tr>
<tr>
<td align="center" width="50%"><img src="docs/screenshots/05-options-controls.jpeg" alt="Controls options screen" width="100%"><br><sub>Touchscreen, classic-controls toggle, vibration, and sensitivity settings.</sub></td>
<td align="center" width="50%"><img src="docs/screenshots/06-options-game.jpeg" alt="Game options screen" width="100%"><br><sub>Game settings like difficulty, server visibility, third-person mode, and HUD controls.</sub></td>
</tr>
</table>

### World creation and menus

<table>
<tr>
<td align="center" width="50%"><img src="docs/screenshots/08-create-world-infinite.jpeg" alt="Create world screen with infinite world selected" width="100%"><br><sub>Create a new world with modernized infinite terrain options.</sub></td>
<td align="center" width="50%"><img src="docs/screenshots/09-create-world-classic.jpeg" alt="Create world screen with classic world selected" width="100%"><br><sub>Classic world support is still here too.</sub></td>
</tr>
<tr>
<td align="center" width="50%"><img src="docs/screenshots/07-world-options-infinite.jpeg" alt="Infinite world generation options screen" width="100%"><br><sub>Tune caves, ravines, lakes, springs, tall grass, and biome grass color for infinite worlds.</sub></td>
<td align="center" width="50%"><img src="docs/screenshots/10-select-world.jpeg" alt="World selection screen" width="100%"><br><sub>World selection and create-new flow.</sub></td>
</tr>
</table>

More screenshots and feature callouts will be added as the game keeps improving.

---

## What TruancyCraftPE already has

Current work in this repository includes:

- **Classic and infinite worlds**
  - classic finite 256x256 worlds
  - infinite terrain with chunk streaming
- **Expanded world generation settings**
  - caves toggle
  - ravines toggle
  - water lakes toggle
  - lava lakes toggle
  - water springs toggle
  - lava springs toggle
  - tall grass worldgen toggle
  - biome grass color toggle
  - option to keep a world fully **classic**
- **Grass and foliage improvements**
  - grass added back into world generation instead of flower-only terrain feeling
  - biome-colored grass block tops
  - tall grass generation in supported biomes
- **Improved controls and settings**
  - classic `0.6.1` touch controls toggle
  - modern custom touch controls toggle
  - live control sizing refresh for newer phones
  - render distance option
  - more menu and settings work than stock 0.6.1
  - mobile-oriented tuning options like pixels-per-millimeter
  - custom keyboard bindings without losing the touch-first layout
  - native Java-server inventory and crafting screens, with the hotbar first and item rearrangement
- **Multiplayer and earlier LAN work**
  - native TCPE and Java servers share the Join Game list
  - saved server list support
  - Add Server flow
  - touch Join Game improvements
- **Modern Android build support** — 32-bit and 64-bit ARM
- **Linux desktop support** — x86_64, keyboard/mouse controls and a packaged launcher
- **GLES3 shader rendering on Android and Linux**
- **Java Edition 1.21.11 networking in Rust**
  - native server menus, readable top-left chat, colored MOTDs and server icons
  - a native PE Storage/Menu toggle that remembers your choice
  - one-second server-list refreshes, including offscreen rows
  - native 2x2/3x3 crafting, hotbar-first inventory and whole/half/quick transfers
  - account sign-in settings and player-profile skins
  - work on resource packs, custom models, holograms and modern items
  - Elytra and firework support, with compatibility still being tested
- **Singleplayer fixes and trees**
  - corrected see-through faces along infinite-world chunk boundaries
  - fixed red backgrounds behind Creative inventory item icons
  - large/broad oaks, tall birches and tall spruce, mixed with the smaller trees
- **Touch/UI cleanup for newer phones**
- **Sprinting**
- **Debug/F3-style information improvements**
- **Ongoing bug fixes across the old codebase**

---

## Terrain and worldgen status

The codebase includes support for modernized infinite-world generation pieces such as:

- biome-based terrain generation
- large cave generation
- canyon / ravine generation
- water lakes
- lava lakes
- configurable cave/ravine/lake/spring generation for world creation

If you want the old feel, you can still make a **Classic world**.
If you want expanded terrain, you can make an **Infinite world** and tune the worldgen options.

Chunk rebuilds have been improved, including a fix for see-through chunk edges in infinite worlds. Large branched oaks, broad oaks, tall birch and tall spruce add more variety to new terrain and newly grown trees; existing trees stay as they were.

---

## Multiplayer status

TruancyCraftPE already includes multiplayer restoration work such as:

- LAN game discovery
- Join Game screen updates
- saved server support
- Add Server UI
- server MOTD ping display for saved/discovered servers
- touch multiplayer UX improvements

v0.5.23 adds **Java Edition 1.21.11 server networking in Rust**, while keeping the native TCPE game and touch controls. Server menus, inventory, crafting and chat are being made to feel at home here too.

This is still an alpha and an active area of development. Rust handles the Java networking; the game and renderer are still native TCPE. There is more work to do on server, plugin and resource-pack compatibility, gameplay and performance, especially on phones.

---

## What we are building toward

Our long-term goal is not just to preserve 0.6.1, but to turn it into the best version of itself.

That means work toward:

- better graphics
- better terrain
- better NPCs / mobs / world behavior
- better controls
- better movement feel
- better camera / looking-around feel
- better UI and usability
- better multiplayer
- better controller support
- better performance
- better modding/restoration infrastructure
- continued bug fixing as we dig through this very old game

This is an old game, and a lot of the work is exactly what you would expect from reviving one:
**fix bugs, modernize carefully, keep the feel, repeat**.

---

## Project goals

TruancyCraftPE exists to:

- preserve the original 0.6.1 alpha experience
- keep classic worlds playable and recognizable
- add optional improvements without forcing the game to lose its identity
- make the game work well on modern Android devices and modern systems
- improve multiplayer and LAN support
- expand world generation while still allowing classic behavior
- give the community a real place to collaborate on old MCPE restoration

---

## Contributing

If you care about classic MCPE, you are welcome here.

You can help by:

1. opening issues for bugs and regressions
2. suggesting gameplay, UI, worldgen, or multiplayer improvements
3. testing APKs and reporting what works and what breaks
4. sending pull requests with focused changes

Please include logs, screenshots, device info, or reproduction steps whenever possible.

---

## Building

### Android build

The Android build uses Rust for Java server networking, then the Android NDK for the native GLES3 game. Build both libraries for the same ABI before packaging the APK.

You need Rustup, Python 3, JDK 17 or newer, `ripgrep`, Android SDK platform 34, build-tools 36.0.0 and NDK 26.1.10909125. The commands below run from the repository root on Linux.

```bash
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export ANDROID_NDK_HOME="$ANDROID_SDK_ROOT/ndk/26.1.10909125"
rustup toolchain install nightly-2026-02-03 --profile minimal
rustup target add --toolchain nightly-2026-02-03 \
  aarch64-linux-android armv7-linux-androideabi

# Use armeabi-v7a here for the 32-bit build.
ABI=arm64-v8a
bash tools/build-android-network.sh "$ABI"
"$ANDROID_NDK_HOME/ndk-build" -C project/android -j2 \
  APP_ABI="$ABI" TCPE_GLES3=1 \
  NDK_MODULE_PATH="$PWD/project/lib_projects"
python3 tools/build-android-apk.py --abi "$ABI" --gles3 \
  --output "build/TruancyCraftPE-0.5.25.1-Android-$ABI-development.apk"
```

To build Android 32-bit, set `ABI=armeabi-v7a` and repeat the networking, NDK and packaging steps. Keep each ABI's networking library in `mobile-libs/<ABI>/`.

### Important packaging note

Without a release keystore, the packager makes a **local development build**. It does not have the official Truancy209 signature and cannot update an official installation. Back up your worlds before changing signing keys. The private release key is not included; see [verification and signing details](verification/README.md) for the public certificate and checks.

The current packaging flow uses the SDK's `d8`. The old `dx` workaround was for earlier builds and is not needed for the verified v0.5.23 release build.

### Linux

The Linux download includes the game and its matching networking library. Extract it somewhere writable and start `launch.sh`; worlds and settings stay with that copy. The Linux build is tested against Ubuntu 24.04. A GLES3-capable graphics driver and X11 or XWayland are required. Other distributions need glibc 2.38 or newer, a compatible C++ runtime, GLFW 3, OpenAL, libpng and zlib.

For a source build on Ubuntu 24.04, install the native build dependencies and the same pinned Rust toolchain:

```bash
sudo apt install clang cmake pkg-config libpng-dev zlib1g-dev \
  libopenal-dev libglfw3-dev libgles-dev
rustup toolchain install nightly-2026-02-03 --profile minimal
CXX=clang++ ./build_linux.sh
./build/linux/launch.sh
```

The script builds the Rust networking library and the native GLES3 game together. Keep the supplied assets and dependency notices with the build.

---

## Repository

Active GitHub home:

- https://github.com/Trauncy209/TruancyCraftPE

---

## Final note

TruancyCraftPE is for people who still love old Pocket Edition and want to make it better together.

If you want to help build a stronger, cleaner, community-driven version of classic MCPE, you're in the right place.
