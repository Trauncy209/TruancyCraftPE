#!/usr/bin/env bash
# ============================================================
# Usage:
#   ./build.sh           # full build (NDK + Java + APK + install)
#   ./build.sh --no-cpp  # skip NDK rebuild (Java/assets changed)
#   ./build.sh --no-java # skip Java recompile (C++ changed only)
#   ./build.sh --no-build # repackage + install only (no recompile)
#
# ABI targeting:
#   ./build.sh --abi arm64-v8a    # build for arm64 only (default)
#   ./build.sh --abi armeabi-v7a  # build for ARMv7 only
#   ./build.sh --abi all          # build for both ABIs (fat APK)
# ============================================================

# lets be strict cuz we are safe like that
# *thanos snap*
set -euo pipefail
IFS=$'\n\t'

########################################
# configuration
########################################
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"

# build output directory (similar to apkbuild in the PS script)
# maybe doing this in build.ps1 would be cleaner, than putting the apkbuild in C:
BUILD_DIR="$REPO_ROOT/build-apk"

# default Android/NDK/SDK paths (can be overridden by env vars)
ANDROID_NDK_PATH="${ANDROID_NDK_PATH:-$HOME/android-ndk-r14b}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
ANDROID_BUILD_TOOLS_VERSION="${ANDROID_BUILD_TOOLS_VERSION:-}"
ANDROID_PLATFORM_API="${ANDROID_PLATFORM_API:-}"

# ABI selection: can be set via --abi flag or MATRIX_ABI env var.
# Supported values: arm64-v8a, armeabi-v7a, all
# MATRIX_ABI takes precedence over the default but is overridden by --abi on the CLI.
TARGET_ABI="${MATRIX_ABI:-arm64-v8a}"

function fail() {
  echo "ERROR: $1" >&2
  exit 1
}

function find_build_tools_dir() {
  if [[ -n "$ANDROID_BUILD_TOOLS_VERSION" ]]; then
    local candidate="$ANDROID_SDK_ROOT/build-tools/$ANDROID_BUILD_TOOLS_VERSION"
    [[ -d "$candidate" ]] && echo "$candidate" && return
  fi

  if [[ ! -d "$ANDROID_SDK_ROOT/build-tools" ]]; then
    fail "Android build-tools not found under $ANDROID_SDK_ROOT/build-tools. Set ANDROID_SDK_ROOT or install Android SDK build-tools."
  fi

  # picking the highest build tools version because its the easiest way rn
  # i guess if it breaks then fuck you
  local best
  best=$(ls -1 "$ANDROID_SDK_ROOT/build-tools" | sort -V | tail -n 1)
  [[ -n "$best" && -d "$ANDROID_SDK_ROOT/build-tools/$best" ]] || \
    fail "No Android build-tools versions found under $ANDROID_SDK_ROOT/build-tools."
  echo "$ANDROID_SDK_ROOT/build-tools/$best"
}

function find_android_platform_dir() {
  if [[ -n "$ANDROID_PLATFORM_API" ]]; then
    local candidate="$ANDROID_SDK_ROOT/platforms/android-$ANDROID_PLATFORM_API"
    [[ -d "$candidate" ]] && echo "$candidate" && return
  fi

  if [[ ! -d "$ANDROID_SDK_ROOT/platforms" ]]; then
    fail "Android platforms not found under $ANDROID_SDK_ROOT/platforms. Install an Android platform."
  fi

  # pick the highest api level installed for now
  # ideally we should be able to build to any api level, but lets keep it simple for now and 
  # just pick the highest one available
  local best
  best=$(ls -1 "$ANDROID_SDK_ROOT/platforms" | grep -E '^android-[0-9]+' | sed 's/android-//' | sort -n | tail -n 1)
  [[ -n "$best" ]] || fail "No Android platforms found under $ANDROID_SDK_ROOT/platforms."
  echo "$ANDROID_SDK_ROOT/platforms/android-$best"
}

ANDROID_BUILD_TOOLS_DIR="$(find_build_tools_dir)"
ANDROID_PLATFORM_DIR="$(find_android_platform_dir)"

KEYSTORE_FILE="$BUILD_DIR/debug.keystore"
PACKAGE_NAME="com.mojang.minecraftpe"

