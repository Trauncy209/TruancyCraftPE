#!/usr/bin/env bash
# Build the native Linux GLES3 game and its in-process Java networking library.
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TOOLCHAIN="${TCPE_RUST_TOOLCHAIN:-nightly-2026-02-03}"
RUST_BUILD="${CARGO_TARGET_DIR:-$REPO/build/rust}"
NATIVE_BUILD="${TCPE_LINUX_BUILD_DIR:-$REPO/build/linux}"
for tool in cargo cmake; do
    command -v "$tool" >/dev/null || { echo "Missing $tool. See README.md for build requirements." >&2; exit 1; }
done
CARGO_TARGET_DIR="$RUST_BUILD" cargo "+$TOOLCHAIN" build \
    --manifest-path "$REPO/rust-network/Cargo.toml" --release --locked \
    --features java-network -j "${TCPE_RUST_JOBS:-1}"
cmake -S "$REPO" -B "$NATIVE_BUILD" -DCMAKE_BUILD_TYPE=Release \
    -DTCPE_GLES3=ON -DTCPE_USE_SYSTEM_LIBS=ON \
    -DTCPE_JAVA_NETWORK_LIBRARY="$RUST_BUILD/release/libtcpe_java_network.so"
cmake --build "$NATIVE_BUILD" --parallel "${TCPE_BUILD_JOBS:-2}"
mkdir -p "$NATIVE_BUILD/lib"
cp "$RUST_BUILD/release/libtcpe_java_network.so" "$NATIVE_BUILD/lib/"
cat > "$NATIVE_BUILD/launch.sh" <<'LAUNCH'
#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export TCPE_JAVA_AUTH_DIR="$PWD/private-auth"
exec ./MinecraftPE "$@"
LAUNCH
chmod +x "$NATIVE_BUILD/launch.sh"
printf 'Built TCPE. Run: %s/launch.sh\n' "$NATIVE_BUILD"
