#!/usr/bin/env bash
#
# Static universal build (arm64 + x86_64) of Csound 7: core + libsndfile +
# libsamplerate, no realtime audio/MIDI backends, no OSC/csbeats/DSSI, no
# separate opcode plugin .dylibs. Everything vendored and built from source
# as static libraries, so the final Csound Studio plugin/standalone has zero
# runtime dependency on a system-installed CsoundLib64.framework - "monolithic"
# as requested: Csound ships baked into the app itself.
#
# Adapted from a proven, already-built-and-tested script for a different
# project (a Max/MSP external, anthonydifuria/csound7-max on GitHub) that
# hit and fixed several real build failures along the way - those fixes are
# carried over here directly rather than re-discovered the hard way:
#   - Apple's bundled bison (2.3) is too old for Csound's grammar: this
#     script requires Homebrew's bison/flex instead.
#   - Csound's own CMake can silently pick up Homebrew's DYNAMIC
#     libsndfile.dylib/libsamplerate.dylib instead of a static one (its
#     find_library() finds the system copy first) - defeating the whole
#     "no separate install" goal. Fixed by building libsndfile and
#     libsamplerate ourselves first and pre-seeding CMake's SndFile_LIBRARY/
#     SndFile_INCLUDE_DIR/SampleRate_LIBRARY/SampleRate_INCLUDE_DIR cache
#     variables so Csound's configure never touches Homebrew's copies.
#   - Homebrew versions of PortAudio/PortMidi/JACK/CoreMIDI/AudioUnit get
#     auto-detected and linked in if not explicitly turned off - not wanted
#     here, since this plugin drives Csound purely through the C API with
#     its own host-implemented audio/MIDI I/O (see PluginProcessor.cpp).
#
# I cannot run this script myself (no macOS/Xcode/CMake toolchain available
# on my end) - if something breaks, paste me the terminal output and I will
# fix the script from the real error, the same way we've been doing with
# the Xcode build errors.
#
# Usage:  cd "scripts" && ./build_csound_static.sh
# Output: build/csound-install/universal/{lib,include}
#   - lib/libCsoundLib64.a, lib/libsndfile.a, lib/libsamplerate.a
#   - include/csound.h and friends

set -euo pipefail

CSOUND_REPO="https://github.com/csound/csound.git"
CSOUND_BRANCH="develop"   # Csound 7 line - no stable 7.x tag yet as of writing;
                          # pin to one (e.g. "7.0.0") here if it exists by the
                          # time you read this, for a reproducible build.
SNDFILE_REPO="https://github.com/libsndfile/libsndfile.git"
SAMPLERATE_REPO="https://github.com/libsndfile/libsamplerate.git"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build"
INSTALL_ROOT="$BUILD_ROOT/csound-install"
ARCHS=(arm64 x86_64)

# ---------------------------------------------------------------------
# 0. toolchain check: need a modern bison (Homebrew), not Apple's ancient
#    2.3. flex is pinned to Homebrew's too, just for consistency.
# ---------------------------------------------------------------------
if ! command -v brew >/dev/null 2>&1; then
  echo "ERROR: Homebrew not found. Install it from https://brew.sh first."
  exit 1
fi

for pkg in bison flex cmake; do
  if ! brew list "$pkg" >/dev/null 2>&1; then
    echo "==> installing $pkg via Homebrew"
    brew install "$pkg"
  fi
done

BISON_BIN="$(brew --prefix bison)/bin/bison"
FLEX_BIN="$(brew --prefix flex)/bin/flex"

if [ ! -x "$BISON_BIN" ]; then
  echo "ERROR: expected bison at $BISON_BIN, not found. Check 'brew info bison'."
  exit 1
fi

echo "==> using bison: $($BISON_BIN --version | head -1)"
echo "==> using flex:   $($FLEX_BIN --version | head -1)"

# ---------------------------------------------------------------------
# helper: clone a repo once
# ---------------------------------------------------------------------
clone_once() {
  local repo="$1" branch="$2" dest="$3"
  if [ ! -d "$dest/.git" ]; then
    echo "==> cloning $repo ($branch)"
    git clone --branch "$branch" --depth 1 "$repo" "$dest"
  fi
}

# ---------------------------------------------------------------------
# 1. libsndfile, static, core formats only (no FLAC/Vorbis/Opus/MP3, so we
#    don't also have to vendor and statically link ogg/vorbis/opus/flac/
#    lame/mpg123 ourselves). Covers WAV/AIFF/AU/CAF and friends, which is
#    what diskin/soundin/fout need for the common case.
# ---------------------------------------------------------------------
SNDFILE_SRC="$BUILD_ROOT/sndfile-static/src"
clone_once "$SNDFILE_REPO" "master" "$SNDFILE_SRC"

