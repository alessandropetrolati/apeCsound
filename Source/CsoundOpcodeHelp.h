#pragma once

#include <JuceHeader.h>
#include <cstring>

/**
    Inline help for the code editor: one entry per opcode of the Csound
    Reference Manual, with the manual's own synopsis (real argument names:
    xamp, kcps, ifn...), its one-line "refpurpose" description and the
    manual's category ("Signal Generators:Basic Oscillators").

    The table itself lives in CsoundOpcodeHelpData.h, a GENERATED file:
    scripts/generate_opcode_help.py builds it from CsoundQt's opcodes.xml,
    which CsoundQt generates from the manual sources. Nothing here is typed
    by hand - to update the help after a new Csound/manual release, re-run
    the script (see its docstring). Text is GNU FDL, like the manual.

    Opcodes not in the manual (third-party plugins) still get help in the
    editor: CsoundCodeEditor falls back to the type signature reported by
    the running engine (csoundNewOpcodeList), with generic argument names.
    User-defined opcodes of the current .csd are parsed by the editor too.
*/
struct CsoundOpcodeHelp
{
    const char* name;
    const char* category;    // "Group:Subgroup" as in the manual index
    const char* syntax;      // one or more lines, "\n"-separated
    const char* description;
};

/**
    An opcode as reported by the Csound engine actually linked
    (csoundNewOpcodeList/opcodeListEntry): name/outTypes/inTypes are the
    compact type strings Csound uses internally ('a', 'k', 'i', 'S', 'x',
    'o', 'z'...). This is what covers EVERY opcode available in this build
    of Csound, including third-party plugins, not only the manual.
*/
struct CsoundLiveOpcodeInfo
{
    juce::String name, outTypes, inTypes;
};

#include "CsoundOpcodeHelpData.h"

namespace CsoundOpcodeHelpData
{
    /** Exact (case-sensitive) lookup by binary search, then a case-insensitive
        fallback so that e.g. "Oscil" still finds "oscil". */
    inline const CsoundOpcodeHelp* find (const juce::String& opcodeName)
    {
        const auto trimmed = opcodeName.trim();

        if (trimmed.isEmpty())
            return nullptr;

        const char* key = trimmed.toRawUTF8();
        int lo = 0, hi = numEntries - 1;

        while (lo <= hi)
        {
            const int mid = (lo + hi) / 2;
            const int cmp = std::strcmp (table[mid].name, key);

            if (cmp == 0)
                return &table[mid];

            if (cmp < 0)
                lo = mid + 1;
            else
                hi = mid - 1;
        }

        for (auto& entry : table)
            if (trimmed.equalsIgnoreCase (entry.name))
                return &entry;

        return nullptr;
    }
}
