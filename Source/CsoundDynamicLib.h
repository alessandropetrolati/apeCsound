#pragma once

#include <JuceHeader.h>
#include "csound.h"
#include "csound_misc.h"
#include "csound_rtaudio.h"
#include "csound_rtmidi.h"

/**
    Csound 7 LINKATA STATICAMENTE (build/csound-install/universal/lib/
    libCsoundLib64.a + libsndfile.a + libsamplerate.a, vedi
    scripts/build_csound_static.sh): nessuna .dylib/.framework da copiare
    nel bundle, nessun dlopen, nessuna firma separata - e soprattutto
    funziona anche su iOS, dove le librerie dinamiche caricate a runtime
    non sono ammesse.

    L'interfaccia resta quella di prima (un namespace di puntatori a
    funzione, CsoundAPI::csoundXxx) cosi' PluginProcessor non cambia:
    load() ora non carica nulla, si limita ad agganciare i puntatori ai
    simboli C linkati staticamente (::csoundXxx di csound.h & co.). E'
    idempotente e non puo' fallire.

    Storia: in precedenza CsoundLib64 veniva caricata a runtime con
    dlopen()/dlsym() dal bundle del plugin (vedi la cronologia git per
    quella versione, scripts/embed_csound_framework.sh era il suo script
    di post-build). I puntatori sotto sono nullptr finche' load() non e'
    stata chiamata.
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

    /** true se load() e' gia' stata chiamata. */
    bool isLoaded() noexcept;

    /** Aggancia i puntatori sopra ai simboli di Csound linkati
        staticamente. Idempotente, ritorna sempre true (errorMessage non
        viene mai scritto: resta per compatibilita' con il codice chiamante). */
    bool load (juce::String* errorMessage = nullptr);
}