# android tool binaries
AAPT="$ANDROID_BUILD_TOOLS_DIR/aapt"
ZIPALIGN="$ANDROID_BUILD_TOOLS_DIR/zipalign"
APKSIGNER="$ANDROID_BUILD_TOOLS_DIR/apksigner"
DEX_TOOL="$ANDROID_BUILD_TOOLS_DIR/d8"
ADB="${ADB:-$ANDROID_SDK_ROOT/platform-tools/adb}"

# java tool binaries
JAVA_HOME_DEFAULT="${JAVA_HOME:-}"  # may be empty

# prefer javac from the jdk; 
# on some systems /usr/lib/jvm/default points to a JRE only.
# If javac is missing, try to locate a JDK installation.
JAVAC_CMD=""
if command -v javac >/dev/null 2>&1; then
  JAVAC_CMD="$(command -v javac)"
elif [[ -n "$JAVA_HOME_DEFAULT" && -x "$JAVA_HOME_DEFAULT/bin/javac" ]]; then
  JAVAC_CMD="$JAVA_HOME_DEFAULT/bin/javac"
elif [[ -x "/usr/lib/jvm/java-8-openjdk/bin/javac" ]]; then
  JAVAC_CMD="/usr/lib/jvm/java-8-openjdk/bin/javac"
elif [[ -x "/usr/lib/jvm/default/bin/javac" ]]; then
  JAVAC_CMD="/usr/lib/jvm/default/bin/javac"
fi

if [[ -z "$JAVAC_CMD" ]]; then
  fail "javac not found; install a JDK and ensure javac is on PATH"
fi

KEYTOOL=""  # will be detected later

# swource directories
JNI_DIR="$REPO_ROOT/project/android/jni"
JAVA_SRC_DIR="$REPO_ROOT/project/android_java/src"
ANDROID_MANIFEST="$REPO_ROOT/project/android_java/AndroidManifest.xml"
ANDROID_RES_DIR="$REPO_ROOT/project/android_java/res"
DATA_DIR="$REPO_ROOT/data"

# output files: APK names are derived after argument parsing once TARGET_ABI is final.
# see the "resolve APK output filenames" block below.
APK_UNSIGNED=""
APK_ALIGNED=""
APK_SIGNED=""
DEX_OUTPUT="$BUILD_DIR/classes.dex"

# flags parsed from CLI args
NO_CPP=false
NO_JAVA=false
NO_BUILD=false

########################################
# helpers
########################################
function usage() {
  cat <<EOF
Usage: $0 [--no-cpp] [--no-java] [--no-build] [--abi <abi>]

Options:
  --no-cpp         Skip the NDK (C++) build step
  --no-java        Skip the Java build step
  --no-build       Skip the compile steps; just package + install
  --abi <abi>      Target ABI: arm64-v8a (default), armeabi-v7a, or all
                   Can also be set via MATRIX_ABI env var for CI matrix builds.
EOF
  exit 1
}

function log_step() {
  echo -e "\n==> $1"
}

function fail() {
  echo "ERROR: $1" >&2
  exit 1
}

function require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    fail "$1 not found; install it (e.g. 'sudo pacman -S $1')"
  fi
}

# ensure required tools are available early
require_cmd zip
require_cmd unzip

function ensure_dir() {
  mkdir -p "$1"
}

function find_keytool() {
  # first try JAVA_HOME if set
  if [[ -n "$JAVA_HOME_DEFAULT" && -x "$JAVA_HOME_DEFAULT/bin/keytool" ]]; then
    echo "$JAVA_HOME_DEFAULT/bin/keytool"
    return
  fi

  # try common install locations
  if [[ -n "${JAVA_HOME:-}" && -x "${JAVA_HOME}/bin/keytool" ]]; then
    echo "${JAVA_HOME}/bin/keytool"
    return
  fi

  if command -v keytool >/dev/null 2>&1; then
    command -v keytool
    return
  fi

  fail "keytool not found. Set JAVA_HOME or install a JDK."
}

function write_stub_file() {
  local rel_path="$1"
  local content="$2"
  local full_path="$BUILD_DIR/stubs/$rel_path"

  ensure_dir "$(dirname "$full_path")"
  if [[ ! -f "$full_path" ]]; then
    echo -e "$content" > "$full_path"
    echo "  stub: $rel_path"
  fi
}

