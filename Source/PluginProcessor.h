#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cstdarg>

// Il framework installato in /Library/Frameworks/CsoundLib64.framework su
// questa macchina e' Csound 6 (non 7: csoundCreate ha un solo argomento,
// niente csoundCompileCSD/csoundSetMessageStringCallback ecc.). Le firme
// usate in questo file sono state verificate contro
// csGrain/Source/PluginProcessor.cpp, un plugin che compila e funziona
// gia' contro lo stesso identico framework. Include tra virgolette come
// nel progetto csGrain (funziona grazie a headerPath/libraryPath/
// extraCustomFrameworks nell'exporter XCODE_MAC del .jucer).
#include "csound.h"

/**
    CsoundAudioProcessor e' il juce::AudioProcessor del plugin: stesso
    identico codice serve sia per l'app Standalone sia per un eventuale
    export VST3/AU in futuro (oggi il progetto ha solo "buildStandalone"
    abilitato in Projucer, ma l'architettura e' gia' pronta per aggiungere
    gli altri formati senza toccare questa classe).

    Csound non apre un proprio device audio (niente -odac): genera i
    campioni internamente (spout) e li copiamo nel buffer che l'host (o il
    wrapper Standalone di JUCE) passa a processBlock(), gestendo la
    differenza tra ksmps (block size interno di Csound) e la dimensione del
    buffer host, che quasi mai coincidono.

    Compilazione/avvio (compileAndStart) va sempre chiamata dal thread dei
    messaggi (es. click sul pulsante Run), mai dal thread audio. Lo scambio
    del puntatore CSOUND* e' protetto con suspendProcessing() + uno
    std::atomic, che per un progetto "serio" andra' sostituito con una coda
    lock-free se si vogliono garanzie real-time piu' rigorose (vedi nota in
    compileAndStart()).
*/
class CsoundAudioProcessor final : public juce::AudioProcessor
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // Chiamati sempre sul message thread di JUCE (mai dal thread audio).
        virtual void csoundMessageReceived (const juce::String& message) {}
        virtual void csoundEngineStarted() {}
        virtual void csoundEngineStopped() {}
    };

    CsoundAudioProcessor();
    ~CsoundAudioProcessor() override;

    /** Compila ed esegue un documento .csd completo (testo, non un path).
        Ferma prima l'esecuzione corrente, se presente.
        Va chiamata dal thread dei messaggi. */
    void compileAndStart (const juce::String& csdText);

    /** Ferma la performance in corso (se presente).
        Va chiamata dal thread dei messaggi. */
    void stopEngine();

    bool isEngineRunning() const noexcept { return ready.load(); }

    juce::String getCsdText() const;
    void setCsdText (const juce::String& text);

    // --- Bridge dei control channel, per collegare gli widget della GUI ---
    void   setControlChannel (const juce::String& channelName, double value);
    double getControlChannel (const juce::String& channelName) const;

    void addListener    (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // --- juce::AudioProcessor ---
    const juce::String getName() const override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // Persistenza dello stato: al momento salviamo solo il testo del .csd,
    // cosi' una DAW che salva/ricarica il progetto ritrova lo stesso codice.
    // In Fase 3 qui andra' aggiunto anche il layout della GUI (widget).
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    // Csound 6: il callback messaggi e' in stile printf/va_list (in Csound 7
    // sarebbe stato csoundSetMessageStringCallback, con un const char* gia'
    // formattato: qui dobbiamo formattarlo noi con vsnprintf).
    static void messageCallback (CSOUND* cs, int attr, const char* fmt, va_list args);
    void handleMessage (const juce::String& msg);
    static juce::String defaultCsdText();

    std::atomic<CSOUND*> activeCsound { nullptr };
    std::atomic<bool> ready { false };

    int csKsmps            = 0;
    int csNumChannels      = 0;
    int spoutReadPos       = 0;
    int samplesLeftInBlock = 0;

    double hostSampleRate = 44100.0;
    int    hostBlockSize  = 512;

    juce::String csdText;
    mutable juce::CriticalSection csdTextLock;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundAudioProcessor)
};
