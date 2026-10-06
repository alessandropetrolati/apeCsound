#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cstdarg>

// Csound e' incorporato nel bundle del plugin come framework DINAMICO
// (CsoundLib64.framework, VERA release 7.0.0 - installata da
// /Applications/Csound/CsoundLib64.framework dal pkg "csound7Environment",
// NON il vecchio /Library/Frameworks/CsoundLib64.framework che e' rimasto
// Csound 6.18 - copiata in Csound/CsoundLib64.framework accanto a questo
// progetto, vedi extraCustomFrameworks + postbuildCommand/
// embed_csound_framework.sh nell'exporter XCODE_MAC di Csound.jucer).
// A differenza della build "7.0.0-beta.18" esaminata in precedenza (che si
// era rivelata essere ancora Csound 6.18 internamente: version.h/Info.plist
// dicevano "6.18.1", cartella "Versions/6.0" - framework sbagliato copiato
// per errore), questa e' davvero Csound 7.0.0 (version.h: VERSION "7.0",
// cartella "Versions/7.0") e usa una API C nuova, NON compatibile con
// Csound 6:
//   - csoundCreate(hostData, opcodedir) - ora 2 argomenti, il secondo
//     permette di indicare direttamente la cartella degli opcode plugin
//     esterni (Resources/Opcodes64 dentro il framework), senza bisogno di
//     variabili d'ambiente.
//   - csoundStop/csoundCleanup non esistono piu': solo csoundReset +
//     csoundDestroy.
//   - csoundCompileCsdText non esiste piu': csoundCompileCSD(cs, csd, mode,
//     async) - mode=1 per codice testuale (non un path di file), async=0.
//   - csoundSetHostImplementedAudioIO/MIDIIO (con argomenti extra)
//     diventano csoundSetHostAudioIO(cs)/csoundSetHostMIDIIO(cs), senza
//     argomenti oltre a CSOUND*.
//   - csoundGetNchnls/csoundGetNchnlsInput diventano un'unica
//     csoundGetChannels(cs, isInput).
//   - csoundNewOpcodeList/opcodeListEntry/csoundDisposeOpcodeList esistono
//     ancora, stessi campi (opname/outypes/intypes), ma sono dichiarate in
//     csound_misc.h invece che in csound.h.
// (tutto verificato con grep diretto sull'header del framework reale appena
// copiato, non per deduzione).
#include "csound.h"
#include "csound_misc.h"
#include "CsoundOpcodeHelp.h"

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

    /** Ultimi messaggi della console Csound (buffer circolare): un messaggio
        arriva anche se la UI del plugin e' chiusa (handleMessage lo salva
        qui comunque), quindi quando l'editor viene creato puo' richiamare
        questo metodo per recuperare la storia e non perdere nulla di quanto
        successo mentre la finestra era chiusa. */
    juce::StringArray getMessageHistory() const;

    /** Elenco completo degli opcode realmente registrati nell'istanza
        Csound in esecuzione (csoundNewOpcodeList - la stessa fonte che usa
        Csound stesso, quindi sempre completa e in sincrono con la versione
        collegata e con eventuali plugin caricati), usato da
        CsoundCodeEditor per help inline + autocompletamento. Ritorna un
        array vuoto se il motore non e' in esecuzione. Va chiamata dal
        thread dei messaggi. */
    juce::Array<CsoundLiveOpcodeInfo> getOpcodeSignatures() const;

    // --- juce::AudioProcessor ---
    const juce::String getName() const override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // Bus di Input (audio live, es. microfono) + bridge MIDI in/out verso
    // Csound: vedi compileAndStart() per csoundSetHostAudioIO/csoundSetHostMIDIIO.
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
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
    // csoundSetMessageCallback (stile printf/va_list) esiste ancora,
    // invariata, in questo branch di Csound 7 (verificato in csound.h):
    // qui dobbiamo formattarlo noi con vsnprintf. csoundSetMessageStringCallback
    // esiste anche in piu' (con un const char* gia' formattato) ma non e'
    // necessaria.
    static void messageCallback (CSOUND* cs, int attr, const char* fmt, va_list args);
    void handleMessage (const juce::String& msg);
    static juce::String defaultCsdText();

    // --- Bridge MIDI host <-> Csound (csoundSetHostMIDIIO) ---
    // In: alimentata in processBlock() con i messaggi del blocco corrente,
    // letta da midiInReadCallback durante csoundPerformKsmps() - sempre sul
    // thread audio, quindi nessun lock e' necessario.
    // Out: outgoingMidiBuffer e' valido solo per la durata di processBlock();
    // midiOutWriteCallback ci accoda dentro i messaggi generati da Csound
    // (es. tramite l'opcode midiout).
    static int midiInOpenCallback   (CSOUND* cs, void** userData, const char* devName);
    static int midiInReadCallback   (CSOUND* cs, void* userData, unsigned char* buffer, int nBytes);
    static int midiInCloseCallback  (CSOUND* cs, void* userData);
    static int midiOutOpenCallback  (CSOUND* cs, void** userData, const char* devName);
    static int midiOutWriteCallback (CSOUND* cs, void* userData, const unsigned char* buffer, int nBytes);
    static int midiOutCloseCallback (CSOUND* cs, void* userData);

    juce::Array<juce::MidiMessage> incomingMidiQueue;
    juce::MidiBuffer* outgoingMidiBuffer = nullptr;

    std::atomic<CSOUND*> activeCsound { nullptr };
    std::atomic<bool> ready { false };

    // true tra il momento in cui setStateInformation ripristina il testo del
    // .csd e il momento in cui quel testo viene effettivamente compilato -
    // serve perche' l'ordine con cui l'host chiama setStateInformation e
    // prepareToPlay non e' garantito (vedi i due usi in PluginProcessor.cpp).
    std::atomic<bool> pendingStateRestore { false };

    int csKsmps            = 0;
    int csNumChannels      = 0; // canali di output (nchnls) - dimensione di spout
    int csInputChannels    = 0; // canali di input (nchnls_i) - dimensione di spin
    int spoutReadPos       = 0;
    int samplesLeftInBlock = 0;

    double hostSampleRate = 44100.0;
    int    hostBlockSize  = 512;

    juce::String csdText;
    mutable juce::CriticalSection csdTextLock;

    // Buffer circolare dei messaggi della console: riempito in handleMessage
    // a prescindere dal fatto che un editor sia aperto o no, cosi' quando la
    // UI viene (ri)aperta puo' recuperare la storia recente invece di
    // trovare la console vuota anche se Csound e' in esecuzione da tempo.
    static constexpr int messageHistoryCapacity = 500;
    juce::StringArray messageHistory;
    mutable juce::CriticalSection messageHistoryLock;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundAudioProcessor)
};