function build_ndk_abi() {
  local abi="$1"

  # armeabi-v7a needs a few extra NDK flags to get hardware FPU support
  # without APP_ABI the default would be whatever Android.mk says, so we
  # always pass it explicitly so the same Android.mk works for both targets
  local -a extra_flags=( "APP_ABI=$abi" )
  if [[ "$abi" == "armeabi-v7a" ]]; then
    # enable hardware FPU + NEON like the old Minecraft ARMv7 builds used to
    extra_flags+=( "APP_ARM_MODE=arm" "APP_ARM_NEON=true" )
  fi

  echo "  ndk-build for $abi..."
  if ! "$ANDROID_NDK_PATH/ndk-build" \
      NDK_PROJECT_PATH="$REPO_ROOT/project/android" \
      APP_BUILD_SCRIPT="$JNI_DIR/Android.mk" \
      "${extra_flags[@]}" \
      2>&1 | tee "$BUILD_DIR/ndk-build-${abi}.log"; then
    echo "NDK build failed for $abi. See $BUILD_DIR/ndk-build-${abi}.log" >&2
    exit 1
  fi

  ensure_dir "$BUILD_DIR/lib/$abi"
  cp -v "$REPO_ROOT/project/android/libs/$abi/libminecraftpe.so" "$BUILD_DIR/lib/$abi/"
  echo "  .so -> $BUILD_DIR/lib/$abi/libminecraftpe.so"
}

########################################
# argument parsing
########################################
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-cpp) NO_CPP=true ;;
    --no-java) NO_JAVA=true ;;
    --no-build) NO_BUILD=true ;;
    --abi)
      shift
      [[ $# -gt 0 ]] || fail "--abi requires a value (arm64-v8a, armeabi-v7a, all)"
      TARGET_ABI="$1"
      ;;
    -h|--help) usage ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      ;;
  esac
  shift
done

# validate the ABI value now that all args are parsed
case "$TARGET_ABI" in
  arm64-v8a|armeabi-v7a|all) ;;
  *) fail "Unknown ABI '$TARGET_ABI'. Supported values: arm64-v8a, armeabi-v7a, all" ;;
esac

echo "  TARGET_ABI=$TARGET_ABI"

# resolve APK output filenames now that TARGET_ABI is final.
# arm64-v8a -> minecraftpe-v8a-debug.apk
# armeabi-v7a -> minecraftpe-v7a-debug.apk
# all -> minecraftpe-all-debug.apk  (fat APK containing both ABIs)
case "$TARGET_ABI" in
  arm64-v8a)   APK_SUFFIX="v8a" ;;
  armeabi-v7a) APK_SUFFIX="v7a" ;;
  *)           APK_SUFFIX="$TARGET_ABI" ;;
esac
APK_UNSIGNED="$BUILD_DIR/minecraftpe-${APK_SUFFIX}-unsigned.apk"
APK_ALIGNED="$BUILD_DIR/minecraftpe-${APK_SUFFIX}-aligned.apk"
APK_SIGNED="$BUILD_DIR/minecraftpe-${APK_SUFFIX}-debug.apk"

########################################
# validate required tools
########################################
KEYTOOL="$(find_keytool)"

if [[ ! -x "$AAPT" ]]; then
  fail "aapt not found at $AAPT"
fi

if [[ ! -x "$ZIPALIGN" ]]; then
  fail "zipalign not found at $ZIPALIGN"
fi

if [[ ! -x "$APKSIGNER" ]]; then
  fail "apksigner not found at $APKSIGNER"
fi

if [[ ! -x "$DEX_TOOL" ]]; then
  fail "d8 not found at $DEX_TOOL"
fi

if [[ ! -x "$ADB" ]]; then
  fail "adb not found at $ADB"
fi

########################################
# bootstrap
########################################
log_step "Bootstrap"

ensure_dir "$BUILD_DIR"
ensure_dir "$BUILD_DIR/lib/arm64-v8a"
ensure_dir "$BUILD_DIR/lib/armeabi-v7a"
ensure_dir "$BUILD_DIR/gen"
ensure_dir "$BUILD_DIR/stubs"

# create a debug keystore if it doesn't exist
if [[ ! -f "$KEYSTORE_FILE" ]]; then
  echo "  generating debug.keystore..."
  "$KEYTOOL" -genkeypair \
    -keystore "$KEYSTORE_FILE" -storepass android -keypass android \
    -alias androiddebugkey -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
  echo "  keystore created"
else
  echo "  keystore OK"
fi

