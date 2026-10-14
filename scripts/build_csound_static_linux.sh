#!/usr/bin/env bash
#
# Static build of Csound 7 for Linux (Ubuntu/Debian): core + libsndfile +
# libsamplerate, no realtime audio/MIDI backends, no OSC/csbeats/DSSI, no
# separate opcode plugins - the same "monolithic" setup as the macOS/iOS
# scripts (build_csound_static_macos.sh / build_csound_static_ios.sh): apeCsound
# drives Csound through the C API with its own audio/MIDI I/O, and Csound is
# linked statically into the VST3 and the Standalone.
#
# Everything is built with -fPIC (CMAKE_POSITION_INDEPENDENT_CODE): the
# static libraries end up inside the VST3 shared object (.so), which would
# otherwise fail to link ("relocation R_X86_64_32 ... recompile with -fPIC").
#
# Usage (from the project root or the scripts folder):
#   ./scripts/build_csound_static_linux.sh            # install deps with apt + build
#   ./scripts/build_csound_static_linux.sh --no-apt   # build only (deps already installed)
#
# Output (the Projucer LINUX_MAKE exporter points here):
#   build/csound-install/linux/lib/libcsound64.a, libsndfile.a, libsamplerate.a
#   build/csound-install/linux/include/csound.h and friends
#
# Then:
#   1. open apeCsound.jucer in Projucer on Linux and save (creates Builds/LinuxMakefile)
#      - or headless:  Projucer --resave apeCsound.jucer
#   2. cd Builds/LinuxMakefile && make CONFIG=Release -j"$(nproc)"
#   3. VST3:       build/apeCsound.vst3   -> copy to ~/.vst3/
#      Standalone: build/apeCsound

set -euo pipefail

CSOUND_REPO="https://github.com/csound/csound.git"
CSOUND_BRANCH="develop"   # Csound 7 line, same as the macOS/iOS scripts
SNDFILE_REPO="https://github.com/libsndfile/libsndfile.git"
SAMPLERATE_REPO="https://github.com/libsndfile/libsamplerate.git"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_ROOT="$PROJECT_ROOT/build"
INSTALL_ROOT="$BUILD_ROOT/csound-install"
ARCH="$(uname -m)"                          # x86_64 or aarch64
WORK="$BUILD_ROOT/linux-$ARCH"              # sources/builds for this machine
STAGE="$INSTALL_ROOT/linux-$ARCH"           # per-library install prefixes
OUT="$INSTALL_ROOT/linux"                   # flattened lib/ + include/ used by the .jucer
JOBS="$(nproc 2>/dev/null || echo 4)"

INSTALL_DEPS=1
[ "${1:-}" = "--no-apt" ] && INSTALL_DEPS=0

# ---------------------------------------------------------------------
# 0. dependencies (Ubuntu/Debian)
#    - Csound build: compiler, cmake, git, bison, flex
#    - JUCE (to build apeCsound itself with the generated Makefile):
#      ALSA, JACK, X11, Freetype, Fontconfig, GL, curl
# ---------------------------------------------------------------------
if [ "$INSTALL_DEPS" -eq 1 ]; then
  if command -v apt-get >/dev/null 2>&1; then
    echo "==> installing build dependencies with apt (sudo)"
    sudo apt-get update
    sudo apt-get install -y \
      build-essential cmake git pkg-config bison flex \
      libasound2-dev libjack-jackd2-dev \
      libfreetype-dev libfontconfig1-dev \
      libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
      libxrandr-dev libxrender-dev libxi-dev \
      libglu1-mesa-dev mesa-common-dev \
      libcurl4-openssl-dev
  else
    echo "WARNING: apt-get not found - install manually: cmake git bison flex and the JUCE Linux dependencies."
  fi
fi

for tool in cmake git bison flex cc c++; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: '$tool' not found. Run without --no-apt, or install it first."
    exit 1
  fi
done

echo "==> arch:  $ARCH"
echo "==> bison: $(bison --version | head -1)"
echo "==> flex:  $(flex --version | head -1)"

