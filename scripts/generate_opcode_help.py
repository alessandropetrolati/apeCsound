#!/usr/bin/env python3
"""
Generates Source/CsoundOpcodeHelpData.h (inline opcode help for the code
editor) from CsoundQt's machine-readable opcode database, which is itself
generated from the Csound Reference Manual sources.

Source file:
    https://raw.githubusercontent.com/CsoundQt/CsoundQt/master/src/opcodes.xml
    (GNU Free Documentation License, same as the Csound manual)

Usage:
    curl -L -o scripts/opcodes.xml \
        https://raw.githubusercontent.com/CsoundQt/CsoundQt/master/src/opcodes.xml
    python3 scripts/generate_opcode_help.py scripts/opcodes.xml [output.h]

Also accepts a tab-separated dump (name, category, description,
synopses joined with \\x01) - used when the XML itself is not at hand.

Each <opcode> element in the XML carries one <desc> and one or more
<synopsis> (each with an <opcodename>). Entries whose name is not a valid
identifier (operators, macros, utilities such as "csound -U ...") are
dropped: the editor looks up the word under the caret, which can only be
an identifier. The few opcodes documented twice in the manual (e.g.
"scale") are merged into one entry with all their synopsis lines.
"""

import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import OrderedDict

IDENT_RE = re.compile(r"^[A-Za-z0-9_]+$")  # 0dbfs is a legal name


def clean_ws(text):
    return re.sub(r"\s+", " ", text or "").strip()


def clean_synopsis(text):
    # The manual wraps long synopses with a trailing backslash: drop it.
    return clean_ws(re.sub(r"\\\s*", " ", text or ""))


# Command-line utilities (csound -U ...) are not opcodes.
SKIP_CATEGORIES = ("Utilities",)


def fix_name(raw):
    """<opcodename> sometimes contains the outputs too ("ihandle faustcompile"):
    the opcode name is always the last token."""
    raw = clean_ws(raw)
    if not raw:
        return ""
    tokens = raw.split(" ")
    name = tokens[-1]
    return name if IDENT_RE.match(name) else ""


def read_xml(path):
    root = ET.parse(path).getroot()
    for category in root.iter("category"):
        cat_name = clean_ws(category.get("name", ""))
        for op in category.iter("opcode"):
            desc_el = op.find("desc")
            desc = clean_ws(desc_el.text if desc_el is not None else "")
            synopses = []
            name = ""
            if cat_name in SKIP_CATEGORIES:
                continue
            for syn in op.findall("synopsis"):
                syn_text = clean_synopsis("".join(syn.itertext()))
                name_el = syn.find("opcodename")
                candidate = fix_name(name_el.text if name_el is not None else "")
                if not name and candidate:
                    name = candidate
                if syn_text:
                    synopses.append(syn_text)
            if name:
                yield name, cat_name, desc, synopses


def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            parts = line.split("\t", 3)
            while len(parts) < 4:
                parts.append("")
            name = fix_name(parts[0])
            if not name or clean_ws(parts[1]) in SKIP_CATEGORIES:
                continue
            synopses = [clean_synopsis(s) for s in parts[3].split("\x01") if clean_synopsis(s)]
            yield name, clean_ws(parts[1]), clean_ws(parts[2]), synopses


def merge(entries):
    table = OrderedDict()
    for name, cat, desc, synopses in entries:
        if name in table:
            e = table[name]
            for s in synopses:
                if s not in e["syntax"]:
                    e["syntax"].append(s)
            if desc and desc not in e["description"]:
                e["description"] = e["description"] + " / " + desc if e["description"] else desc
        else:
            table[name] = {"category": cat, "description": desc, "syntax": list(synopses)}
    return table


def c_str(s):
    out = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ord(ch) < 32:
            continue
        elif ord(ch) > 126:
            # keep the header pure ASCII: encode as UTF-8 escapes
            out.append("".join("\\x%02x" % b for b in ch.encode("utf-8")))
            # a hex escape would swallow following hex digits: break the literal
            out.append('" "')
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    src = sys.argv[1]
    here = os.path.dirname(os.path.abspath(__file__))
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "Source", "CsoundOpcodeHelpData.h")

    entries = read_xml(src) if src.lower().endswith(".xml") else read_tsv(src)
    table = merge(entries)

    # strcmp order, so the C++ side can binary-search by exact name
    names = sorted(table.keys())

    lines = []
    lines.append("// GENERATED FILE - DO NOT EDIT BY HAND.")
    lines.append("// Produced by scripts/generate_opcode_help.py from CsoundQt's opcodes.xml,")
    lines.append("// itself generated from the Csound Reference Manual sources")
    lines.append("// (https://github.com/csound/manual).")
    lines.append("// Text (c) the Csound manual authors, GNU Free Documentation License.")
    lines.append("#pragma once")
    lines.append("")
    lines.append("// Included by CsoundOpcodeHelp.h only (which declares CsoundOpcodeHelp).")
    lines.append("")
    lines.append("namespace CsoundOpcodeHelpData")
    lines.append("{")
    lines.append("    // Sorted by name in strcmp() order (see find()).")
    lines.append("    static const CsoundOpcodeHelp table[] =")
    lines.append("    {")
    for n in names:
        e = table[n]
        lines.append("        { %s, %s," % (c_str(n), c_str(e["category"])))
        lines.append("          %s," % c_str("\n".join(e["syntax"])))
        lines.append("          %s }," % c_str(e["description"]))
    lines.append("    };")
    lines.append("")
    lines.append("    static constexpr int numEntries = %d;" % len(names))
    lines.append("}")
    lines.append("")

    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    cats = len(set(e["category"] for e in table.values()))
    print("Wrote %s: %d opcodes, %d categories" % (os.path.normpath(out_path), len(names), cats))


if __name__ == "__main__":
    main()
