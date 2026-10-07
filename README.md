# TruancyCraftPE

TruancyCraftPE is a community restoration and expansion of **Minecraft: Pocket Edition 0.6.1 alpha**.

This project is about keeping the soul of old MCPE while pushing it forward on modern devices.

**Our philosophy:** this should be a **community-made Minecraft, not corporate slop**.

We want to preserve what made early Pocket Edition special, fix what was broken, and steadily build a better version of it in public.

[![Download Latest Release](https://img.shields.io/badge/Download-Latest%20Release-2ea44f?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/releases/latest)
[![Join the Discord](https://img.shields.io/badge/Join%20the%20Discord-5865F2?style=for-the-badge&logo=discord&logoColor=white)](https://discord.gg/pnN65AvKu)
[![Release](https://img.shields.io/github/v/release/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Trauncy209/TruancyCraftPE/total?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/releases)
[![Stars](https://img.shields.io/github/stars/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/stargazers)
[![Issues](https://img.shields.io/github/issues/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/issues)
[![Last Commit](https://img.shields.io/github/last-commit/Trauncy209/TruancyCraftPE?style=for-the-badge)](https://github.com/Trauncy209/TruancyCraftPE/commits/main)

**Current release: [v0.5.22.1](https://github.com/Trauncy209/TruancyCraftPE/releases/tag/v0.5.22.1)** · [32-bit and 64-bit APKs](https://github.com/Trauncy209/TruancyCraftPE/releases/tag/v0.5.22.1) · [Checksums & scans](verification/README.md)

**Join the community:** https://discord.gg/pnN65AvKu

If you want updates as they drop, **star the repo, join the Discord, and turn on notifications/watch it** — releases are frequent and you do not want to miss out.


## Come say hi

I'm also really busy running our cracked Java network, **[ZeusPvP.net](https://www.zeuspvp.net/)**. Feel free to [join our community](https://discord.gg/zeuspvp), say hi, and mention **TruancyCraftPE**!

**Minecraft server:** `ZeusPvP.net` · **Discord:** [discord.gg/zeuspvp](https://discord.gg/zeuspvp)

## Signed releases

Official APKs now use our own **Truancy209 release certificate**. v0.5.22.1 keeps the v0.5.22 game files unchanged and replaces debug signing. [Hashes, the public certificate, and VirusTotal links](verification/README.md) are published with the downloads.

Coming from an older build? [Read the signing update notes](docs/RELEASE-SIGNING.md) before replacing an installation so your worlds stay safe.

## Screenshots

Here is what TruancyCraftPE looks like right now on-device.

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
- **Multiplayer and LAN work**
  - LAN discovery
  - saved server list support
  - Add Server flow
  - touch Join Game improvements
- **Modern Android build support**
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

---

## Multiplayer status

TruancyCraftPE already includes multiplayer restoration work such as:

- LAN game discovery
- Join Game screen updates
- saved server support
- Add Server UI
- server MOTD ping display for saved/discovered servers
- touch multiplayer UX improvements

This is still an active area of development, especially around compatibility and protocol cleanup.

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

The current Android build flow uses `build.sh`.

Example full build:

```bash
ANDROID_SDK_ROOT=/home/Alkaline/Android/Sdk \
ANDROID_NDK_PATH=/home/Alkaline/Android/Sdk/ndk/26.1.10909125 \
ADB=/usr/bin/true \
bash ./build.sh --abi arm64-v8a
```

Useful variants:

```bash
# skip Java rebuild
bash ./build.sh --abi arm64-v8a --no-java

# skip C++ rebuild
bash ./build.sh --abi arm64-v8a --no-cpp

# package only
bash ./build.sh --abi arm64-v8a --no-java --no-cpp
```

### Important packaging note

On this project, the Android `d8` / `r8` step can crash on:

- `build-apk/classes/com/mojang/minecraftpe/MainActivity$1.class`

If that happens, the current working fallback is:

```bash
/home/Alkaline/Android/Sdk/build-tools/30.0.3/dx \
  --dex --output=build-apk/classes.dex build-apk/classes

ANDROID_SDK_ROOT=/home/Alkaline/Android/Sdk \
ANDROID_NDK_PATH=/home/Alkaline/Android/Sdk/ndk/26.1.10909125 \
ADB=/usr/bin/true \
bash ./build.sh --abi arm64-v8a --no-java --no-cpp
```

---

## Repository

Active GitHub home:

- https://github.com/Trauncy209/TruancyCraftPE

---

## Final note

TruancyCraftPE is for people who still love old Pocket Edition and want to make it better together.

If you want to help build a stronger, cleaner, community-driven version of classic MCPE, you're in the right place.