# why dont we just include the stubs lol
write_stub_file "com/mojang/android/StringValue.java" "package com.mojang.android;\npublic interface StringValue { String getStringValue(); }\n"

write_stub_file "com/mojang/android/licensing/LicenseCodes.java" "package com.mojang.android.licensing;\npublic class LicenseCodes { public static final int LICENSE_OK = 0; }\n"

write_stub_file "com/mojang/android/EditTextAscii.java" "package com.mojang.android;\nimport android.content.Context;\nimport android.text.Editable;\nimport android.text.TextWatcher;\nimport android.util.AttributeSet;\nimport android.widget.EditText;\npublic class EditTextAscii extends EditText implements TextWatcher {\n    public EditTextAscii(Context c) { super(c); addTextChangedListener(this); }\n    public EditTextAscii(Context c, AttributeSet a) { super(c,a); addTextChangedListener(this); }\n    public EditTextAscii(Context c, AttributeSet a, int d) { super(c,a,d); addTextChangedListener(this); }\n    @Override public void onTextChanged(CharSequence s,int st,int b,int co){}\n    public void beforeTextChanged(CharSequence s,int st,int co,int aft){}\n    public void afterTextChanged(Editable e){\n        String s=e.toString(),san=sanitize(s);\n        if(!s.equals(san))e.replace(0,e.length(),san);\n    }\n    static public String sanitize(String s){\n        StringBuilder sb=new StringBuilder();\n        for(int i=0;i<s.length();i++){char c=s.charAt(i);if(c<128)sb.append(c);}\n        return sb.toString();\n    }\n}\n"

write_stub_file "com/mojang/android/preferences/SliderPreference.java" "package com.mojang.android.preferences;\nimport android.content.Context;\nimport android.content.res.Resources;\nimport android.preference.DialogPreference;\nimport android.util.AttributeSet;\nimport android.view.Gravity;\nimport android.view.View;\nimport android.widget.LinearLayout;\nimport android.widget.SeekBar;\nimport android.widget.TextView;\npublic class SliderPreference extends DialogPreference implements SeekBar.OnSeekBarChangeListener {\n    private static final String NS=\"http://schemas.android.com/apk/res/android\";\n    private Context _ctx; private TextView _tv; private SeekBar _sb;\n    private String _suf; private int _def,_max,_val,_min;\n    public SliderPreference(Context ctx,AttributeSet a){\n        super(ctx,a); _ctx=ctx;\n        _suf=gStr(a,NS,\"text\",\"\"); _def=gInt(a,NS,\"defaultValue\",0);\n        _max=gInt(a,NS,\"max\",100); _min=gInt(a,null,\"min\",0);\n        setDefaultValue(_def);\n    }\n    @Override protected View onCreateDialogView(){\n        LinearLayout l=new LinearLayout(_ctx); l.setOrientation(LinearLayout.VERTICAL); l.setPadding(6,6,6,6);\n        _tv=new TextView(_ctx); _tv.setGravity(Gravity.CENTER_HORIZONTAL); _tv.setTextSize(32);\n        l.addView(_tv,new LinearLayout.LayoutParams(-1,-2));\n        _sb=new SeekBar(_ctx); _sb.setOnSeekBarChangeListener(this);\n        l.addView(_sb,new LinearLayout.LayoutParams(-1,-2));\n        if(shouldPersist())_val=getPersistedInt(_def);\n        _sb.setMax(_max); _sb.setProgress(_val); return l;\n    }\n    @Override protected void onSetInitialValue(boolean r,Object d){\n        super.onSetInitialValue(r,d);\n        _val=r?(shouldPersist()?getPersistedInt(_def):0):(Integer)d;\n    }\n    public void onProgressChanged(SeekBar s,int v,boolean f){\n        _val=v+_min; _tv.setText(_val+_suf);\n        if(shouldPersist())persistInt(_val); callChangeListener(Integer.valueOf(_val));\n    }\n    public void onStartTrackingTouch(SeekBar s){}\n    public void onStopTrackingTouch(SeekBar s){}\n    private int gInt(AttributeSet a,String ns,String n,int d){int id=a.getAttributeResourceValue(ns,n,0);return id!=0?getContext().getResources().getInteger(id):a.getAttributeIntValue(ns,n,d);}\n    private String gStr(AttributeSet a,String ns,String n,String d){int id=a.getAttributeResourceValue(ns,n,0);if(id!=0)return getContext().getResources().getString(id);String v=a.getAttributeValue(ns,n);return v!=null?v:d;}\n}\n"

