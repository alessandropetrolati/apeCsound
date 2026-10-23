# apeCsound

**apeCsound** is an audio plugin (VST3) and standalone app that runs [Csound](https://csound.com) code live inside your DAW. You write or load a `.csd` file in the built-in editor, press **Apply**, and the engine recompiles it in place. Csound channels can be exposed as automatable DAW parameters.

Developed by **Alessandro Petrolati** — [apeSoft](https://www.apesoft.it).

---

## Features

### Code editor
- Csound syntax highlighting.
- Structure-aware auto-indent for `instr/endin`, `opcode/endop`, `if/elseif/else/endif` and `while/until … do/od`.
- Opcode autocompletion and an inline help bar: syntax with the manual's argument names, one-line description and category for every opcode of the Csound Reference Manual (generated from CsoundQt's `opcodes.xml`, GNU FDL — see `scripts/generate_opcode_help.py`), plus the user-defined opcodes of the current file and, for anything else, the type signature reported by the running engine. **Modern Syntax in Help** (main menu) shows every synopsis in Csound 7 functional form with type annotations (`ares:a = oscil(xamp, xcps)`); **Csound Manual (online)** opens the Csound 7 reference manual in the browser.
- **Apply** recompiles the edited code without restarting the host. The button is outlined in red while the editor differs from the running code.
- Console with Csound's messages and errors.
- The editor itself is a custom JUCE component (`CodeView`, `Source/CsoundCodeView.*`) rather than `juce::CodeEditorComponent`: one implementation for macOS, Windows, Linux and iOS with smooth pixel scrolling (mouse wheel, trackpad, one-finger pan with inertia), line numbers, current-line highlight, double/triple click selection, and on touch screens tap to place the caret, long press to select with handles and a magnifier, and the app's own menu on release.

### DAW parameters
## Audio channels

The plugin accepts 1–16 output channels and 0–16 input channels per bus (mono, stereo, surround or discrete layouts). The DAW decides the track channel count; the engine is recompiled whenever it changes.

- **Channels follow the track** (default): `nchnls` / `nchnls_i` are set to the track's output/input channels (minimum 2 outputs, since `outs` needs two), overriding the values declared in the `.csd`, and the orchestra is recompiled when the track's channel count changes.
- **Follow CSD nchnls** (off by default, in *Config* on the main menu, saved with the project): Csound runs with the `nchnls` / `nchnls_i` declared in the `.csd` header, so the code behaves identically on any track. `outch n` goes to track channel *n*; channels that exist only on one side are silent. If the header does not declare a value, the track count is used.

Any mismatch between header and track is reported in the console. Surround layouts (5.1, 7.1…) are delivered by the host in physical speaker order; the plugin compensates the reordering done by the JUCE wrapper so that `outch 5` is really channel 5 of the track.

A fixed pool of host-automatable parameters can be mapped to Csound channels and read with `chnget`:

| Type | Slots | Configuration |
|---|---|---|
| Slider Float | 64 | min, max, default, skew, step |
| Slider Int | 32 | min, max, default |
| Toggle | 32 | default |
| Menu | 16 | up to 16 labelled options, default |

- Drag a parameter onto the code editor to insert the matching `chnget` line.
- Two-way: `chnset` on a mapped channel updates the parameter (UI slider, host, automation); the value is not overwritten by the host until the parameter has followed it.
- `.csd` files made with **Cabbage** (`<Cabbage>` section) or **CsoundQt** (`<bsbPanel>` widgets: sliders, knobs, spin boxes, scroll numbers, XY controllers, checkboxes, dropdown menus) are imported automatically: widgets are mapped to parameters.
- **Porting Cabbage / CsoundQt files is not guaranteed.** Whether a file runs depends on many factors (frontend-specific opcodes, plugin libraries, external files, real-time options). apeCsound imports **only the supported parameter widgets**; GUI layout, graphics, displays and event buttons are ignored, and the code is left untouched: `invalue` / `cabbageGetValue` must be replaced with `chnget`, `outvalue` / `cabbageSet…` with `chnset`, and FLTK, Python, Lua, OSC or non-bundled plugin opcodes do not work. See the in-app **apeCsound Guide** for details.
- The parameter mapping is saved **inside the `.csd`** in an `<apeCsoundParams>` section after `</CsoundSynthesizer>`. Csound ignores it, so the file stays a valid, portable `.csd`.

### Sessions
- **Folders**: sessions live in `~/Documents/apeCsound/CSD` (iOS: App Group container). `CSD/User` is the default for Save / Save As; Load opens `CSD`. If the session is linked to a file, the choosers open its folder instead.
- **Examples**: the factory examples (Cabbage and CsoundQt collections, see `README_Examples.md`) are built into the binary (`Resources/Examples.zip`, made with `scripts/make_examples_zip.sh`) and extracted to `CSD/Examples` on first launch, without overwriting existing files. **Open Example...** and **Restore Examples** are in the file capsule menu.
- **Load CSD**, **Save**, **Save As…** and **Initialize Session** from the main menu, plus drag & drop of `.csd` files onto the window.
- The DAW project stores the path of the linked `.csd`, a backup copy of the session and the parameter values. **The file on disk is the source of truth.** When the project is reopened, the file is loaded if it exists. The backup copy is used only if the file is missing, or if the session had unsaved changes when the project was saved.
- A toolbar indicator shows the linked file, where it lives, and a `•` when there are unsaved changes.
- A single undo history covers both code edits and parameter mapping changes. It survives closing and reopening the plugin window. Parameter **values** are left to the host's automation.

### Samples folder and errors
- **SSDIR folder**: apeCsound creates `~/Documents/apeCsound/SSDIR` (iOS: in the App Group container shared by app and AUv3) and passes it to Csound as `SSDIR`, after the folder of the linked `.csd`; files written by Csound go to `~/Documents/apeCsound/SFDIR` (`SFDIR`). Copy audio files there and refer to them by name only (`diskin2 "loop.wav"`, GEN01). `SADIR` (analysis files) and `INCDIR` (`#include`) get their own `apeCsound/SADIR` and `apeCsound/INCDIR` folders too. On desktop, **Config > Csound Folders** adds an extra folder to SSDIR/SADIR/INCDIR and chooses the SFDIR output folder. `--env:SSDIR=` in `<CsOptions>` overrides it. **Import Files...** (file capsule menu, every platform) copies files into the right folder by extension (audio to SSDIR, `.udo`/`.inc`/`.orc` to INCDIR, analysis files to SADIR); on iOS it is the only way, since the App Group folder is not visible in the Files app, and **Export Files...** (iOS) shares the files written to SFDIR via the share sheet.
- **Inline errors**: the line of a Csound compile error (and of INIT/PERF errors at run time) is marked in the editor with a red dot, red background and wavy underline; the help bar shows the message when the caret is on that line. Marks clear when the line is edited or on the next Apply.

### Graphs
- **Show Graphs** (main menu) opens a panel docked at the bottom next to the console, like Parameters and Console. It draws Csound's function tables and signal displays instead of printing ASCII graphs in the console: every ftable when it is created (long tables as a min/max band), and `display` / `dispfft` while the panel is visible. Displays are always enabled, even with `-d` in `<CsOptions>`; the panel is cleared on every Apply.

### Audio and MIDI
- Audio input is passed to Csound, so the plugin can process live input as an effect.
- MIDI input is forwarded to Csound.

---

## Requirements

- macOS 12 or later, with Xcode.
- [JUCE 8](https://juce.com) and the Projucer.
- **Csound 7**, built as a static library by `scripts/build_csound_static_macos.sh` (Homebrew `cmake`, `bison`, `flex`).

Csound is **linked statically**: `libCsoundLib64.a` + `libsndfile.a` + `libsamplerate.a` (universal arm64/x86_64, all opcodes in the core, no plugin `.dylib`s, no realtime audio/MIDI backends since the host provides audio and MIDI). The plugin bundle has no external dependency and the same approach works on iOS, where runtime-loaded libraries are not allowed. `Source/CsoundDynamicLib.*` keeps a thin function-pointer layer over the C API (a leftover of the previous `dlopen` design) so the rest of the code did not change.

---

## Building

1. **Clone the repository**
   ```sh
   git clone https://github.com/alessandropetrolati/apeCsound.git
   cd apeCsound
   ```

2. **Build Csound 7 as a static library**
   ```sh
   cd scripts && ./build_csound_static_macos.sh
   ```
   This clones Csound (`develop` branch), libsndfile and libsamplerate, builds them for arm64 and x86_64 and installs headers and universal `.a` files in `build/csound-install/universal/{include,lib}`. The folder is ignored by git. The Xcode project reads headers and libraries from there and links `Accelerate.framework` (Csound's FFT).

   For iOS, then run `./build_csound_static_ios.sh`: it cross-compiles the same sources for device (arm64) and simulator (arm64 + x86_64) into `build/csound-install/ios/{iphoneos,iphonesimulator}/lib`, which the iOS exporter picks up through `$(PLATFORM_NAME)`.

3. **Open the project in the Projucer.** Open `apeCsound.jucer` and check that the JUCE module paths point to your JUCE installation. Then save the project to generate `Builds/MacOSX` and `JuceLibraryCode`.

4. **Build in Xcode.** Open `Builds/MacOSX/apeCsound.xcodeproj` and build the **VST3**, **AUv3** or **Standalone** target; for iOS open `Builds/iOS/apeCsound.xcodeproj` (AUv3 + Standalone). Nothing else to embed: Csound is inside the binary.

> Static Csound 7 build scripts, one per platform (all produce `build/csound-install/...`, the paths used by the `.jucer` exporters):
> - `scripts/build_csound_static_macos.sh` - macOS, universal (arm64 + x86_64)
> - `scripts/build_csound_static_ios.sh` - iOS device + simulator (run the macOS script first: it clones the sources)
> - `scripts/build_csound_static_linux.sh` - Linux (Ubuntu/Debian), x86_64 or aarch64
> - `scripts/build_csound_static_windows.ps1` - Windows x64, Visual Studio 2022 (Debug + Release, static runtime)

---

## Quick start

1. Insert **apeCsound** on a track in your DAW, or launch the standalone app.
2. Use **Load CSD** from the menu, or drag a `.csd` file onto the window. To start from scratch, use **Initialize Session**.
3. Edit the code and press **Apply**.
4. Open the **Parameters** panel, add a parameter, give it the channel name your code reads with `chnget`, and automate it from the DAW.
5. Use **Save** to write code and parameter mapping back to the `.csd`. New files go to `~/Documents/apeCsound` by default.

---

## Project layout

```
Source/
  PluginProcessor.*        Csound engine, parameters, session and project state
  PluginEditor.*           Main window: toolbar, editor, console, menus, About view
  CsoundCodeView.*         CodeView: custom code editor component (layout, scrolling, mouse/touch, keyboard)
  CsoundCodeEditor.*       Csound layer over CodeView (auto-indent, autocompletion, help bar)
  CsoundTokeniser.*        Syntax highlighting
  CsoundParameterEditor.*  Parameters panel
  CsoundActionSheet.*      Touch-friendly menus
  CsoundDynamicLib.*       Function-pointer layer over the statically linked Csound C API
  NativeAlertMac.*         Native macOS dialogs
scripts/                   Build helpers for Csound
Resources/                 App icon
```

---

## Credits

- [Csound](https://csound.com) — the sound and music computing system this plugin is built on. Csound is distributed under the LGPL 2.1.
- [JUCE](https://juce.com) — the C++ framework for the plugin, the standalone app and the UI.

---

## Author

**Alessandro Petrolati** — [apeSoft](https://www.apesoft.it)