clone_once() {
  local repo="$1" branch="$2" dest="$3"
  if [ ! -d "$dest/.git" ]; then
    echo "==> cloning $repo ($branch)"
    git clone --branch "$branch" --depth 1 "$repo" "$dest"
  fi
}

# Some distributions install static libraries in lib64/ instead of lib/.
libdir_of() {
  if [ -d "$1/lib64" ] && ls "$1/lib64"/*.a >/dev/null 2>&1; then echo "$1/lib64"; else echo "$1/lib"; fi
}

COMMON_CMAKE=(
  -G "Unix Makefiles"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  -DBUILD_SHARED_LIBS=OFF
)

# ---------------------------------------------------------------------
# 1. libsndfile, static, core formats only (WAV/AIFF/AU/CAF...), as on macOS
# ---------------------------------------------------------------------
SNDFILE_SRC="$WORK/sndfile/src"
SNDFILE_PREFIX="$STAGE/sndfile"
clone_once "$SNDFILE_REPO" "master" "$SNDFILE_SRC"

echo "==> building static libsndfile"
cmake -S "$SNDFILE_SRC" -B "$WORK/sndfile/build" "${COMMON_CMAKE[@]}" \
  -DCMAKE_INSTALL_PREFIX="$SNDFILE_PREFIX" \
  -DBUILD_PROGRAMS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF \
  -DENABLE_EXTERNAL_LIBS=OFF \
  -DENABLE_MPEG=OFF \
  -DENABLE_CPACK=OFF
cmake --build "$WORK/sndfile/build" -j"$JOBS"
cmake --install "$WORK/sndfile/build"

# ---------------------------------------------------------------------
# 2. libsamplerate, static
# ---------------------------------------------------------------------
SAMPLERATE_SRC="$WORK/samplerate/src"
SAMPLERATE_PREFIX="$STAGE/samplerate"
clone_once "$SAMPLERATE_REPO" "master" "$SAMPLERATE_SRC"

echo "==> building static libsamplerate"
cmake -S "$SAMPLERATE_SRC" -B "$WORK/samplerate/build" "${COMMON_CMAKE[@]}" \
  -DCMAKE_INSTALL_PREFIX="$SAMPLERATE_PREFIX" \
  -DBUILD_TESTING=OFF \
  -DLIBSAMPLERATE_EXAMPLES=OFF \
  -DLIBSAMPLERATE_INSTALL=ON
cmake --build "$WORK/samplerate/build" -j"$JOBS"
cmake --install "$WORK/samplerate/build"

SNDFILE_LIB="$(libdir_of "$SNDFILE_PREFIX")/libsndfile.a"
SAMPLERATE_LIB="$(libdir_of "$SAMPLERATE_PREFIX")/libsamplerate.a"

for f in "$SNDFILE_LIB" "$SAMPLERATE_LIB"; do
  [ -f "$f" ] || { echo "ERROR: expected $f, not found."; exit 1; }
done

# ---------------------------------------------------------------------
# 3. Csound 7, static, pointed at our own static sndfile/samplerate
#    (never the system ones), every realtime backend off.
# ---------------------------------------------------------------------
CSOUND_SRC="$WORK/csound/src"
CSOUND_PREFIX="$STAGE/csound"
clone_once "$CSOUND_REPO" "$CSOUND_BRANCH" "$CSOUND_SRC"

echo "==> building static Csound"
echo "    SndFile:    $SNDFILE_LIB"
echo "    SampleRate: $SAMPLERATE_LIB"

cmake -S "$CSOUND_SRC" -B "$WORK/csound/build" "${COMMON_CMAKE[@]}" \
  -DCMAKE_INSTALL_PREFIX="$CSOUND_PREFIX" \
  -DSndFile_LIBRARY="$SNDFILE_LIB" \
  -DSndFile_INCLUDE_DIR="$SNDFILE_PREFIX/include" \
  -DSampleRate_LIBRARY="$SAMPLERATE_LIB" \
  -DSampleRate_INCLUDE_DIR="$SAMPLERATE_PREFIX/include" \
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
  -DUSE_ALSA=OFF \
  -DUSE_PULSEAUDIO=OFF \
  -DUSE_PIPEWIRE=OFF \
  -DUSE_IPMIDI=OFF \
  -DUSE_COREMIDI=OFF \
  -DUSE_AUDIOUNIT=OFF \
  -DBUILD_OSC_OPCODES=OFF \
  -DBUILD_CSBEATS=OFF \
  -DBUILD_DSSI_OPCODES=OFF \
  -DINSTALL_PYTHON_INTERFACE=OFF
# INSTALL_PYTHON_INTERFACE=OFF: otherwise "cmake --install" copies ctcsound.py
# into the system Python site-packages (/usr/local/...) -> Permission denied.
#
# Only the static library is built, and there is no "cmake --install": the
# install step also copies the shared lib, the plugins, the csound command and
# (depending on the Csound version) ctcsound.py into the system Python
# site-packages, which fails without root. The library and the public headers
# are staged by hand, like in build_csound_static_windows.ps1.
cmake --build "$WORK/csound/build" -j"$JOBS" --target csound64-static

rm -rf "$CSOUND_PREFIX"
mkdir -p "$CSOUND_PREFIX/lib" "$CSOUND_PREFIX/include/csound"
STATIC_A="$(find "$WORK/csound/build" -name 'libcsound64.a' -type f | head -1)"
[ -n "$STATIC_A" ] || { echo "ERROR: libcsound64.a not found in $WORK/csound/build"; exit 1; }
cp "$STATIC_A" "$CSOUND_PREFIX/lib/libcsound64.a"
cp "$CSOUND_SRC"/include/*.h          "$CSOUND_PREFIX/include/csound/"
cp "$CSOUND_SRC"/include/*.hpp        "$CSOUND_PREFIX/include/csound/" 2>/dev/null || true
cp "$WORK/csound/build"/include/*.h   "$CSOUND_PREFIX/include/csound/"   # version.h, float-version.h

# ---------------------------------------------------------------------
# 4. flatten into build/csound-install/linux/{lib,include}
# ---------------------------------------------------------------------
echo "==> collecting libraries and headers in $OUT"
rm -rf "$OUT"
mkdir -p "$OUT/lib" "$OUT/include"

CSOUND_LIBDIR="$(libdir_of "$CSOUND_PREFIX")"
CSOUND_LIB="$(ls "$CSOUND_LIBDIR"/libcsound64.a 2>/dev/null || ls "$CSOUND_LIBDIR"/libcsound*.a 2>/dev/null | head -1 || true)"

if [ -z "$CSOUND_LIB" ] || [ ! -f "$CSOUND_LIB" ]; then
  echo "ERROR: static Csound library not found in $CSOUND_LIBDIR:"
  ls -la "$CSOUND_LIBDIR" 2>/dev/null || true
  exit 1
fi

cp "$CSOUND_LIB"      "$OUT/lib/libcsound64.a"   # nome atteso dal .jucer (externalLibraries "csound64")
cp "$SNDFILE_LIB"     "$OUT/lib/"
cp "$SAMPLERATE_LIB"  "$OUT/lib/"

# Csound installs its headers in include/csound/ on Linux; the plugin
# includes them as "csound.h", so they are copied flat.
if [ -d "$CSOUND_PREFIX/include/csound" ]; then
  cp -R "$CSOUND_PREFIX/include/csound/." "$OUT/include/"
else
  cp -R "$CSOUND_PREFIX/include/." "$OUT/include/"
fi
cp -R "$SNDFILE_PREFIX/include/."    "$OUT/include/"
cp -R "$SAMPLERATE_PREFIX/include/." "$OUT/include/"

[ -f "$OUT/include/csound.h" ] || { echo "ERROR: csound.h not found in $OUT/include"; exit 1; }

echo ""
echo "Done."
ls -la "$OUT/lib"
echo ""
echo "Next:"
echo "  Projucer --resave \"$PROJECT_ROOT/apeCsound.jucer\""
echo "  cd \"$PROJECT_ROOT/Builds/LinuxMakefile\" && make CONFIG=Release -j$JOBS"
