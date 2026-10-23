#!/usr/bin/env bash
#
# Static iOS build of Csound 7 + libsndfile + libsamplerate, for the
# XCODE_IPHONE exporter in apeCsound.jucer. Companion of
# build_csound_static_macos.sh (macOS): same sources, same CMake options, but
# cross-compiled with the iOS toolchain. Run the macOS script FIRST - it
# clones the sources and installs the headers that the iOS target shares
# (build/csound-install/universal/include).
#
# Output (what the .jucer expects, "$(PLATFORM_NAME)" resolved by Xcode):
#   build/csound-install/ios/iphoneos/lib/{libCsoundLib64,libsndfile,libsamplerate,liblo,libcsoundosc}.a   (arm64 device)
#   build/csound-install/ios/iphonesimulator/lib/...                                      (arm64 + x86_64 simulator, lipo)
#
# Csound's sources guard the iOS-unavailable calls (system(), fork()...)
# with "#if defined(IOS)" - a C macro that Csound's own iOS toolchain file
# defines and CMake's generic iOS support does not: hence -DIOS=1 below
# (CMake's IOS *variable* is set automatically by CMAKE_SYSTEM_NAME=iOS).
#
# I cannot run this script myself (no macOS toolchain here): if Csound's
# CMake complains about a try_run() while cross-compiling or about a
# missing feature, paste the output and I will adjust the options.
#
# Usage:  cd scripts && ./build_csound_static_ios.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build"
INSTALL_ROOT="$BUILD_ROOT/csound-install"
IOS_ROOT="$INSTALL_ROOT/ios"
IOS_MIN="14.0"

SNDFILE_SRC="$BUILD_ROOT/sndfile-static/src"
SAMPLERATE_SRC="$BUILD_ROOT/samplerate-static/src"
CSOUND_SRC="$BUILD_ROOT/csound-static/src"
LIBLO_SRC="$BUILD_ROOT/liblo-static/src"   # OSC (vedi build_csound_static_macos.sh, sezione 3b)

for d in "$SNDFILE_SRC" "$SAMPLERATE_SRC" "$CSOUND_SRC" "$LIBLO_SRC"; do
  if [ ! -d "$d" ]; then
    echo "ERROR: $d not found - run ./build_csound_static_macos.sh (macOS) first, it clones the sources."
    exit 1
  fi
done

BISON_BIN="$(brew --prefix bison)/bin/bison"
FLEX_BIN="$(brew --prefix flex)/bin/flex"
NCPU="$(sysctl -n hw.ncpu)"

# OSC statico: plugin "osc" di Csound come libreria statica (patch condivisa).
python3 "$SCRIPT_DIR/patch_csound_static_osc.py" "$CSOUND_SRC"