build_sndfile_arch() {
  local arch="$1"
  local build_dir="$BUILD_ROOT/sndfile-static/build-$arch"
  local install_dir="$INSTALL_ROOT/$arch-deps/sndfile"
  echo "==> building static libsndfile for $arch"
  cmake -S "$SNDFILE_SRC" -B "$build_dir" -G "Unix Makefiles" \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_dir" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_PROGRAMS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TESTING=OFF \
    -DENABLE_EXTERNAL_LIBS=OFF \
    -DENABLE_MPEG=OFF \
    -DENABLE_CPACK=OFF
  cmake --build "$build_dir" --config Release -j"$(sysctl -n hw.ncpu)"
  cmake --install "$build_dir" --config Release
}

# ---------------------------------------------------------------------
# 2. libsamplerate, static
# ---------------------------------------------------------------------
SAMPLERATE_SRC="$BUILD_ROOT/samplerate-static/src"
clone_once "$SAMPLERATE_REPO" "master" "$SAMPLERATE_SRC"

build_samplerate_arch() {
  local arch="$1"
  local build_dir="$BUILD_ROOT/samplerate-static/build-$arch"
  local install_dir="$INSTALL_ROOT/$arch-deps/samplerate"
  echo "==> building static libsamplerate for $arch"
  cmake -S "$SAMPLERATE_SRC" -B "$build_dir" -G "Unix Makefiles" \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_dir" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DLIBSAMPLERATE_EXAMPLES=OFF \
    -DLIBSAMPLERATE_INSTALL=ON
  cmake --build "$build_dir" --config Release -j"$(sysctl -n hw.ncpu)"
  cmake --install "$build_dir" --config Release
}

# ---------------------------------------------------------------------
# 3. Csound 7, static, pointed at our own static sndfile/samplerate
# ---------------------------------------------------------------------
CSOUND_SRC="$BUILD_ROOT/csound-static/src"
clone_once "$CSOUND_REPO" "$CSOUND_BRANCH" "$CSOUND_SRC"

build_csound_arch() {
  local arch="$1"
  local build_dir="$BUILD_ROOT/csound-static/build-$arch"
  local install_dir="$INSTALL_ROOT/$arch"
  local sndfile_dir="$INSTALL_ROOT/$arch-deps/sndfile"
  local samplerate_dir="$INSTALL_ROOT/$arch-deps/samplerate"

  local sndfile_lib="$sndfile_dir/lib/libsndfile.a"
  local samplerate_lib="$samplerate_dir/lib/libsamplerate.a"

  if [ ! -f "$sndfile_lib" ]; then
    echo "ERROR: expected static libsndfile at $sndfile_lib, not found."
    echo "Contents of $sndfile_dir/lib:"
    ls -la "$sndfile_dir/lib" 2>/dev/null || echo "  (directory does not exist)"
    exit 1
  fi
  if [ ! -f "$samplerate_lib" ]; then
    echo "ERROR: expected static libsamplerate at $samplerate_lib, not found."
    echo "Contents of $samplerate_dir/lib:"
    ls -la "$samplerate_dir/lib" 2>/dev/null || echo "  (directory does not exist)"
    exit 1
  fi

  echo "==> building static Csound for $arch"
  echo "    using SndFile:    $sndfile_lib"
  echo "    using SampleRate: $samplerate_lib"

  cmake -S "$CSOUND_SRC" -B "$build_dir" -G "Unix Makefiles" \
    -DCMAKE_OSX_ARCHITECTURES="$arch" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$install_dir" \
    -DBISON_EXECUTABLE="$BISON_BIN" \
    -DFLEX_EXECUTABLE="$FLEX_BIN" \
    -DSndFile_LIBRARY="$sndfile_lib" \
    -DSndFile_INCLUDE_DIR="$sndfile_dir/include" \
    -DSampleRate_LIBRARY="$samplerate_lib" \
    -DSampleRate_INCLUDE_DIR="$samplerate_dir/include" \
    -DBUILD_STATIC_LIBRARY=ON \
    -DBUILD_PLUGINS=OFF \
    -DUSE_LIBSNDFILE=ON \
    -DUSE_LIBSAMPLERATE=ON \
    -DUSE_CURL=OFF \
    -DUSE_GETTEXT=OFF \
    -DBUILD_UTILITIES=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_INSTALLER=OFF \
    -DBUILD_DOCS=OFF \
    -DUSE_DOUBLE=ON \
    -DFAIL_MISSING=OFF \
    -DUSE_PORTAUDIO=OFF \
    -DUSE_PORTMIDI=OFF \
    -DUSE_JACK=OFF \
    -DUSE_COREMIDI=OFF \
    -DUSE_AUDIOUNIT=OFF \
    -DUSE_IPMIDI=OFF \
    -DUSE_PULSEAUDIO=OFF \
    -DUSE_PIPEWIRE=OFF \
    -DBUILD_OSC_OPCODES=OFF \
    -DBUILD_CSBEATS=OFF \
    -DBUILD_DSSI_OPCODES=OFF

  cmake --build "$build_dir" --config Release -j"$(sysctl -n hw.ncpu)"
  cmake --install "$build_dir" --config Release
}

