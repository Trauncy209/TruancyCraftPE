#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: TCPE_RELEASE_KEYSTORE=/private/release.p12 TCPE_RELEASE_STOREPASS_FILE=/private/password $0 input.apk output.apk" >&2
  exit 2
fi
: "${TCPE_RELEASE_KEYSTORE:?Set the private release keystore path}"
: "${TCPE_RELEASE_STOREPASS_FILE:?Set the private password-file path}"
tools_dir="${ANDROID_BUILD_TOOLS_DIR:-${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}/build-tools/34.0.0}"
key_alias="${TCPE_RELEASE_KEY_ALIAS:-truancy-release}"
aligned="$(mktemp --suffix=.apk)"
trap 'rm -f "$aligned"' EXIT
"$tools_dir/zipalign" -f 4 "$1" "$aligned"
"$tools_dir/apksigner" sign --ks "$TCPE_RELEASE_KEYSTORE" --ks-key-alias "$key_alias" --ks-pass "file:$TCPE_RELEASE_STOREPASS_FILE" --out "$2" "$aligned"
"$tools_dir/apksigner" verify --verbose --print-certs "$2"
sha256sum "$2" > "$2.sha256"