# platform = iphoneos | iphonesimulator ; arch = arm64 | x86_64
build_one() {
  local platform="$1" arch="$2"
  local tag="$platform-$arch"
  local sysroot
  sysroot="$(xcrun --sdk "$platform" --show-sdk-path)"
  local deps="$IOS_ROOT/$tag-deps"
  local common=(
    -G "Unix Makefiles"
    -DCMAKE_SYSTEM_NAME=iOS
    -DCMAKE_OSX_SYSROOT="$sysroot"
    -DCMAKE_OSX_ARCHITECTURES="$arch"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$IOS_MIN"
    -DCMAKE_BUILD_TYPE=Release
    -DBUILD_SHARED_LIBS=OFF
    # no try_run() on a cross build: CMake can't execute iOS binaries
    -DCMAKE_CROSSCOMPILING=ON
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY
  )

  echo "==> libsndfile $tag"
  cmake -S "$SNDFILE_SRC" -B "$BUILD_ROOT/sndfile-static/build-$tag" "${common[@]}" \
    -DCMAKE_INSTALL_PREFIX="$deps/sndfile" \
    -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
    -DENABLE_EXTERNAL_LIBS=OFF -DENABLE_MPEG=OFF -DENABLE_CPACK=OFF
  cmake --build "$BUILD_ROOT/sndfile-static/build-$tag" --config Release -j"$NCPU"
  cmake --install "$BUILD_ROOT/sndfile-static/build-$tag" --config Release

  echo "==> libsamplerate $tag"
  cmake -S "$SAMPLERATE_SRC" -B "$BUILD_ROOT/samplerate-static/build-$tag" "${common[@]}" \
    -DCMAKE_INSTALL_PREFIX="$deps/samplerate" \
    -DBUILD_TESTING=OFF -DLIBSAMPLERATE_EXAMPLES=OFF
  cmake --build "$BUILD_ROOT/samplerate-static/build-$tag" --config Release -j"$NCPU"
  cmake --install "$BUILD_ROOT/samplerate-static/build-$tag" --config Release

  echo "==> liblo $tag"
  cmake -S "$LIBLO_SRC/cmake" -B "$BUILD_ROOT/liblo-static/build-$tag" "${common[@]}" \
    -DCMAKE_INSTALL_PREFIX="$deps/liblo" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DWITH_STATIC=ON -DWITH_TOOLS=OFF -DWITH_TESTS=OFF -DWITH_EXAMPLES=OFF -DWITH_CPP_TESTS=OFF \
    -DTHREADING=ON
  cmake --build "$BUILD_ROOT/liblo-static/build-$tag" --config Release -j"$NCPU"
  cmake --install "$BUILD_ROOT/liblo-static/build-$tag" --config Release
  local lo_a
  lo_a="$(find "$deps/liblo" "$BUILD_ROOT/liblo-static/build-$tag" -name 'liblo*.a' -type f | head -1)"
  [ -n "$lo_a" ] || { echo "ERROR: static liblo not found for $tag"; exit 1; }
  mkdir -p "$deps/liblo/lib"
  [ "$lo_a" = "$deps/liblo/lib/liblo.a" ] || cp "$lo_a" "$deps/liblo/lib/liblo.a"
  if [ ! -f "$deps/liblo/include/lo/lo.h" ]; then
    mkdir -p "$deps/liblo/include/lo"
    cp "$LIBLO_SRC"/lo/*.h "$deps/liblo/include/lo/"
    find "$BUILD_ROOT/liblo-static/build-$tag" -path '*lo/*.h' -exec cp {} "$deps/liblo/include/lo/" \;
  fi

  echo "==> Csound $tag"
  cmake -S "$CSOUND_SRC" -B "$BUILD_ROOT/csound-static/build-$tag" "${common[@]}" \
    -DCMAKE_INSTALL_PREFIX="$IOS_ROOT/$tag" \
    -DBISON_EXECUTABLE="$BISON_BIN" \
    -DFLEX_EXECUTABLE="$FLEX_BIN" \
    -DCMAKE_C_FLAGS="-DIOS=1" \
    -DCMAKE_CXX_FLAGS="-DIOS=1" \
    -DSndFile_LIBRARY="$deps/sndfile/lib/libsndfile.a" \
    -DSndFile_INCLUDE_DIR="$deps/sndfile/include" \
    -DSampleRate_LIBRARY="$deps/samplerate/lib/libsamplerate.a" \
    -DSampleRate_INCLUDE_DIR="$deps/samplerate/include" \
    -DBUILD_STATIC_LIBRARY=ON \
    -DBUILD_PLUGINS=OFF \
    -DUSE_LIBSNDFILE=ON \
    -DUSE_LIBSAMPLERATE=ON \
    -DUSE_CURL=OFF -DUSE_GETTEXT=OFF \
    -DBUILD_UTILITIES=OFF -DBUILD_TESTS=OFF -DBUILD_INSTALLER=OFF -DBUILD_DOCS=OFF \
    -DUSE_DOUBLE=ON -DFAIL_MISSING=OFF \
    -DUSE_PORTAUDIO=OFF -DUSE_PORTMIDI=OFF -DUSE_JACK=OFF -DUSE_COREMIDI=OFF -DUSE_AUDIOUNIT=OFF \
    -DUSE_IPMIDI=OFF -DUSE_PULSEAUDIO=OFF -DUSE_PIPEWIRE=OFF \
    -DBUILD_OSC_OPCODES=ON -DAPECSOUND_STATIC_PLUGINS=ON \
    -DLibLO_LIBRARY="$deps/liblo/lib/liblo.a" -DLibLO_INCLUDE_DIR="$deps/liblo/include" \
    -DBUILD_CSBEATS=OFF -DBUILD_DSSI_OPCODES=OFF
  # Solo la libreria statica: i frontend (csound-bin, csdebugger) e il
  # plugin "deprecated" non servono e su iOS non compilano/linkano
  # (cercano csound.h fuori dall'albero, producono .dylib). Niente
  # "cmake --install" (costruirebbe tutto): la .a si copia dal build dir.
  cmake --build "$BUILD_ROOT/csound-static/build-$tag" --config Release -j"$NCPU" --target CsoundLib64-static osc
  mkdir -p "$IOS_ROOT/$tag/lib"
  cp "$BUILD_ROOT/csound-static/build-$tag/libCsoundLib64.a" "$IOS_ROOT/$tag/lib/libCsoundLib64.a"

  # Opcode OSC statici (libosc.a, vedi patch_csound_static_osc.py)
  local osc_a
  osc_a="$(find "$BUILD_ROOT/csound-static/build-$tag" -name 'libosc.a' -type f | head -1)"
  [ -n "$osc_a" ] || { echo "ERROR: libosc.a not found for $tag (is liblo found?)"; exit 1; }
  mkdir -p "$deps/csoundosc/lib"
  cp "$osc_a" "$deps/csoundosc/lib/libcsoundosc.a"
}

build_one iphoneos        arm64
build_one iphonesimulator arm64
build_one iphonesimulator x86_64

# ---------------------------------------------------------------------
# collect: device as is, simulator merged with lipo
# ---------------------------------------------------------------------
collect_lib() {
  local platform="$1" name="$2" subdir="$3"   # subdir: "" for csound, "-deps/sndfile" etc.
  local out="$IOS_ROOT/$platform/lib"
  mkdir -p "$out"

  if [ "$platform" = "iphoneos" ]; then
    cp "$IOS_ROOT/iphoneos-arm64$subdir/lib/$name" "$out/$name"
  else
    lipo -create "$IOS_ROOT/iphonesimulator-arm64$subdir/lib/$name" \
                 "$IOS_ROOT/iphonesimulator-x86_64$subdir/lib/$name" \
         -output "$out/$name"
  fi
  echo "   $platform: $name"
}

for platform in iphoneos iphonesimulator; do
  collect_lib "$platform" libCsoundLib64.a ""
  collect_lib "$platform" libsndfile.a     "-deps/sndfile"
  collect_lib "$platform" libsamplerate.a  "-deps/samplerate"
  collect_lib "$platform" liblo.a          "-deps/liblo"
  collect_lib "$platform" libcsoundosc.a   "-deps/csoundosc"
done

echo ""
echo "Done. iOS static libraries in:"
echo "  $IOS_ROOT/iphoneos/lib"
echo "  $IOS_ROOT/iphonesimulator/lib"
echo "Headers: shared with macOS in $INSTALL_ROOT/universal/include"
