#include "CsoundDynamicLib.h"


namespace CsoundAPI
{
    CsoundInitializeFn csoundInitialize = nullptr;
    CsoundCreateFn csoundCreate = nullptr;
    CsoundDestroyFn csoundDestroy = nullptr;
    CsoundResetFn csoundReset = nullptr;
    CsoundSetHostDataFn csoundSetHostData = nullptr;
    CsoundGetHostDataFn csoundGetHostData = nullptr;
    CsoundSetMessageCallbackFn csoundSetMessageCallback = nullptr;
    CsoundSetHostAudioIOFn csoundSetHostAudioIO = nullptr;
    CsoundSetHostMIDIIOFn csoundSetHostMIDIIO = nullptr;
    CsoundSetExternalMidiInOpenCallbackFn csoundSetExternalMidiInOpenCallback = nullptr;
    CsoundSetExternalMidiReadCallbackFn csoundSetExternalMidiReadCallback = nullptr;
    CsoundSetExternalMidiInCloseCallbackFn csoundSetExternalMidiInCloseCallback = nullptr;
    CsoundSetExternalMidiOutOpenCallbackFn csoundSetExternalMidiOutOpenCallback = nullptr;
    CsoundSetExternalMidiWriteCallbackFn csoundSetExternalMidiWriteCallback = nullptr;
    CsoundSetExternalMidiOutCloseCallbackFn csoundSetExternalMidiOutCloseCallback = nullptr;
    CsoundSetOptionFn csoundSetOption = nullptr;
    CsoundCompileCSDFn csoundCompileCSD = nullptr;
    CsoundStartFn csoundStart = nullptr;
    CsoundGetKsmpsFn csoundGetKsmps = nullptr;
    CsoundGetChannelsFn csoundGetChannels = nullptr;
    CsoundGetSpinFn csoundGetSpin = nullptr;
    CsoundGetSpoutFn csoundGetSpout = nullptr;
    CsoundPerformKsmpsFn csoundPerformKsmps = nullptr;
    CsoundSetControlChannelFn csoundSetControlChannel = nullptr;
    CsoundGetControlChannelFn csoundGetControlChannel = nullptr;
    CsoundNewOpcodeListFn csoundNewOpcodeList = nullptr;
    CsoundDisposeOpcodeListFn csoundDisposeOpcodeList = nullptr;

    namespace
    {
        bool bound = false;
    }

    bool isLoaded() noexcept
    {
        return bound;
    }

    bool load (juce::String* errorMessage)
    {
        juce::ignoreUnused (errorMessage);

        if (bound)
            return true;

        // "::nome" = la funzione C di csound.h / csound_misc.h /
        // csound_rtaudio.h / csound_rtmidi.h, linkata staticamente. Un
        // errore di compilazione qui significa che la firma nel typedef
        // (CsoundDynamicLib.h) non coincide piu' con l'header di Csound.
        csoundInitialize                      = &::csoundInitialize;
        csoundCreate                          = &::csoundCreate;
        csoundDestroy                         = &::csoundDestroy;
        csoundReset                           = &::csoundReset;
        csoundSetHostData                     = &::csoundSetHostData;
        csoundGetHostData                     = &::csoundGetHostData;
        csoundSetMessageCallback              = &::csoundSetMessageCallback;
        csoundSetHostAudioIO                  = &::csoundSetHostAudioIO;
        csoundSetHostMIDIIO                   = &::csoundSetHostMIDIIO;
        csoundSetExternalMidiInOpenCallback   = &::csoundSetExternalMidiInOpenCallback;
        csoundSetExternalMidiReadCallback     = &::csoundSetExternalMidiReadCallback;
        csoundSetExternalMidiInCloseCallback  = &::csoundSetExternalMidiInCloseCallback;
        csoundSetExternalMidiOutOpenCallback  = &::csoundSetExternalMidiOutOpenCallback;
        csoundSetExternalMidiWriteCallback    = &::csoundSetExternalMidiWriteCallback;
        csoundSetExternalMidiOutCloseCallback = &::csoundSetExternalMidiOutCloseCallback;
        csoundSetOption                       = &::csoundSetOption;
        csoundCompileCSD                      = &::csoundCompileCSD;
        csoundStart                           = &::csoundStart;
        csoundGetKsmps                        = &::csoundGetKsmps;
        csoundGetChannels                     = &::csoundGetChannels;
        csoundGetSpin                         = &::csoundGetSpin;
        csoundGetSpout                        = &::csoundGetSpout;
        csoundPerformKsmps                    = &::csoundPerformKsmps;
        csoundSetControlChannel               = &::csoundSetControlChannel;
        csoundGetControlChannel               = &::csoundGetControlChannel;
        csoundNewOpcodeList                   = &::csoundNewOpcodeList;
        csoundDisposeOpcodeList               = &::csoundDisposeOpcodeList;

        bound = true;
        return true;
    }
}
