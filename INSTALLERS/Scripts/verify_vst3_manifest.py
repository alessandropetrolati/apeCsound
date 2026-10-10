#!/usr/bin/env python3
"""
verify_vst3_manifest.py — post-build sanity check for a JUCE VST3 bundle.

What it does
------------
Given a .vst3 bundle path, it:
  1. Reads Contents/Resources/moduleinfo.json and lists every declared class
     (Component, EditController, ...) with its CID.
  2. Reads the Mach-O binary under Contents/MacOS/<PluginName> and counts how
     many times each declared CID appears as a raw 16-byte sequence in the
     binary's data sections.
  3. Reports OK if every CID declared in the manifest is present at least
     once in the binary (meaning the factory will register exactly those
     CIDs at runtime), otherwise MISMATCH.

A MISMATCH means that a JUCE host doing a fast/manifest-based scan will read
CIDs the factory will then fail to produce, and will reject the plugin;
forcing a slow/full scan would still work.

Usage
-----
    ./verify_vst3_manifest.py "/path/to/apeCsound.vst3"

Exit code:
    0  -> OK (every manifest CID found in binary)
    1  -> MISMATCH (at least one manifest CID missing from binary)
    2  -> usage/path error

Works on a macOS .vst3 bundle from a Mac or from a Linux shell: it only
parses the raw binary and the JSON, no codesign/pkgutil/otool required.
"""

from __future__ import annotations

import json
import pathlib
import re
import sys


def _strip_json_trailing_commas(text: str) -> str:
    """JUCE's moduleinfo.json uses trailing commas, which strict json.loads()
    doesn't accept. Strip them without disturbing commas inside strings."""
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
            # look ahead past whitespace for ] or }
            j = i + 1
            while j < n and text[j] in " \t\r\n":
                j += 1
            if j < n and text[j] in "]}":
                i += 1  # drop this comma
                continue
        out.append(c)
        i += 1
    return "".join(out)


def load_manifest_cids(manifest_path: pathlib.Path):
    raw = manifest_path.read_text()
    data = json.loads(_strip_json_trailing_commas(raw))
    classes = data.get("Classes", [])
    result = []
    for cls in classes:
        cid_hex = cls.get("CID", "")
        category = cls.get("Category", "?")
        name = cls.get("Name", "?")
        try:
            cid_bytes = bytes.fromhex(cid_hex)
        except ValueError:
            cid_bytes = b""
        result.append((category, name, cid_hex, cid_bytes))
    return data, result


def find_binary(bundle: pathlib.Path) -> pathlib.Path:
    macos_dir = bundle / "Contents" / "MacOS"
    if not macos_dir.is_dir():
        raise FileNotFoundError(f"Not a macOS .vst3 bundle (no Contents/MacOS): {bundle}")
    execs = [p for p in macos_dir.iterdir() if p.is_file()]
    if not execs:
        raise FileNotFoundError(f"No binary found in {macos_dir}")
    # Prefer the one matching the bundle stem (e.g. "apeCsound"), else first.
    stem = bundle.stem
    for p in execs:
        if p.name == stem:
            return p
    return execs[0]


def printable_ascii(cid_bytes: bytes) -> str:
    return "".join(chr(b) if 32 <= b < 127 else "." for b in cid_bytes)


def verify(bundle_path: str) -> int:
    bundle = pathlib.Path(bundle_path).expanduser().resolve()
    if bundle.suffix != ".vst3":
        print(f"error: not a .vst3 bundle path: {bundle}", file=sys.stderr)
        return 2
    if not bundle.is_dir():
        print(f"error: bundle not found or not a directory: {bundle}", file=sys.stderr)
        return 2

    manifest_path = bundle / "Contents" / "Resources" / "moduleinfo.json"
    if not manifest_path.is_file():
        print(f"NOTE: no moduleinfo.json in {bundle}")
        print("      the host will be forced into a slow scan; nothing to verify.")
        return 0

    try:
        manifest, cids = load_manifest_cids(manifest_path)
    except Exception as e:
        print(f"error: cannot parse {manifest_path}: {e}", file=sys.stderr)
        return 2

    binary_path = find_binary(bundle)
    data = binary_path.read_bytes()

    print(f"Bundle       : {bundle}")
    print(f"Manifest     : {manifest_path.relative_to(bundle)}")
    print(f"Binary       : {binary_path.relative_to(bundle)}  ({len(data):,} bytes)")
    print(f"Plugin Name  : {manifest.get('Name', '?')}")
    print(f"Version      : {manifest.get('Version', '?')}")
    print(f"SDK Version  : {cids[0][1] if cids else '?'}")
    print()
    print(f"{'#':<3}{'Category':<32}{'CID (hex)':<34}{'ASCII':<18}{'found-in-bin'}")
    print("-" * 105)

    all_ok = True
    for i, (category, name, cid_hex, cid_bytes) in enumerate(cids, 1):
        if len(cid_bytes) != 16:
            print(f"{i:<3}{category:<32}{cid_hex:<34}{'(bad hex)':<18}(skipped)")
            all_ok = False
            continue
        count = data.count(cid_bytes)
        ok = count >= 1
        all_ok &= ok
        status = f"{count}x  {'OK' if ok else 'MISSING'}"
        print(f"{i:<3}{category:<32}{cid_hex:<34}{printable_ascii(cid_bytes):<18}{status}")

    print()
    if all_ok:
        print("RESULT: OK — every manifest CID is registered by the binary.")
        print("        Fast-scan hosts (Camelot, Cubase, etc.) will match the factory.")
        return 0

    # Helpful extra info on failure: show candidate CIDs we see in the binary
    # that look like JUCE VST3 UIDs, so you can spot what the factory really
    # exposes vs. what the manifest claims.
    print("RESULT: MISMATCH — at least one manifest CID is NOT in the binary.")
    print()
    print("Candidate VST3 UIDs actually present in the binary")
    print("(JUCE pattern: <PluginCode> ABCDEF01 <1234ABCD|9182FAEB> <ManufacturerCode>):")
    seen = set()
    pat = re.compile(rb"[\x20-\x7e]{4}\xab\xcd\xef\x01(\x12\x34\xab\xcd|\x91\x82\xfa\xeb)[\x20-\x7e]{4}")
    for m in pat.finditer(data):
        chunk = data[m.start():m.start() + 16]
        if chunk in seen:
            continue
        seen.add(chunk)
        kind = "Component " if chunk[8:12] == b"\x12\x34\xab\xcd" else "Controller"
        print(f"  {kind}  {chunk.hex()}   {printable_ascii(chunk)}")
    if not seen:
        print("  (no JUCE-style UIDs matched the expected pattern — custom factory?)")
    print()
    print("Tip: delete Contents/Resources/moduleinfo.json from the bundle to ship an")
    print("     immediate hotfix (hosts will fall back to slow scan and the plugin loads).")
    return 1


def main(argv):
    if len(argv) != 2:
        print(f"usage: {argv[0]} <path/to/Plugin.vst3>", file=sys.stderr)
        return 2
    return verify(argv[1])


if __name__ == "__main__":
    sys.exit(main(sys.argv))
