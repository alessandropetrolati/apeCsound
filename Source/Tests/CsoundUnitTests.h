#pragma once

#include <JuceHeader.h>
#include <functional>

class CsoundAudioProcessor;

/*
    Test unitari (juce::UnitTest) delle funzioni "pure" del plugin:

      - CsoundParameterGroups::parse   (gruppi, @xy, pad Cabbage/CsoundQt, groupbox)
      - cabbageImport::*               (regex dei widget: channel, range, value, text)
      - csoundLayout::wrappedField*    (colonne/righe dei campi delle card)
      - csoundSyntax::toModernSyntax   (sintassi funzionale di Csound 7)
      - csoundSyntax::channelNameInLine(canale sotto il caret)

    e, con un processor a disposizione (dentro l'app), dei pezzi che hanno
    bisogno dello stato del plugin, eseguiti su una copia dello stato che
    viene poi ripristinata (captureSessionSnapshot/restoreSessionSnapshot):

      - import Cabbage / CsoundQt      (loadSessionFromText su csd sintetici)
      - albero degli snapshot          (add/rename/overwrite/remove/recall)

    Come si lanciano:
      - Main menu > Config > Run Self-Tests: risultato nella consolle.
      - Standalone: apeCsound --run-tests (stessa cosa, all'avvio).
      - Fuori dall'app (CI, sandbox): le sole suite "pure" compilano con
        juce_core + le intestazioni (CSOUND_TESTS_CORE_ONLY definito), vedi
        scripts/run_unit_tests.sh.
*/
namespace csoundTests
{
    struct Summary
    {
        int passes = 0, failures = 0;
        juce::StringArray failureMessages;
    };

    /** Esegue tutte le suite registrate (quelle "pure" sempre; quelle che
        usano il processor solo se 'processor' non e' nullo). 'log' riceve
        ogni riga di esito. */
    Summary runAll (CsoundAudioProcessor* processor, const std::function<void (const juce::String&)>& log);
}
