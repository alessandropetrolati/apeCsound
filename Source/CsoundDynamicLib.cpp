#include "CsoundDynamicLib.h"

#if JUCE_MAC
 #include <dlfcn.h>
#endif

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
        void* libraryHandle = nullptr;

       #if JUCE_MAC
        juce::File findEmbeddedDylib()
        {
            // L'eseguibile in esecuzione e' .../<Bundle>.{app,vst3,component}/
            // Contents/MacOS/<Nome>: risaliamo a Contents e scendiamo in
            // Frameworks/... - stesso schema di getEmbeddedOpcodeDir() in
            // PluginProcessor.cpp. Il file .dylib (non il .framework intero:
            // non ci serve altro che il binario) viene copiato li' da
            // scripts/embed_csound_framework.sh ad ogni build.
            const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            const auto contents = exe.getParentDirectory().getParentDirectory();
            return contents.getChildFile ("Frameworks/CsoundLib64.framework/CsoundLib64");
        }
       #endif

        template <typename FnPtr>
        bool resolve (FnPtr& fnPtr, const char* name, juce::String* errorMessage)
        {
            fnPtr = reinterpret_cast<FnPtr> (dlsym (libraryHandle, name));

            if (fnPtr == nullptr)
            {
                if (errorMessage != nullptr && errorMessage->isEmpty())
                    *errorMessage = juce::String ("Simbolo Csound mancante nella libreria caricata: ") + name;

                return false;
            }

            return true;
        }
    }

    bool isLoaded() noexcept
    {
        return libraryHandle != nullptr && csoundCreate != nullptr;
    }

    bool load (juce::String* errorMessage)
    {
        if (isLoaded())
            return true;

       #if ! JUCE_MAC
        if (errorMessage != nullptr)
            *errorMessage = "Il caricamento dinamico di CsoundLib64 e' implementato solo per macOS.";

        return false;
       #else
        const auto dylibFile = findEmbeddedDylib();

        if (! dylibFile.existsAsFile())
        {
            if (errorMessage != nullptr)
                *errorMessage = "CsoundLib64 non trovata nel bundle del plugin: " + dylibFile.getFullPathName();

            return false;
        }

        // RTLD_LOCAL (non _GLOBAL): i simboli di Csound restano visibili
        // solo attraverso questi puntatori, non finiscono nello spazio dei
        // simboli globale del processo - evita collisioni se l'host (o un
        // altro plugin) avesse gia' un'altra versione di Csound caricata.
        libraryHandle = dlopen (dylibFile.getFullPathName().toRawUTF8(), RTLD_NOW | RTLD_LOCAL);

        if (libraryHandle == nullptr)
        {
            if (errorMessage != nullptr)
            {
                const char* reason = dlerror();
                *errorMessage = juce::String ("dlopen di CsoundLib64 fallita: ")
                                    + (reason != nullptr ? juce::String (reason) : juce::String ("motivo sconosciuto"));
            }

            return false;
        }

        bool ok = true;

       #define CSOUND_RESOLVE(name) if (ok) ok = resolve (name, #name, errorMessage);

        CSOUND_RESOLVE (csoundInitialize)
        CSOUND_RESOLVE (csoundCreate)
        CSOUND_RESOLVE (csoundDestroy)
        CSOUND_RESOLVE (csoundReset)
        CSOUND_RESOLVE (csoundSetHostData)
        CSOUND_RESOLVE (csoundGetHostData)
        CSOUND_RESOLVE (csoundSetMessageCallback)
        CSOUND_RESOLVE (csoundSetHostAudioIO)
        CSOUND_RESOLVE (csoundSetHostMIDIIO)
        CSOUND_RESOLVE (csoundSetExternalMidiInOpenCallback)
        CSOUND_RESOLVE (csoundSetExternalMidiReadCallback)
        CSOUND_RESOLVE (csoundSetExternalMidiInCloseCallback)
        CSOUND_RESOLVE (csoundSetExternalMidiOutOpenCallback)
        CSOUND_RESOLVE (csoundSetExternalMidiWriteCallback)
        CSOUND_RESOLVE (csoundSetExternalMidiOutCloseCallback)
        CSOUND_RESOLVE (csoundSetOption)
        CSOUND_RESOLVE (csoundCompileCSD)
        CSOUND_RESOLVE (csoundStart)
        CSOUND_RESOLVE (csoundGetKsmps)
        CSOUND_RESOLVE (csoundGetChannels)
        CSOUND_RESOLVE (csoundGetSpin)
        CSOUND_RESOLVE (csoundGetSpout)
        CSOUND_RESOLVE (csoundPerformKsmps)
        CSOUND_RESOLVE (csoundSetControlChannel)
        CSOUND_RESOLVE (csoundGetControlChannel)
        CSOUND_RESOLVE (csoundNewOpcodeList)
        CSOUND_RESOLVE (csoundDisposeOpcodeList)

       #undef CSOUND_RESOLVE

        if (! ok)
        {
            dlclose (libraryHandle);
            libraryHandle = nullptr;
        }

        return ok;
       #endif
    }
}
