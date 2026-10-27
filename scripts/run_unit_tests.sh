#!/bin/bash
# Compila ed esegue le suite "pure" di Source/Tests/CsoundUnitTests.cpp con il
# solo modulo juce_core (nessun plugin, nessun Csound): CsoundParameterGroups,
# helper Cabbage, wrap dei campi, sintassi. Le suite che usano il processor
# (import, snapshot) girano dentro l'app: Config > Run Self-Tests, oppure
# "apeCsound --run-tests" (Standalone).
#
#   scripts/run_unit_tests.sh [cartella modules di JUCE]
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
JUCE_MODULES="${1:-$HERE/../JUCE/modules}"
[ -d "$JUCE_MODULES/juce_core" ] || { echo "JUCE modules not found: $JUCE_MODULES (pass the path as argument)"; exit 2; }
OUT="$(mktemp -d)"
mkdir -p "$OUT/jh"
printf '#pragma once\n#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1\n#include <juce_core/juce_core.h>\n' > "$OUT/jh/JuceHeader.h"
cat > "$OUT/core.cpp" <<'EOC'
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#define JUCE_STANDALONE_APPLICATION 1
#define JUCE_USE_CURL 0
#define JUCE_MODULE_AVAILABLE_juce_core 1
#include <juce_core/juce_core.cpp>
EOC
cat > "$OUT/main.cpp" <<'EOC'
#include "Tests/CsoundUnitTests.h"
#include <cstdio>
namespace juce { const char* juce_compilationDate = __DATE__; const char* juce_compilationTime = __TIME__; }
int main()
{
    const auto s = csoundTests::runAll (nullptr, [] (const juce::String& m)
    {
        if (m.startsWith ("!!!") || m.startsWith ("FAILED") || m.startsWith ("---") || m.startsWith ("Starting"))
            std::printf ("%s\n", m.toRawUTF8());
    });
    return s.failures == 0 ? 0 : 1;
}
EOC
FLAGS="-std=c++17 -O1 -DCSOUND_TESTS_CORE_ONLY=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_USE_CURL=0 -DJUCE_MODULE_AVAILABLE_juce_core=1 -I$OUT/jh -I$HERE/Source -I$JUCE_MODULES"
if [ "$(uname)" = "Darwin" ]; then
  clang++ $FLAGS -ObjC++ -c "$OUT/core.cpp" -o "$OUT/core.o"
  clang++ $FLAGS "$OUT/main.cpp" "$HERE/Source/Tests/CsoundUnitTests.cpp" "$OUT/core.o" -o "$OUT/tests" -framework Foundation -framework IOKit -framework Security -framework CoreFoundation
else
  g++ $FLAGS -c "$OUT/core.cpp" -o "$OUT/core.o"
  g++ $FLAGS "$OUT/main.cpp" "$HERE/Source/Tests/CsoundUnitTests.cpp" "$OUT/core.o" -o "$OUT/tests" -lpthread -ldl
fi
"$OUT/tests"
