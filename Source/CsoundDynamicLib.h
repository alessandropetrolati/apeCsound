#pragma once

#include <JuceHeader.h>
#include "csound.h"
#include "csound_misc.h"

/**
    Carica CsoundLib64 a RUNTIME con dlopen()/dlsym(), invece di linkarla a
    tempo di compilazione (-framework CsoundLib64). Il motivo: linkare il
    framework costringeva Xcode a gestire install name/@rpath/fase di
    embedding/firma automatica - una catena fragile che ha richiesto piu'
    round di debug (install_name_tool, LD_RUNPATH_SEARCH_PATHS, la cartella
    "libs" che viola le regole di Apple sui framework imbeddati...).

    Con dlopen() scegliamo NOI il path esatto da cui caricare la libreria
    (dentro il bundle del plugin, calcolato a partire dall'eseguibile in
    esecuzione - vedi CsoundDynamicLib.cpp), quindi non serve piu' nessun
    @rpath/LC_LOAD_DYLIB: il nostro eseguibile non dipende a livello di
    Mach-O da CsoundLib64 affatto, dipende da questo modulo che la trova e
    la carica lui stesso. Il file .dylib dentro il bundle resta comunque
    necessario (va ancora copiato li' da uno script di build - vedi
    scripts/embed_csound_framework.sh, molto piu' semplice ora: solo
    copia+pulizia+firma, niente piu' install_name_tool), ma come la
    carichiamo e' interamente sotto il nostro controllo.

    Tutti i puntatori sotto sono nullptr finche' load() non ha successo:
    vanno usati solo dopo aver controllato isLoaded() (o il valore di
    ritorno di load() stesso).
*/
namespace CsoundAPI
{
    using CsoundInitializeFn = int32_t (*) (int32_t);
    using CsoundCreateFn = CSOUND* (*) (void*, const char*);
    using CsoundDestroyFn = void (*) (CSOUND*);
    using CsoundResetFn = void (*) (CSOUND*);
    using CsoundSetHostDataFn = void (*) (CSOUND*, void*);
    using CsoundGetHostDataFn = void* (*) (CSOUND*);
    using CsoundMessageCallbackFn = void (*) (CSOUND*, int32_t, const char*, va_list);
    using CsoundSetMessageCallbackFn = void (*) (CSOUND*, CsoundMessageCallbackFn);
    using CsoundSetHostAudioIOFn = void (*) (CSOUND*);
    using CsoundSetHostMIDIIOFn = void (*) (CSOUND*);
    using CsoundMidiInOpenFn = int32_t (*) (CSOUND*, void**, const char*);
    using CsoundMidiInReadFn = int32_t (*) (CSOUND*, void*, unsigned char*, int32_t);
    using CsoundMidiInCloseFn = int32_t (*) (CSOUND*, void*);
    using CsoundMidiOutOpenFn = int32_t (*) (CSOUND*, void**, const char*);
    using CsoundMidiOutWriteFn = int32_t (*) (CSOUND*, void*, const unsigned char*, int32_t);
    using CsoundMidiOutCloseFn = int32_t (*) (CSOUND*, void*);
    using CsoundSetExternalMidiInOpenCallbackFn = void (*) (CSOUND*, CsoundMidiInOpenFn);
    using CsoundSetExternalMidiReadCallbackFn = void (*) (CSOUND*, CsoundMidiInReadFn);
    using CsoundSetExternalMidiInCloseCallbackFn = void (*) (CSOUND*, CsoundMidiInCloseFn);
    using CsoundSetExternalMidiOutOpenCallbackFn = void (*) (CSOUND*, CsoundMidiOutOpenFn);
    using CsoundSetExternalMidiWriteCallbackFn = void (*) (CSOUND*, CsoundMidiOutWriteFn);
    using CsoundSetExternalMidiOutCloseCallbackFn = void (*) (CSOUND*, CsoundMidiOutCloseFn);
    using CsoundSetOptionFn = int32_t (*) (CSOUND*, const char*);
    using CsoundCompileCSDFn = int32_t (*) (CSOUND*, const char*, int32_t, int32_t);
    using CsoundStartFn = int32_t (*) (CSOUND*);
    using CsoundGetKsmpsFn = uint32_t (*) (CSOUND*);
    using CsoundGetChannelsFn = uint32_t (*) (CSOUND*, int32_t);
    using CsoundGetSpinFn = cs_float* (*) (CSOUND*);
    using CsoundGetSpoutFn = const cs_float* (*) (CSOUND*);
    using CsoundPerformKsmpsFn = int32_t (*) (CSOUND*);
    using CsoundSetControlChannelFn = void (*) (CSOUND*, const char*, cs_float);
    using CsoundGetControlChannelFn = cs_float (*) (CSOUND*, const char*, int32_t*);
    using CsoundNewOpcodeListFn = int32_t (*) (CSOUND*, opcodeListEntry**);
    using CsoundDisposeOpcodeListFn = void (*) (CSOUND*, opcodeListEntry*);

