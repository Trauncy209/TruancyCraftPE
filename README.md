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

**v0.5.25.6 · Performance Update** — less wasted rendering work, with the same Pocket Edition feel. Includes the world-save, inventory, mob and native-server stability fixes from 0.5.25.5.

| Your device | Download |
| :--- | :--- |
| Android 64-bit — most newer phones | [64-bit APK](https://github.com/Trauncy209/TruancyCraftPE/releases/download/v0.5.25.6/TruancyCraftPE-0.5.25.6-Android-arm64-v8a.apk) |
| Android 32-bit — older ARM phones | [32-bit APK](https://github.com/Trauncy209/TruancyCraftPE/releases/download/v0.5.25.6/TruancyCraftPE-0.5.25.6-Android-armeabi-v7a.apk) |
| Linux x86_64 — desktop/laptop | [Linux archive](https://github.com/Trauncy209/TruancyCraftPE/releases/download/v0.5.25.6/TruancyCraftPE-0.5.25.6-Linux-x86_64.tar.gz) |

On Linux, extract the full archive and run `launch.sh`. Keep `data` and `lib` beside the game. Android and Linux require **GLES3-capable graphics**.

Official builds use our **Truancy209 release signature**. [Checksums, signing details and past scan reports](verification/README.md) accompany the project. Back up your worlds before updating or changing signing keys.

## What’s here

- Classic finite worlds and infinite terrain, with caves, ravines, lakes, springs and configurable generation.
- Large/broad oaks, tall birches and tall spruce, mixed with the smaller trees.
- Rain, snow, thunderstorms, lightning and weather that saves with the world.
- Classic and modern touch controls, custom keyboard bindings and native inventory/crafting screens.
- Native TCPE multiplayer, saved servers, PNG icons and player counts.
- Java Edition 1.21.11 networking in Rust, with native menus, readable chat, profile skins and resource-pack work.
- GLES3 rendering on Android and Linux, with reduced rendering allocations, uniform overhead and common shader work.

The game and renderer are still native TCPE; Rust handles Java networking. Native multiplayer remains alpha, and **Microsoft login is not working yet**. Username-only Java servers remain supported. Server/plugin/resource-pack compatibility and phone performance are still being tested.

The performance update measured roughly **33% more FPS at 854×480 and 45% at 1080p** in matched Radeon 610M laptop scenes. Those are laptop measurements, not phone FPS claims. [Benchmark settings and checks](docs/PERFORMANCE-2026-11-19.md).

## Native TCPE servers

Our **[server software](https://github.com/Trauncy209/TruancyCraftPE-Server)** runs native TCPE survival. [Download the matching server](https://github.com/Trauncy209/TruancyCraftPE-Server/releases/tag/v0.5.25.6) for Linux x86_64, or use the Docker setup in the server repo.

Come play **TCSMP** at **`45.41.206.239:25566`**, added as **Type: TCPE**. Native TruancyCraftPE survival with infinite Extreme Terrain and custom generation, hosted through Chicago. World administration is restricted to server operators; see the server repo for setup and alpha limitations.

## Screenshots

These show earlier builds; the download above includes the latest fixes.

<details>
<summary>Mobile gameplay, menus and world settings</summary>

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


</details>

## Come say hi

I’m also busy running our cracked Java network, **[ZeusPvP.net](https://www.zeuspvp.net/)**. [Join the Discord](https://discord.gg/zeuspvp), say hi, and mention **TruancyCraftPE**!

If you care about classic MCPE, you’re welcome here. Help by testing builds, reporting bugs with device info/logs/screenshots, or sending focused pull requests. This is an old game: **fix bugs, modernize carefully, keep the feel, repeat**.

<details>
<summary>Build from source</summary>

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
  --output "build/TruancyCraftPE-0.5.25.6-Android-$ABI-development.apk"
```

To build Android 32-bit, set `ABI=armeabi-v7a` and repeat the networking, NDK and packaging steps. Keep each ABI's networking library in `mobile-libs/<ABI>/`.

### Important packaging note

Without a release keystore, the packager makes a **local development build**. It does not have the official Truancy209 signature and cannot update an official installation. Back up your worlds before changing signing keys. The private release key is not included; see [verification and signing details](verification/README.md) for the public certificate and checks.

The packaging flow uses the SDK’s `d8`.

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

The script builds the Rust networking library and the native GLES3 game together. Keep the supplied assets and dependency notices with the build. If CMake cannot find `GLESv2`, install `libgles-dev` on Ubuntu/Debian (`mesa-libGLES-devel` on Fedora or `libglvnd` on Arch); the graphics driver alone does not include the build headers.

</details>
