#!/usr/bin/env bash
# Build the in-process Rust connection library for one Android ABI.
set -euo pipefail
port_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
abi="${1:-arm64-v8a}"
api="${TCPE_ANDROID_API:-21}"
toolchain="${TCPE_RUST_TOOLCHAIN:-nightly-2026-02-03}"
ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [[ -z "$ndk" ]]; then echo 'Set ANDROID_NDK_HOME to your installed Android NDK.' >&2; exit 1; fi
case "$abi" in
 arm64-v8a) rust_target=aarch64-linux-android; clang_target=aarch64-linux-android ;;
 armeabi-v7a) rust_target=armv7-linux-androideabi; clang_target=armv7a-linux-androideabi ;;
 x86_64) rust_target=x86_64-linux-android; clang_target=x86_64-linux-android ;;
 *) echo "Unsupported ABI: $abi" >&2; exit 1 ;;
esac
if ! rustup target list --installed --toolchain "$toolchain" | rg -Fxq "$rust_target"; then
 echo "Missing Rust target: $rust_target. Install it with rustup target add --toolchain $toolchain $rust_target." >&2; exit 1
fi
ndk_tools="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin"
linker="$ndk_tools/${clang_target}${api}-clang"
[[ -x "$linker" ]] || { echo "Missing NDK linker: $linker" >&2; exit 1; }
linker_variable="CARGO_TARGET_$(printf '%s' "$rust_target" | tr '[:lower:]-' '[:upper:]_')_LINKER"
cc_variable="CC_${rust_target//-/_}"
ar_variable="AR_${rust_target//-/_}"
android_target_dir="${TCPE_ANDROID_TARGET_DIR:-$port_root/build/android-cargo}"
network_manifest="${TCPE_ANDROID_SOURCE_MANIFEST:-$port_root/rust-network/Cargo.toml}"
library_output="${TCPE_ANDROID_LIBRARY_OUTPUT:-$port_root/mobile-libs}"
env "$linker_variable=$linker" "$cc_variable=$linker" "$ar_variable=$ndk_tools/llvm-ar" \
 CARGO_BUILD_JOBS=2 CARGO_TARGET_DIR="$android_target_dir" \
 RUSTFLAGS='-C link-arg=-Wl,-z,max-page-size=16384' \
 cargo "+$toolchain" rustc --release --locked --lib --target "$rust_target" --manifest-path "$network_manifest" -- -C link-arg=-Wl,-soname,libtcpe_java_network.so
mkdir -p "$library_output/$abi"
cp "$android_target_dir/$rust_target/release/libtcpe_java_network.so" "$library_output/$abi/"
"$ndk_tools/llvm-readelf" -h "$library_output/$abi/libtcpe_java_network.so"
printf 'Built %s\n' "$library_output/$abi/libtcpe_java_network.so"