write_stub_file "com/mojang/minecraftpe/MainMenuOptionsActivity.java" "package com.mojang.minecraftpe;\nimport android.app.Activity;\npublic class MainMenuOptionsActivity extends Activity {\n    public static final String Internal_Game_DifficultyPeaceful=\"internal_game_difficulty_peaceful\";\n    public static final String Game_DifficultyLevel=\"game_difficulty\";\n    public static final String Controls_Sensitivity=\"controls_sensitivity\";\n}\n"

write_stub_file "com/mojang/minecraftpe/Minecraft_Market.java" "package com.mojang.minecraftpe;\nimport android.app.Activity; import android.content.Intent; import android.os.Bundle;\npublic class Minecraft_Market extends Activity {\n    @Override protected void onCreate(Bundle s){super.onCreate(s);startActivity(new Intent(this,MainActivity.class));finish();}\n}\n"

write_stub_file "com/mojang/minecraftpe/Minecraft_Market_Demo.java" "package com.mojang.minecraftpe;\nimport android.content.Intent; import android.net.Uri;\npublic class Minecraft_Market_Demo extends MainActivity {\n    @Override public void buyGame(){startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(\"market://details?id=com.mojang.minecraftpe\")));}\n    @Override protected boolean isDemo(){return true;}\n}\n"

write_stub_file "com/mojang/minecraftpe/GameModeButton.java" "package com.mojang.minecraftpe;\nimport com.mojang.android.StringValue;\nimport android.content.Context; import android.util.AttributeSet;\nimport android.view.View; import android.view.View.OnClickListener;\nimport android.widget.TextView; import android.widget.ToggleButton;\npublic class GameModeButton extends ToggleButton implements OnClickListener,StringValue {\n    static final int Creative=0,Survival=1;\n    private int _type=0; private boolean _attached=false;\n    public GameModeButton(Context c,AttributeSet a){super(c,a);setOnClickListener(this);}\n    public void onClick(View v){_update();}\n    @Override protected void onFinishInflate(){super.onFinishInflate();_update();}\n    @Override protected void onAttachedToWindow(){if(!_attached){_update();_attached=true;}}\n    private void _update(){_set(isChecked()?Survival:Creative);}\n    private void _set(int i){\n        _type=i<Creative?Creative:(i>Survival?Survival:i);\n        int id=_type==Survival?R.string.gamemode_survival_summary:R.string.gamemode_creative_summary;\n        String desc=getContext().getString(id);\n        View v=getRootView().findViewById(R.id.labelGameModeDesc);\n        if(desc!=null&&v instanceof TextView)((TextView)v).setText(desc);\n    }\n    public String getStringValue(){return new String[]{\"creative\",\"survival\"}[_type];}\n    static public String getStringForType(int i){int c=i<Creative?Creative:(i>Survival?Survival:i);return new String[]{\"creative\",\"survival\"}[c];}\n}\n"

echo "  stubs OK"

########################################
# ndk build
########################################
if [[ "$NO_CPP" == false && "$NO_BUILD" == false ]]; then
  log_step "NDK build ($TARGET_ABI)"

  # the original windows build script used a junction to avoid long paths here
  # on linux, path lengths are *usually* fine, but we still keep things simple
  pushd "$JNI_DIR" >/dev/null

  export NDK_MODULE_PATH="$REPO_ROOT/project/lib_projects"

  # build each requested ABI by delegating to build_ndk_abi()
  if [[ "$TARGET_ABI" == "all" ]]; then
    build_ndk_abi "arm64-v8a"
    build_ndk_abi "armeabi-v7a"
  else
    build_ndk_abi "$TARGET_ABI"
  fi

  popd >/dev/null
fi

