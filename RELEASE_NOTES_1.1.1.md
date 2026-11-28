# apeCsound 1.1.1

Csound 7 inside your DAW: AU, AUv3, VST3 and standalone. This is a maintenance release of 1.1.0, with crash fixes and sample-tighter MIDI out.

## Downloads

| System | File |
|---|---|
| macOS (AU, VST3, Standalone - universal, signed and notarized) | `apeCsound-1.1.1-macOS.pkg` |
| Windows x64 (VST3, Standalone) | `apeCsound-v1.1.1-windows-x64.zip` |
| Linux x64 (VST3, Standalone) | `apeCsound-v1.1.1-linux-x64.zip` |

iOS / iPadOS: from the App Store.

## What's new

**Fixes**
- Undo of a code edit after closing and reopening the plug-in window could crash the host.
- `midion` crashed the host on its first note (a Csound 7.0 bug, patched in our Csound build).
- Editor: after typing `endin` the cursor jumped to the wrong place.
- Parameters: when adding a parameter, the field clicked after the channel keeps the focus.

**MIDI out**
- MIDI written by Csound (`midiout`, `noteon` / `noteoff`, `outkc`...) goes out at the start of the control cycle that wrote it (accurate to ksmps samples), no longer at the start of each DAW block.
- No MIDI flags are needed in `<CsOptions>`. The Guide explains how to record the MIDI out as a MIDI item in REAPER.

**Editor and Parameters**
- Pasted lines are indented automatically.
- Clicking a field of the Parameters panel selects its whole text.

**Algorithm**
- `if / else` that sets the same variable is one "choice" box; conditions are boxes, with dashed "only if" links to what they enclose.
- Box strips have the colour of their block, as in the code margin.
- Explain: notes on `else` / `elseif`, a warning on an empty argument, `sprintfk` shown as k-rate.
- Patch view (experimental): an instrument drawn as a Max / Pure Data patch.

Full list: [CHANGELOG.txt](CHANGELOG.txt)