for arch in "${ARCHS[@]}"; do
  build_sndfile_arch "$arch"
  build_samplerate_arch "$arch"
  build_csound_arch "$arch"
done

# ---------------------------------------------------------------------
# 4. merge everything into a universal build (lipo)
# ---------------------------------------------------------------------
echo "==> merging into a universal build (lipo)"
UNIVERSAL_LIB_DIR="$INSTALL_ROOT/universal/lib"
mkdir -p "$UNIVERSAL_LIB_DIR"

lipo_merge_dir() {
  local label="$1" dir_a="$2" dir_b="$3"
  if [ ! -d "$dir_a" ]; then
    echo "ERROR: $dir_a does not exist, the arm64 build for $label did not install anything there."
    exit 1
  fi
  for LIBFILE in $(cd "$dir_a" && ls *.a 2>/dev/null); do
    if [ -f "$dir_b/$LIBFILE" ]; then
      lipo -create "$dir_a/$LIBFILE" "$dir_b/$LIBFILE" -output "$UNIVERSAL_LIB_DIR/$LIBFILE"
      echo "   universal ($label): $LIBFILE"
    else
      echo "   WARNING: $LIBFILE ($label) only present for arm64, skipping (please check)"
    fi
  done
}

lipo_merge_dir "csound"     "$INSTALL_ROOT/arm64/lib"               "$INSTALL_ROOT/x86_64/lib"
lipo_merge_dir "sndfile"    "$INSTALL_ROOT/arm64-deps/sndfile/lib"    "$INSTALL_ROOT/x86_64-deps/sndfile/lib"
lipo_merge_dir "samplerate" "$INSTALL_ROOT/arm64-deps/samplerate/lib" "$INSTALL_ROOT/x86_64-deps/samplerate/lib"

mkdir -p "$INSTALL_ROOT/universal/include"

# Csound's own CMake install does NOT put headers under our
# CMAKE_INSTALL_PREFIX/include on macOS - it packages itself as a real
# CsoundLib64.framework and installs headers there instead, typically under
# ~/Library/Frameworks/CsoundLib64.framework/Headers. The static .a itself
# DOES land in our own prefix (lib/libCsoundLib64.a, already handled above),
# just not the headers.
CSOUND_FRAMEWORK_HEADERS="$HOME/Library/Frameworks/CsoundLib64.framework/Headers"
if [ ! -d "$CSOUND_FRAMEWORK_HEADERS" ]; then
  echo "ERROR: Csound headers not found at $CSOUND_FRAMEWORK_HEADERS"
  echo "Csound's CMake install must have put them somewhere else on your system -"
  echo "look for csound.h under ~/Library/Frameworks or /Library/Frameworks and"
  echo "adjust CSOUND_FRAMEWORK_HEADERS above."
  exit 1
fi
cp -R "$CSOUND_FRAMEWORK_HEADERS/." "$INSTALL_ROOT/universal/include/"
cp -R "$INSTALL_ROOT/arm64-deps/sndfile/include/." "$INSTALL_ROOT/universal/include/"
cp -R "$INSTALL_ROOT/arm64-deps/samplerate/include/." "$INSTALL_ROOT/universal/include/"

echo ""
echo "Done. Universal static libraries in: $UNIVERSAL_LIB_DIR"
echo "Headers in: $INSTALL_ROOT/universal/include"
echo ""
echo "Check the real .a file names with:"
echo "  ls \"$UNIVERSAL_LIB_DIR\""
echo "and tell me if they don't match libCsoundLib64.a / libsndfile.a /"
echo "libsamplerate.a - Csound.jucer's extraLinkerFlags assumes those names."