########################################
# java compile
########################################
if [[ "$NO_JAVA" == false && "$NO_BUILD" == false ]]; then
  log_step "Java compile"

  ensure_dir "$(dirname "$BUILD_DIR/gen/R.java")"

  # generate R.java
  "$AAPT" package -f -M "$ANDROID_MANIFEST" -S "$ANDROID_RES_DIR" -I "$ANDROID_PLATFORM_DIR/android.jar" -J "$BUILD_DIR/gen" -F "$BUILD_DIR/_rgen.apk"
  rm -f "$BUILD_DIR/_rgen.apk"

  # collect all source files (project + stubs + generated R.java)
  JAVA_SOURCES=(
    $(find "$JAVA_SRC_DIR" -name "*.java" -print)
    $(find "$BUILD_DIR/stubs" -name "*.java" -print)
    "$BUILD_DIR/gen/R.java"
  )

  rm -rf "$BUILD_DIR/classes"
  ensure_dir "$BUILD_DIR/classes"

  # Some JDK versions (<=8) don't support --release.
  JAVAC_ARGS=(--release 8 -parameters)
  if "$JAVAC_CMD" -version 2>&1 | grep -qE '^javac 1\.'; then
    JAVAC_ARGS=(-source 1.8 -target 1.8 -parameters)
  fi

  "$JAVAC_CMD" "${JAVAC_ARGS[@]}" -cp "$ANDROID_PLATFORM_DIR/android.jar" -d "$BUILD_DIR/classes" "${JAVA_SOURCES[@]}"
  echo "  javac OK"

  # convert class files into dex
  JAVA_CLASS_FILES=( $(find "$BUILD_DIR/classes" -name "*.class" -print) )
  "$DEX_TOOL" --min-api 21 --output "$BUILD_DIR" "${JAVA_CLASS_FILES[@]}"
  echo "  d8 -> $DEX_OUTPUT"
fi

########################################
# package apk
########################################
log_step "Package APK"

rm -f "$APK_UNSIGNED" "$APK_ALIGNED" "$APK_SIGNED"

"$AAPT" package -f -M "$ANDROID_MANIFEST" -S "$ANDROID_RES_DIR" -I "$ANDROID_PLATFORM_DIR/android.jar" -F "$APK_UNSIGNED"

# add classes.dex and native library/libraries into apk.
# when building for "all" we pack both ABIs into the same APK so Android can
# pick the right one at install time (fat APK). for a single-ABI build we
# only include the one .so that was actually compiled.
pushd "$BUILD_DIR" >/dev/null
zip -q "$APK_UNSIGNED" "classes.dex"
if [[ "$TARGET_ABI" == "all" ]]; then
  zip -q "$APK_UNSIGNED" "lib/arm64-v8a/libminecraftpe.so"
  zip -q "$APK_UNSIGNED" "lib/armeabi-v7a/libminecraftpe.so"
else
  zip -q "$APK_UNSIGNED" "lib/$TARGET_ABI/libminecraftpe.so"
fi
popd >/dev/null

# add assets from data/ directory into the apk under assets/
TMP_ASSETS_DIR="$(mktemp -d)"
mkdir -p "$TMP_ASSETS_DIR/assets"
cp -r "$DATA_DIR/." "$TMP_ASSETS_DIR/assets/"
pushd "$TMP_ASSETS_DIR" >/dev/null
zip -q -r "$APK_UNSIGNED" assets
popd >/dev/null
rm -rf "$TMP_ASSETS_DIR"

"$ZIPALIGN" -p 4 "$APK_UNSIGNED" "$APK_ALIGNED"
if [[ -n "${TCPE_RELEASE_KEYSTORE:-}" ]]; then
  : "${TCPE_RELEASE_STOREPASS_FILE:?Set the private release password-file path}"
  "$APKSIGNER" sign --ks "$TCPE_RELEASE_KEYSTORE" --ks-key-alias "${TCPE_RELEASE_KEY_ALIAS:-truancy-release}" --ks-pass "file:$TCPE_RELEASE_STOREPASS_FILE" --out "$APK_SIGNED" "$APK_ALIGNED"
else
  "$APKSIGNER" sign --ks "$KEYSTORE_FILE" --ks-pass pass:android --key-pass pass:android --out "$APK_SIGNED" "$APK_ALIGNED"
fi
"$APKSIGNER" verify --verbose "$APK_SIGNED"

echo "  signed -> $APK_SIGNED"

########################################
# install
########################################
if [[ "${TCPE_SKIP_INSTALL:-0}" == "1" ]]; then
  echo "Packaging complete; device installation skipped."
  exit 0
fi

log_step "Install"

"$ADB" shell am force-stop "$PACKAGE_NAME" || true
"$ADB" uninstall "$PACKAGE_NAME" || true
"$ADB" install --no-incremental "$APK_SIGNED"

echo -e "\nDone. Enjoy MCPE 0.6.1 on your device!"