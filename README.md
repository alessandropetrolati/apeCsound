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

### DAW parameters
A fixed pool of host-automatable parameters can be mapped to Csound channels and read with `chnget`:

| Type | Slots | Configuration |
|---|---|---|
| Slider Float | 64 | min, max, default, skew, step |
| Slider Int | 32 | min, max, default |
| Toggle | 32 | default |
| Menu | 16 | up to 16 labelled options, default |

- Drag a parameter onto the code editor to insert the matching `chnget` line.
- `.csd` files made with **Cabbage** (`<Cabbage>` section) or **CsoundQt** (`<bsbPanel>` widgets: sliders, knobs, spin boxes, scroll numbers, XY controllers, checkboxes, dropdown menus) are imported automatically: widgets are mapped to parameters.
- The parameter mapping is saved **inside the `.csd`** in an `<apeCsoundParams>` section after `</CsoundSynthesizer>`. Csound ignores it, so the file stays a valid, portable `.csd`.

### Sessions
- **Load CSD**, **Save**, **Save As…** and **Initialize Session** from the main menu, plus drag & drop of `.csd` files onto the window.
- The DAW project stores the path of the linked `.csd`, a backup copy of the session and the parameter values. **The file on disk is the source of truth.** When the project is reopened, the file is loaded if it exists. The backup copy is used only if the file is missing, or if the session had unsaved changes when the project was saved.
- A toolbar indicator shows the linked file, where it lives, and a `•` when there are unsaved changes.
- A single undo history covers both code edits and parameter mapping changes. It survives closing and reopening the plugin window. Parameter **values** are left to the host's automation.

### Audio and MIDI
- Audio input is passed to Csound, so the plugin can process live input as an effect.
- MIDI input is forwarded to Csound.

---

## Requirements

- macOS 12 or later, with Xcode.
- [JUCE 8](https://juce.com) and the Projucer.
- **Csound 7** as `CsoundLib64.framework` (version 7.0.0 is used during development).

Csound is **not linked at build time**. The plugin loads `CsoundLib64.framework` at runtime (`dlopen`) from inside its own bundle, and a post-build script copies the framework into the bundle. See `Source/CsoundDynamicLib.h`.

---

## Building

1. **Clone the repository**
   ```sh
   git clone https://github.com/alessandropetrolati/apeCsound.git
   cd apeCsound
   ```

2. **Provide Csound 7.** Place `CsoundLib64.framework` in a folder named `Csound` at the repository root:
   ```
   Csound/CsoundLib64.framework
   ```
   This folder is ignored by git. The Xcode project reads Csound's headers from `Csound/CsoundLib64.framework/Headers`.

3. **Check the post-build script.** `scripts/embed_csound_framework.sh` copies the framework into the built bundle and ad-hoc signs it. Make sure its `FRAMEWORK_SRC` variable points to the framework from step 2.

4. **Open the project in the Projucer.** Open `Csound.jucer` and check that the JUCE module paths point to your JUCE installation. Then save the project to generate `Builds/MacOSX` and `JuceLibraryCode`.

5. **Build in Xcode.** Open `Builds/MacOSX/apeCsound.xcodeproj` and build the **VST3** or **Standalone** target. The post-build script embeds and signs Csound automatically.

> `scripts/build_csound_static.sh` is an experimental script for building Csound 7 as static universal libraries (arm64 + x86_64). It is not used by the current Xcode project.

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
  CsoundCodeEditor.*       Code editor (auto-indent, autocompletion, help bar)
  CsoundTokeniser.*        Syntax highlighting
  CsoundParameterEditor.*  Parameters panel
  CsoundActionSheet.*      Touch-friendly menus
  CsoundDynamicLib.*       Runtime loading of CsoundLib64
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
