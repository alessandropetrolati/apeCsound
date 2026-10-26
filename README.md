# apeCsound

**apeCsound** runs [Csound 7](https://csound.com) code live inside your DAW. Write or load a `.csd` file in the built-in editor, press **Apply**, and the engine recompiles while the track keeps playing. Csound channels become automatable DAW parameters.

It is a **MIDI effect** (an audio effect that also receives MIDI), available as:

| Platform | Formats |
|---|---|
| macOS | AU, VST3, Standalone app |
| Windows | VST3, Standalone app |
| Linux | VST3, Standalone app |
| iPadOS | AUv3, Standalone app |

Developed by **Alessandro Petrolati** — [apeSoft](https://www.apesoft.it).

---

## Download

- **iPad** (AUv3 + app): [App Store](https://apps.apple.com/app/id6821382219)
- **macOS**: [apeCsound.pkg](https://www.apesoft.it/Download/apeCsound.pkg)
- **Windows**: [apeCsound-win.zip](https://www.apesoft.it/Download/apeCsound-win.zip)
- **Linux**: [apeCsound-linux.zip](https://www.apesoft.it/Download/apeCsound-linux.zip)
- Product page: [www.apesoft.it/apecsound](https://www.apesoft.it/apecsound/)

---

## Features

### Code editor
- Csound syntax highlighting and structure-aware auto-indent (`instr/endin`, `opcode/endop`, `if/endif`, loops).
- Autocompletion and an inline **help bar**: syntax, description and category of every opcode of the Csound Reference Manual (from CsoundQt's `opcodes.xml`, GNU FDL — see `scripts/generate_opcode_help.py`), plus the UDOs of the current file. **Config > Modern Syntax in Help** shows the Csound 7 functional form (`ares:a = oscil(xamp, xcps)`).
- **Apply** recompiles without restarting the host; the button is outlined in red while the editor differs from the running code. **Pause** stops Csound and lets the audio pass through.
- **Inline errors**: the line of a compile error (and of INIT/PERF errors at run time) is marked with a red dot, red background and wavy underline; the help bar shows the message.
- Find / Replace, Comment / Uncomment, re-indent, keyboard shortcuts, undo / redo shared with the Parameters panel.
- A custom JUCE code view (`Source/CsoundCodeView.*`), the same on macOS, Windows, Linux and iPadOS: smooth scrolling (wheel, trackpad, one-finger pan with inertia), line numbers, and on touch screens tap to place the caret, long press to select with handles and a magnifier.

### DAW parameters
A fixed pool of **144 host-automatable parameters** can be mapped to Csound channels and read with `chnget`:

| Type | Slots | Configuration |
|---|---|---|
| Float | 64 | min, max, default, skew, step |
| Int | 32 | min, max, default |
| Bool | 32 | default |
| Choice | 16 | up to 16 labelled options, default |

- Drag a parameter onto the code to insert the matching `chnget` line.
- Two-way: `chnset` on a mapped channel moves the parameter (slider, host, automation).
- The mapping is saved **inside the `.csd`**, in an `<apeCsoundParams>` section after `</CsoundSynthesizer>`. Csound ignores it, so the file stays a valid, portable `.csd`.
- **Groups**: a `; @group Name` comment in the code puts the parameters read below it (sliders, toggles and menus alike) under one collapsible label in the panel; parameters without a group go to "Other".
- **Snapshots** (camera button in the Parameters panel): named sets of parameter values, stored in the `.csd` with the mapping. Save, recall, overwrite, rename, delete.
- **Cabbage** (`<Cabbage>`) and **CsoundQt** (`<bsbPanel>`) widgets — sliders, knobs, spin boxes, XY controllers, checkboxes, menus — are imported as parameters when the file is loaded. The rest of their GUI is ignored and the code is not changed: frontend-specific opcodes (`invalue`, `cabbageGetValue`...) must be replaced with `chnget` / `chnset`. Porting is not guaranteed; see the in-app **apeCsound Guide**.

### Audio, MIDI and OSC
- **MIDI effect**: AU/AUv3 type "Music Effect" (`aumf`), VST3 category "Fx". The track audio reaches Csound through `inch`, MIDI through `massign` / `notnum`; MIDI written by Csound (`midiout`) goes back to the DAW. To play a `.csd` instrument, load apeCsound as an effect and send it MIDI (the Guide explains how in Logic, Ableton Live, Cubase, REAPER, Bitwig, Studio One and the iOS hosts).
- **Channels**: 1–16 outputs and 0–16 inputs (mono, stereo, surround, discrete). By default `nchnls` / `nchnls_i` follow the track; **Config > Follow CSD nchnls** uses the values of the `.csd` instead. Surround layouts are reordered so that `outch n` is channel *n* of the track.
- `sr` follows the DAW; `ksmps` is the plug-in latency, reported to the host.
- **OSC** opcodes (`oscinit`, `osclisten`, `oscsend`...) are built in.
- **Host transport** as control channels (Cabbage names): `HOST_BPM`, `IS_PLAYING`, `IS_RECORDING`, `HOST_PPQ_POS`, `HOST_PPQ_BAR_START`, `TIME_IN_SECONDS`, `TIME_IN_SAMPLES`, `TIME_SIG_NUM`, `TIME_SIG_DENOM`.

### Sessions and files
- The **file on disk is the reference**. A DAW project stores the path of the linked `.csd`, an embedded copy and the parameter values; the copy is used only if the file is missing.
- The **apeCsound folder** (`~/Documents/apeCsound`; on iPadOS a folder shared by the app and the AUv3):
  - `CSD/Examples` — factory examples (Cabbage and CsoundQt collections, see `README_Examples.md`), built into the binary and restorable with **Restore Examples**; `CSD/User` — your sessions, default for **Save as**.
  - `SSDIR` (audio files to read), `SFDIR` (files written by Csound), `SADIR` (analysis files), `INCDIR` (`#include`). Csound looks first in the folder of the linked `.csd`, then here.
  - **Config > Extra Folders** adds folders of your choice to SSDIR/SADIR/INCDIR and chooses where Csound writes (SFDIR).
- **File menu** (the file name in the centre of the toolbar): Init Session Clear / Template, Load, Open Example, Save, Save as, Reveal in Finder, **Import Files...** (copies files into the right folder by type), **Manage Files...** (desktop: opens the apeCsound folder; iPadOS: a file manager to import, export and delete files), Restore Examples.
- On iPadOS, Load / Save use apeCsound's own file browser, because the shared folder is not visible in the Files app.

### Graphs and console
- **Graphs** panel: function tables and `display` / `dispfft` signals, drawn instead of ASCII graphs.
- **Console** with Csound's messages and errors; repeated messages are summarised to keep the DAW responsive.

---

## Building

### Requirements
- macOS 12+ with Xcode (for macOS and iPadOS); Visual Studio 2022 (Windows); GCC and the usual development packages (Linux).
- [JUCE 9](https://juce.com) and the Projucer.
- **Csound 7.0.0**, built as a static library by the scripts in `scripts/` (macOS: Homebrew `cmake`, `bison`, `flex`).

Csound is **linked statically** (`libCsoundLib64.a` + libsndfile + libsamplerate + liblo for OSC): all opcodes are in the core, no plugin `.dylib`s and no realtime audio/MIDI backends, since the host provides audio and MIDI. The bundles have no external dependency, and the same approach works on iPadOS, where runtime-loaded libraries are not allowed.

### Steps (macOS / iPadOS)
1. **Clone the repository**
   ```sh
   git clone https://github.com/alessandropetrolati/apeCsound.git
   cd apeCsound
   ```
2. **Build Csound 7.0.0 as a static library**
   ```sh
   cd scripts
   ./build_csound_static_macos.sh   # macOS, universal arm64 + x86_64
   ./build_csound_static_ios.sh     # iPadOS device + simulator (uses the sources cloned by the macOS script)
   ```
   The scripts clone the official **Csound `7.0.0` release tag**, libsndfile, libsamplerate and liblo, and install headers and `.a` files in `build/csound-install/` (ignored by git). If an older Csound checkout is found, it is replaced.
3. **Open `apeCsound.jucer` in the Projucer** and save it, to generate `Builds/` and `JuceLibraryCode/`.
4. **Build in Xcode**: `Builds/MacOSX` (AU, VST3, Standalone) or `Builds/iOS` (AUv3 + app).

### Windows and Linux
Built by **GitHub Actions** (`.github/workflows/build-windows.yml`, `build-linux.yml`): Csound static build (cached), Projucer resave, VST3 + Standalone, zip artifact. They can also be built locally with `scripts/build_csound_static_windows.ps1` / `scripts/build_csound_static_linux.sh` and the generated Visual Studio / Makefile project.

---

## Quick start

1. Insert **apeCsound** on a track as an effect, or launch the standalone app.
2. Open the file menu (file name in the toolbar) and choose **Open Example...** or **Load...**, or drop a `.csd` on the window.
3. Edit the code and press **Apply**.
4. In the **Parameters** panel, add a parameter, give it the channel name your code reads with `chnget`, and automate it from the DAW.
5. **Save** writes the code and the parameter mapping back to the `.csd`.

---

## Project layout

```
Source/
  PluginProcessor.*        Csound engine, parameters, session and project state, folders
  PluginEditor.*           Main window: toolbar, editor, console, menus, Guide, About
  CsoundCodeView.*         Custom code view (layout, scrolling, mouse/touch, keyboard)
  CsoundCodeEditor.*       Csound layer over CodeView (auto-indent, autocompletion, help bar)
  CsoundTokeniser.*        Syntax highlighting
  CsoundParameterEditor.*  Parameters panel
  CsoundGraphs.*           Graphs panel (ftables, display, dispfft)
  CsoundActionSheet.*      Menus (touch-friendly on iPadOS)
  CsdBrowserView.h         iPadOS file browser / file manager
  CsoundAudioSettings.*    Audio/MIDI settings of the standalone app
  CsoundDynamicLib.*       Function-pointer layer over the statically linked Csound C API
  SecurityScopedFile.*     iPadOS file access (security-scoped bookmarks)
  NativeAlertMac.*         Native dialogs
scripts/                   Csound static builds, examples zip, opcode help generator
Resources/                 App icon, factory examples (Examples.zip)
```

---

## Credits

- [Csound](https://csound.com) — the sound and music computing system apeCsound is built on (LGPL 2.1).
- [JUCE](https://juce.com) — the C++ framework for the plug-in, the standalone app and the UI.
- Opcode help text from the Csound Reference Manual (GNU FDL), via [CsoundQt](https://github.com/CsoundQt/CsoundQt)'s opcode database.
- Factory examples by Iain McCurdy, Joachim Heintz and others — see `README_Examples.md` for authors and licenses.

---

## Author

**Alessandro Petrolati** — [apeSoft](https://www.apesoft.it)
