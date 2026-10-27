#pragma once

#include <JuceHeader.h>
#include <initializer_list>

/*
    Helper di layout "puri" del pannello Parametri (vedi CsoundParameterEditor.cpp),
    isolati qui per i test unitari (Tests/CsoundUnitTests.cpp).
*/
namespace csoundLayout
{
    // Numero di colonne con cui disporre i campi di una card (MIN/MAX/INIT/
    // SKEW/STEP) nella larghezza disponibile: tutti i campi hanno la stessa
    // larghezza (il piu' largo), 1..n colonne. computeWrappedFieldRows() e
    // layoutWrappedFields() passano entrambi da qui: stesso conteggio di
    // colonne, quindi stesso numero di righe (prima i due algoritmi contavano
    // il gap in modo diverso e, per certe larghezze, l'ultimo campo finiva in
    // una riga che l'altezza della card non prevedeva: spariva).
    inline int wrappedFieldColumns (int availableWidth, std::initializer_list<int> fieldWidths, int gap)
    {
        int minW = 1;
        for (int w : fieldWidths) minW = juce::jmax (minW, w);
        const int n = (int) fieldWidths.size();
        return juce::jlimit (1, juce::jmax (1, n), (availableWidth + gap) / (minW + gap));
    }

    // Righe necessarie per n campi disposti su quelle colonne.
    inline int wrappedFieldRows (int availableWidth, std::initializer_list<int> fieldWidths, int gap)
    {
        const int n = (int) fieldWidths.size();
        if (n == 0) return 1;
        const int cols = wrappedFieldColumns (availableWidth, fieldWidths, gap);
        return (n + cols - 1) / cols;
    }
} // namespace csoundLayout
