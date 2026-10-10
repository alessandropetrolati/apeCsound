#!/usr/bin/env python3
"""
fix_moduleinfo_json.py — manual post-build patcher for a JUCE VST3 bundle.

Why this script exists
----------------------
With Projucer 8.0.9 / 8.0.10 the juce_vst3_helper tool can be generated with
JucePlugin_Build_VST3=0. When that happens, the helper still writes
Contents/Resources/moduleinfo.json, but fills the "Classes" array with the
ASCII fallback CIDs ("VST<code><name>" / "VSE<code><name>") instead of the
real VST3 TUIDs registered by the plugin's factory at runtime
(INLINE_UID(PluginCode, 0xABCDEF01, 0x1234ABCD|0x9182FAEB, ManufacturerCode)).

The result: hosts that do a fast/manifest-based scan (Camelot, recent Cubase,
Bitwig, etc.) read the wrong CIDs from the JSON, ask the factory for them,
get nothing, and reject the plugin. A slow scan works because it talks to
the factory directly.

This script fixes a shipped .vst3 IN PLACE, without rebuilding:

  1. Finds the Mach-O binary under Contents/MacOS/<Name>.
  2. Scans it for JUCE-style TUIDs with the canonical pattern
        <PluginCode:4><AB CD EF 01><1234ABCD|9182FAEB><ManufacturerCode:4>
     and picks the pair (Component / Controller) whose <PluginCode> and
     <ManufacturerCode> bytes match the ones encoded in the wrong CIDs.
  3. Reads Contents/Resources/moduleinfo.json (tolerating JUCE's trailing
     commas), rewrites the "CID" field of the "Audio Module Class" entry
     with the Component TUID and the "Component Controller Class" entry
     with the Controller TUID, then writes the file back preserving the
     surrounding formatting (it only substitutes the CID hex strings).
  4. Is idempotent: if the file already matches the binary it just prints
     OK and exits 0 without touching the file.

Usage
-----
    ./fix_moduleinfo_json.py "/path/to/apeCsound.vst3"
    ./fix_moduleinfo_json.py "/path/to/apeCsound.vst3" --dry-run
    ./fix_moduleinfo_json.py "/path/to/apeCsound.vst3" --no-backup

By default a backup copy is saved next to the original as
moduleinfo.json.bak (first run only; never overwritten).

Exit codes
----------
    0  -> manifest already OK, or patched successfully
    1  -> couldn't locate the real CIDs in the binary, file left untouched
    2  -> usage / path error
    3  -> moduleinfo.json structure not recognized

Platform
--------
Pure Python 3. No external tools (otool, codesign, pkgutil) required.
Re-signing the bundle after patching is the caller's responsibility
(the .vst3 signature covers Resources/moduleinfo.json):

    codesign --force --deep --sign "Developer ID Application: ..." \\
             "/path/to/apeCsound.vst3"
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil
import sys


# --------------------------------------------------------------------------
# JSON helpers (JUCE's moduleinfo.json is JSON-with-trailing-commas)
# --------------------------------------------------------------------------

def _strip_json_trailing_commas(text: str) -> str:
    """Return text with trailing commas before ] or } removed, without
    touching characters inside string literals."""
    out, i, n, in_str, esc = [], 0, len(text), False, False
    while i < n:
        c = text[i]
        if in_str:
            out.append(c)
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                in_str = False
            i += 1
            continue
        if c == '"':
            in_str = True
            out.append(c)
            i += 1
            continue
        if c == ",":
            j = i + 1
            while j < n and text[j] in " \t\r\n":
                j += 1
            if j < n and text[j] in "]}":
                i += 1  # drop this comma
                continue
        out.append(c)
        i += 1
    return "".join(out)


# --------------------------------------------------------------------------
# Bundle / binary helpers
# --------------------------------------------------------------------------

def find_binary(bundle: pathlib.Path) -> pathlib.Path:
    macos_dir = bundle / "Contents" / "MacOS"
    if not macos_dir.is_dir():
        raise FileNotFoundError(
            f"Not a macOS .vst3 bundle (no Contents/MacOS): {bundle}"
        )
    execs = [p for p in macos_dir.iterdir() if p.is_file()]
    if not execs:
        raise FileNotFoundError(f"No binary found in {macos_dir}")
    stem = bundle.stem
    for p in execs:
        if p.name == stem:
            return p
    return execs[0]


# --------------------------------------------------------------------------
# CID discovery
# --------------------------------------------------------------------------

# JUCE VST3 wrapper encodes TUIDs as:
#   byte[ 0: 4] = PluginCode      (ASCII, 4 chars, e.g. "Csnd")
#   byte[ 4: 8] = 0xAB 0xCD 0xEF 0x01
#   byte[ 8:12] = 0x12 0x34 0xAB 0xCD  (Component / Processor)
#              or 0x91 0x82 0xFA 0xEB  (Component Controller / Edit Controller)
#   byte[12:16] = ManufacturerCode (ASCII, 4 chars, e.g. "SLog")
JUCE_UID_RE = re.compile(
    rb"[\x20-\x7e]{4}\xab\xcd\xef\x01"
    rb"(\x12\x34\xab\xcd|\x91\x82\xfa\xeb)"
    rb"[\x20-\x7e]{4}"
)
COMPONENT_MID = b"\x12\x34\xab\xcd"
CONTROLLER_MID = b"\x91\x82\xfa\xeb"


def scan_binary_for_uids(binary_data: bytes) -> list[bytes]:
    """Return the de-duplicated list of 16-byte JUCE-style TUIDs found in
    the binary, in order of first appearance."""
    seen, result = set(), []
    for m in JUCE_UID_RE.finditer(binary_data):
        uid = binary_data[m.start():m.start() + 16]
        if uid not in seen:
            seen.add(uid)
            result.append(uid)
    return result


def decode_vst_fallback_cid(cid_hex: str):
    """If the given CID looks like JUCE's ASCII fallback
      "VST" + <PluginCode:4> + <PluginName prefix up to 9 chars padded>
    (first byte 'V','S','T' / 'V','S','E'), return
    (plugin_code_bytes, name_prefix_bytes). Otherwise return None.

    This is how we know which <PluginCode> / <ManufacturerCode> pair to
    pick from the binary when there is more than one candidate.
    """
    try:
        raw = bytes.fromhex(cid_hex)
    except ValueError:
        return None
    if len(raw) != 16:
        return None
    if raw[:3] not in (b"VST", b"VSE"):
        return None
    # bytes[3:7] = plugin code in ASCII; bytes[7:16] = name prefix lowercase
    return raw[3:7], raw[7:16]


def pick_uid_pair(uids: list[bytes],
                  plugin_code: bytes | None,
                  manufacturer_code: bytes | None):
    """From a list of JUCE-style TUIDs pick (component_uid, controller_uid).

    If plugin_code / manufacturer_code are known (derived from the ASCII
    fallback CID), restrict to UIDs whose bytes[0:4] == plugin_code
    (case-insensitive) AND whose bytes[12:16] == manufacturer_code. This
    disambiguates when the binary contains more than one plugin's UIDs
    (e.g. debug leftovers, static libs).

    Returns (component_uid, controller_uid) or (None, None) if not found.
    """
    def eq_ci(a: bytes, b: bytes) -> bool:
        return a.lower() == b.lower()

    def matches(uid: bytes) -> bool:
        if plugin_code is not None and not eq_ci(uid[0:4], plugin_code):
            return False
        if manufacturer_code is not None and uid[12:16] != manufacturer_code:
            return False
        return True

    candidates = [u for u in uids if matches(u)]
    if not candidates and plugin_code is None and manufacturer_code is None:
        candidates = uids

    comp = next((u for u in candidates if u[8:12] == COMPONENT_MID), None)
    ctrl = next((u for u in candidates if u[8:12] == CONTROLLER_MID), None)
    return comp, ctrl


# --------------------------------------------------------------------------
# Manifest read / write
# --------------------------------------------------------------------------

CATEGORY_COMPONENT = "Audio Module Class"
CATEGORY_CONTROLLER = "Component Controller Class"


def parse_manifest(text: str) -> dict:
    return json.loads(_strip_json_trailing_commas(text))


def patch_manifest_text(text: str,
                        class_name: str,
                        old_cid_hex: str,
                        new_cid_hex: str) -> tuple[str, bool]:
    """Replace the CID of the class whose Category == class_name in the raw
    manifest text. We do a minimal text substitution (not a full re-serialise)
    to preserve JUCE's formatting — indentation, trailing commas, blank
    lines, key order, etc. — so that the resulting file is byte-identical
    to a JUCE-built manifest except for the two CID strings.

    Returns (new_text, changed).
    """
    if old_cid_hex.lower() == new_cid_hex.lower():
        return text, False

    # Find the class block. We look for the first occurrence of the category
    # string and then walk backwards to the opening "CID": "...", then swap
    # the hex.
    cat_needle = f'"Category": "{class_name}"'
    cat_pos = text.find(cat_needle)
    if cat_pos < 0:
        raise ValueError(f'class with Category "{class_name}" not found')

    # Find the "Classes": [ ... ] array start before cat_pos to make sure we
    # stay inside the matching object. Then find the "CID": "<hex>" preceding
    # this category.
    cid_re = re.compile(r'"CID"\s*:\s*"([0-9A-Fa-f]{32})"')
    # scan backwards for the CID entry immediately preceding the category
    last = None
    for m in cid_re.finditer(text, 0, cat_pos):
        last = m
    if last is None:
        # maybe CID comes AFTER Category in this file
        m = cid_re.search(text, cat_pos)
        if m is None:
            raise ValueError(f'CID for class "{class_name}" not found')
        last = m

    start, end = last.span(1)
    current = text[start:end]
    if current.lower() != old_cid_hex.lower():
        # manifest already has something different at this slot; still swap
        # it to the target value (idempotent w.r.t. the target).
        pass
    new_text = text[:start] + new_cid_hex.upper() + text[end:]
    return new_text, new_text != text


# --------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------

def fix(bundle_path: str, dry_run: bool = False, make_backup: bool = True) -> int:
    bundle = pathlib.Path(bundle_path).expanduser().resolve()
    if bundle.suffix != ".vst3":
        print(f"error: not a .vst3 bundle path: {bundle}", file=sys.stderr)
        return 2
    if not bundle.is_dir():
        print(f"error: bundle not found or not a directory: {bundle}",
              file=sys.stderr)
        return 2

    manifest_path = bundle / "Contents" / "Resources" / "moduleinfo.json"
    if not manifest_path.is_file():
        print(f"error: no moduleinfo.json in {bundle}", file=sys.stderr)
        print("       nothing to patch — a slow-scan host would accept the "
              "plugin; a fast-scan host will fall back to slow scan.",
              file=sys.stderr)
        return 2

    try:
        binary_path = find_binary(bundle)
    except FileNotFoundError as e:
        print(f"error: {e}", file=sys.stderr)
        return 2

    raw_json = manifest_path.read_text()
    try:
        manifest = parse_manifest(raw_json)
    except Exception as e:
        print(f"error: cannot parse {manifest_path}: {e}", file=sys.stderr)
        return 3

    classes = manifest.get("Classes") or []
    if len(classes) < 2:
        print(f"error: expected >= 2 entries in 'Classes', got {len(classes)}",
              file=sys.stderr)
        return 3

    comp_cls = next(
        (c for c in classes if c.get("Category") == CATEGORY_COMPONENT), None
    )
    ctrl_cls = next(
        (c for c in classes if c.get("Category") == CATEGORY_CONTROLLER), None
    )
    if comp_cls is None or ctrl_cls is None:
        print(f"error: manifest missing '{CATEGORY_COMPONENT}' or "
              f"'{CATEGORY_CONTROLLER}' entry", file=sys.stderr)
        return 3

    old_comp_cid = comp_cls.get("CID", "")
    old_ctrl_cid = ctrl_cls.get("CID", "")

    # Derive expected plugin code / manufacturer code from the current CIDs
    # if they are in JUCE's ASCII fallback form. If not, we still try with
    # None and just pick the first Component + first Controller we see.
    plugin_code = manufacturer_code = None
    for cid in (old_comp_cid, old_ctrl_cid):
        parsed = decode_vst_fallback_cid(cid)
        if parsed is not None:
            plugin_code, _ = parsed
            break
    # Manufacturer code isn't encoded in the ASCII fallback CID; try to get
    # it from a raw (non-fallback) CID in case only one of the two is wrong.
    for cid in (old_comp_cid, old_ctrl_cid):
        try:
            raw = bytes.fromhex(cid)
        except ValueError:
            continue
        if len(raw) == 16 and raw[4:8] == b"\xab\xcd\xef\x01":
            plugin_code = raw[0:4]
            manufacturer_code = raw[12:16]
            break

    binary_data = binary_path.read_bytes()
    uids = scan_binary_for_uids(binary_data)
    if not uids:
        print("error: no JUCE-style VST3 TUIDs found in binary "
              f"{binary_path}", file=sys.stderr)
        print("       (is this really a JUCE VST3? a custom factory would "
              "not match the pattern).", file=sys.stderr)
        return 1

    comp_uid, ctrl_uid = pick_uid_pair(uids, plugin_code, manufacturer_code)
    if comp_uid is None or ctrl_uid is None:
        print("error: could not locate a (Component, Controller) UID pair "
              f"in {binary_path}", file=sys.stderr)
        print("       candidates found:", file=sys.stderr)
        for u in uids:
            kind = ("Component " if u[8:12] == COMPONENT_MID
                    else "Controller" if u[8:12] == CONTROLLER_MID
                    else "?         ")
            print(f"         {kind}  {u.hex()}", file=sys.stderr)
        return 1

    new_comp_cid = comp_uid.hex().upper()
    new_ctrl_cid = ctrl_uid.hex().upper()

    print(f"Bundle              : {bundle}")
    print(f"Binary              : {binary_path.relative_to(bundle)}")
    print(f"Manifest            : {manifest_path.relative_to(bundle)}")
    print(f"Plugin Name         : {manifest.get('Name', '?')}")
    print(f"Version             : {manifest.get('Version', '?')}")
    if plugin_code:
        pc = plugin_code.decode("ascii", errors="replace")
        print(f"Plugin Code         : 0x{plugin_code.hex()} ('{pc}')")
    if manufacturer_code:
        mc = manufacturer_code.decode("ascii", errors="replace")
        print(f"Manufacturer Code   : 0x{manufacturer_code.hex()} ('{mc}')")
    print()
    print(f"Component   old CID : {old_comp_cid}")
    print(f"Component   new CID : {new_comp_cid}")
    print(f"Controller  old CID : {old_ctrl_cid}")
    print(f"Controller  new CID : {new_ctrl_cid}")
    print()

    already_ok = (old_comp_cid.lower() == new_comp_cid.lower()
                  and old_ctrl_cid.lower() == new_ctrl_cid.lower())
    if already_ok:
        print("RESULT: manifest already matches the binary — nothing to do.")
        return 0

    # Apply two textual substitutions. Do them against the ORIGINAL raw
    # string so we preserve every comma, space and newline JUCE wrote.
    new_text = raw_json
    try:
        new_text, changed_c = patch_manifest_text(
            new_text, CATEGORY_COMPONENT, old_comp_cid, new_comp_cid
        )
        new_text, changed_e = patch_manifest_text(
            new_text, CATEGORY_CONTROLLER, old_ctrl_cid, new_ctrl_cid
        )
    except ValueError as e:
        print(f"error: {e}", file=sys.stderr)
        return 3

    if not (changed_c or changed_e):
        print("RESULT: manifest already matches the binary — nothing to do.")
        return 0

    # Sanity-check: the edited text must still parse.
    try:
        parse_manifest(new_text)
    except Exception as e:
        print(f"error: edited manifest no longer parses ({e}); aborting "
              "without writing.", file=sys.stderr)
        return 3

    if dry_run:
        print("DRY RUN: no file written. Re-run without --dry-run to apply.")
        return 0

    if make_backup:
        backup_path = manifest_path.with_suffix(manifest_path.suffix + ".bak")
        if not backup_path.exists():
            shutil.copy2(manifest_path, backup_path)
            print(f"Backup              : {backup_path.relative_to(bundle)}")

    manifest_path.write_text(new_text)
    print(f"RESULT: patched {manifest_path.relative_to(bundle)} "
          f"({len(new_text):,} bytes).")
    print()
    print("IMPORTANT: the bundle signature covers this file. Re-sign with:")
    print(f'    codesign --force --deep --sign "Developer ID Application: '
          f'<your identity>" "{bundle}"')
    print("then re-verify with:  ./verify_vst3_manifest.py "
          f'"{bundle}"')
    return 0


def main(argv) -> int:
    parser = argparse.ArgumentParser(
        description="Patch a shipped JUCE VST3 bundle's moduleinfo.json so "
                    "its CIDs match the factory CIDs inside the binary."
    )
    parser.add_argument("bundle", help="Path to the .vst3 bundle")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show what would be changed, don't write.")
    parser.add_argument("--no-backup", action="store_true",
                        help="Don't create moduleinfo.json.bak before "
                             "overwriting.")
    args = parser.parse_args(argv[1:])
    return fix(args.bundle,
               dry_run=args.dry_run,
               make_backup=not args.no_backup)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