    extern CsoundInitializeFn csoundInitialize;
    extern CsoundCreateFn csoundCreate;
    extern CsoundDestroyFn csoundDestroy;
    extern CsoundResetFn csoundReset;
    extern CsoundSetHostDataFn csoundSetHostData;
    extern CsoundGetHostDataFn csoundGetHostData;
    extern CsoundSetMessageCallbackFn csoundSetMessageCallback;
    extern CsoundSetHostAudioIOFn csoundSetHostAudioIO;
    extern CsoundSetHostMIDIIOFn csoundSetHostMIDIIO;
    extern CsoundSetExternalMidiInOpenCallbackFn csoundSetExternalMidiInOpenCallback;
    extern CsoundSetExternalMidiReadCallbackFn csoundSetExternalMidiReadCallback;
    extern CsoundSetExternalMidiInCloseCallbackFn csoundSetExternalMidiInCloseCallback;
    extern CsoundSetExternalMidiOutOpenCallbackFn csoundSetExternalMidiOutOpenCallback;
    extern CsoundSetExternalMidiWriteCallbackFn csoundSetExternalMidiWriteCallback;
    extern CsoundSetExternalMidiOutCloseCallbackFn csoundSetExternalMidiOutCloseCallback;
    extern CsoundSetOptionFn csoundSetOption;
    extern CsoundCompileCSDFn csoundCompileCSD;
    extern CsoundStartFn csoundStart;
    extern CsoundGetKsmpsFn csoundGetKsmps;
    extern CsoundGetChannelsFn csoundGetChannels;
    extern CsoundGetSpinFn csoundGetSpin;
    extern CsoundGetSpoutFn csoundGetSpout;
    extern CsoundPerformKsmpsFn csoundPerformKsmps;
    extern CsoundSetControlChannelFn csoundSetControlChannel;
    extern CsoundGetControlChannelFn csoundGetControlChannel;
    extern CsoundNewOpcodeListFn csoundNewOpcodeList;
    extern CsoundDisposeOpcodeListFn csoundDisposeOpcodeList;

    /** true se load() e' gia' stata chiamata con successo. */
    bool isLoaded() noexcept;

    /** Cerca ed apre CsoundLib64 dentro il bundle del plugin (Contents/
        Frameworks/CsoundLib64.framework/CsoundLib64, risolto a partire
        dall'eseguibile in esecuzione) e risolve tutti i simboli sopra.
        Idempotente: se gia' riuscita, le chiamate successive ritornano
        true senza ricaricare nulla. Va chiamata una volta prima di usare
        qualunque puntatore di questo namespace (es. all'inizio di
        CsoundAudioProcessor::compileAndStart). Se ritorna false ed
        errorMessage non e' nullptr, vi scrive il motivo (file non trovato,
        dlopen fallita, simbolo mancante). */
    bool load (juce::String* errorMessage = nullptr);
}
