#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cstdarg>

// Csound (la VERA release 7.0.0, installata da /Applications/Csound dal pkg
// "csound7Environment" - non il vecchio /Library/Frameworks/CsoundLib64.
// framework, rimasto Csound 6.18) NON viene piu' linkata a tempo di
// compilazione (niente -framework CsoundLib64): viene caricata a RUNTIME
// con dlopen()/dlsym() da CsoundDynamicLib - vedi il commento in cima a
// quel file per il perche' (install name/@rpath/fase di embedding gestita
// da Xcode erano una fonte continua di problemi). Qui serve comunque
// #include "csound.h"/"csound_misc.h" per i TIPI (CSOUND, MYFLT/cs_float,
// opcodeListEntry...) e per i prototipi usati solo come riferimento per i
// typedef dei puntatori a funzione in CsoundDynamicLib.h - non per
// linkare: tutte le chiamate nel .cpp passano da CsoundAPI::csoundXxx
// (puntatori a funzione risolti da CsoundDynamicLib::load()), mai da
// csoundXxx(...) direttamente, altrimenti il linker tornerebbe a
// richiedere il simbolo reale.
//
// API usata (Csound 7, NON compatibile con la sintassi Csound 6 - vedi
// anche i commenti in PluginProcessor.cpp): csoundCreate(hostData,
// opcodedir) a 2 argomenti; niente csoundStop/csoundCleanup (solo
// csoundReset + csoundDestroy); niente csoundCompileCsdText
// (csoundCompileCSD(cs, csd, mode, async) con mode=1 per codice testuale);
// csoundSetHostAudioIO(cs)/csoundSetHostMIDIIO(cs) senza argomenti extra;
// csoundGetChannels(cs, isInput) al posto di csoundGetNchnls/
// csoundGetNchnlsInput; csoundNewOpcodeList/opcodeListEntry/
// csoundDisposeOpcodeList dichiarate in csound_misc.h, non in csound.h.
#include "csound.h"
#include "csound_misc.h"
#include "CsoundDynamicLib.h"
#include "CsoundOpcodeHelp.h"

class CsoundAudioProcessor; // vedi ChannelHostParameter sotto

/**
    Parametro apvts di uno dei 16 slot "macro" (vedi CsoundAudioProcessor::
    ChannelParamSlot): resta un juce::AudioParameterFloat normale (stesso
    range 0..1 fisso, stesso ID fisso - l'host continua a vederlo cosi' per
    l'automazione) ma legge SEMPRE lo slot corrente (owner.getChannelParamSlot)
    per decidere cosa MOSTRARE all'host:
      - getName(): il nome del canale Csound assegnato ("rename"), se non
        vuoto, altrimenti il nome di default "Param N";
      - getText()/getValueForText(): il valore denormalizzato (vero min/max/
        skew/increment), non il numero 0..1 grezzo, cosi' l'host (automazione,
        tooltip) mostra qualcosa di leggibile invece di "0.42";
      - getDefaultValue(): il defaultValue dello slot (denormalizzato come
        sopra), cosi' il "reset to default" dell'host usa il valore scelto
        dall'utente nell'editor dei parametri invece di un fisso 0..1.
    Nessuno stato duplicato: niente da tenere sincronizzato, lo slot in
    CsoundAudioProcessor resta l'unica fonte di verita'. CsoundAudioProcessor::
    setChannelParamSlot chiama updateHostDisplay() dopo ogni modifica, cosi'
    l'host rilegge subito il nome aggiornato (il supporto a parameterInfo
    Changed varia da host a host, ma e' il meccanismo standard JUCE per
    questo).
*/
class ChannelHostParameter final : public juce::AudioParameterFloat
{
public:
    ChannelHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                           const juce::ParameterID& paramID, juce::String defaultName);

    juce::String getName (int maximumStringLength) const override;
    juce::String getText (float normalisedValue, int maximumStringLength) const override;
    float getValueForText (const juce::String& text) const override;
    float getDefaultValue() const override;

private:
    CsoundAudioProcessor& owner;
    const int index;
    const juce::String fallbackName;
};

/**
    Stesso disegno di ChannelHostParameter ma per i 16 "macro" parametri
    INTERI (vedi CsoundAudioProcessor::IntParamSlot): un juce::
    AudioParameterInt con range NATIVO fisso 0..intHostRangeMax (mai
    cambiato - e' quello che l'host automatizza/salva), che pero' legge
    sempre lo slot corrente per nome, min/max/default REALI (interi,
    qualsiasi range l'utente scelga nell'editor) e il testo mostrato.
    A differenza di ChannelHostParameter (float, range nativo gia' 0..1)
    qui il range nativo e' 0..intHostRangeMax: denormalizeIntParam/
    normalizeIntParam fanno da ponte tra normalizzato 0..1 (il "linguaggio"
    comune di tutti i RangedAudioParameter - getText/getValueForText/
    getDefaultValue ricevono/restituiscono SEMPRE 0..1, mai il valore
    nativo intero) e il valore intero reale dello slot.
*/
class IntHostParameter final : public juce::AudioParameterInt
{
public:
    IntHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                       const juce::ParameterID& paramID, juce::String defaultName);

    juce::String getName (int maximumStringLength) const override;
    juce::String getText (float normalisedValue, int maximumStringLength) const override;
    float getValueForText (const juce::String& text) const override;
    float getDefaultValue() const override;

private:
    CsoundAudioProcessor& owner;
    const int index;
    const juce::String fallbackName;
};

/**
    Stesso disegno di ChannelHostParameter ma per i 16 "macro" parametri
    booleani (vedi CsoundAudioProcessor::BoolParamSlot): un juce::
    AudioParameterBool stabile per l'host (ID/range mai cambiati), che pero'
    legge sempre lo slot corrente per nome visualizzato e default value.
*/
class BoolHostParameter final : public juce::AudioParameterBool
{
public:
    BoolHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                        const juce::ParameterID& paramID, juce::String defaultName);

    juce::String getName (int maximumStringLength) const override;
    float getDefaultValue() const override;

private:
    CsoundAudioProcessor& owner;
    const int index;
    const juce::String fallbackName;
};

/**
    Stesso disegno di ChannelHostParameter ma per i 16 "macro" parametri a
    scelta multipla (vedi CsoundAudioProcessor::ChoiceParamSlot): un juce::
    AudioParameterChoice con un numero FISSO di opzioni (CsoundAudioProcessor::
    maxChoiceOptions - stabile per l'host, come numChannelParams/numBoolParams),
    costruito con etichette segnaposto ("1".."N") mai mostrate davvero:
    getName()/getText() leggono sempre lo slot corrente per il nome del
    parametro e le etichette VERE delle opzioni (con fallback "Option N" per
    quelle non rinominate - vedi CsoundAudioProcessor::getChoiceOptionLabel).
*/
class ChoiceHostParameter final : public juce::AudioParameterChoice
{
public:
    ChoiceHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                          const juce::ParameterID& paramID, juce::String defaultName,
                          const juce::StringArray& placeholderChoices, int defaultChoiceIndex);

    juce::String getName (int maximumStringLength) const override;
    juce::String getText (float normalisedValue, int maximumStringLength) const override;
    float getDefaultValue() const override;

private:
    CsoundAudioProcessor& owner;
    const int index;
    const juce::String fallbackName;
};

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

    // --- 16 "macro" parametri host (VST3/AU/Standalone) mappabili a canali
    //     Csound (chnget) -------------------------------------------------
    //
    // VST3/AU (e la maggior parte degli host) si aspettano un numero di
    // parametri FISSO, deciso una volta per tutte alla costruzione del
    // processor: non si possono far apparire/scomparire parametri a runtime
    // in modo affidabile. Per questo, come fa Cabbage, pre-allochiamo un
    // pool fisso (numChannelParams) di parametri REALI (automatizzabili,
    // con recall automatico via APVTS) invece di crearne uno per canale.
    //
    // Ogni parametro apvts resta SEMPRE un float normalizzato 0..1, MAI
    // modificato dopo la costruzione (nome, range incluso): e' quello che
    // l'host vede, automatizza e salva, quindi deve restare stabile. Nome
    // "vero" (es. "freq"), range reale (es. 20-2000), skew e increment che
    // l'utente definisce nel futuro editor dei parametri vivono SOLO qui,
    // in ChannelParamSlot: sono usati per denormalizzare
    // il valore 0..1 dell'host nel valore reale da scrivere nel canale
    // Csound (vedi denormalizeChannelParam) e per etichettare la UI
    // nostra (slider nell'editor dei parametri, popup cliccando
    // sull'opcode nel codice) - MAI per rinominare o cambiare range al
    // parametro apvts stesso, che da' problemi su molti host se fatto a
    // runtime.
    static constexpr int numChannelParams = 64;

    // skew: stesso significato di juce::Slider::setSkewFactor (la stessa
    // convenzione usata da Cabbage per il campo skew di range(), verificata
    // contro il comportamento reale di Cabbage) - reshape del normalizzato
    // x (0..1) PRIMA di interpolare linearmente tra min e max, con
    // x' = pow(x, 1/skew). skew=1 e' lineare; skew<1 da' piu' risoluzione
    // vicino a MIN (la parte bassa del range occupa piu' corsa dello
    // slider); skew>1 da' piu' risoluzione vicino a MAX. Funziona con
    // QUALSIASI min/max (anche negativi), a differenza di un esponenziale
    // classico.
    // increment: passo di quantizzazione in unita' REALI (come minValue/
    // maxValue) - il valore denormalizzato viene arrotondato al multiplo
    // di increment piu' vicino a partire da minValue. <= 0 disabilita lo
    // snap (valore continuo).
    struct ChannelParamSlot
    {
        // Vuoto = slot non assegnato a nessun canale Csound (non viene
        // scritto nulla in prepareToPlay/processBlock).
        juce::String channelName;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float defaultValue = 0.0f; // in unita' REALI (come minValue/maxValue, non normalizzato 0..1)
        float skew = 1.0f;
        float increment = 0.001f;
    };

    /** ID stabile (mai da cambiare, anche in futuro: e' quello con cui host
        e progetti salvati identificano il parametro) del parametro apvts
        per lo slot "index" (0-based, 0..numChannelParams-1). */
    static juce::String getChannelParamID (int index);

    ChannelParamSlot getChannelParamSlot (int index) const;
    void setChannelParamSlot (int index, const ChannelParamSlot& slot);

    /** Converte il valore normalizzato 0..1 del parametro apvts nel valore
        reale (secondo min/max/skew/increment di quello slot) da scrivere
        nel canale Csound. Funzione pura, nessun lock. */
    static double denormalizeChannelParam (const ChannelParamSlot& slot, float normalized);

    /** Inversa di denormalizeChannelParam: dal valore reale al normalizzato
        0..1 - usata da ChannelHostParameter::getValueForText, per quando
        l'utente digita un valore nella casella di automazione dell'host.
        Funzione pura, nessun lock. */
    static float normalizeChannelParam (const ChannelParamSlot& slot, double real);

    // --- 32 "macro" parametri host INTERI, stesso disegno dei 64 float qui
    //     sopra ma senza skew/increment (sempre lineare, passo 1) - vedi IntHostParameter e
    //     IntParamSlot. Range nativo apvts fisso 0..intHostRangeMax (molti
    //     passi per una risoluzione di automazione decente), min/max/default
    //     REALI (interi) solo nello slot. Il valore spinto al canale Csound
    //     e' l'intero reale (denormalizeIntParam), non il valore nativo
    //     0..intHostRangeMax.
    static constexpr int numIntParams = 32;
    static constexpr int intHostRangeMax = 1000;

    struct IntParamSlot
    {
        juce::String channelName; // vuoto = non assegnato
        int minValue = 0;
        int maxValue = 127;
        int defaultValue = 0;
    };

    static juce::String getIntParamID (int index);

    IntParamSlot getIntParamSlot (int index) const;
    void setIntParamSlot (int index, const IntParamSlot& slot);

    /** Converte il normalizzato 0..1 (il "linguaggio" comune di getText/
        getValueForText/getDefaultValue per qualsiasi RangedAudioParameter)
        nel valore intero reale (min/max dello slot) da scrivere nel canale
        Csound. Funzione pura, nessun lock. */
    static int denormalizeIntParam (const IntParamSlot& slot, float normalized);

    /** Inversa di denormalizeIntParam. Funzione pura, nessun lock. */
    static float normalizeIntParam (const IntParamSlot& slot, double real);

    // --- 32 "macro" parametri host booleani, stesso disegno dei parametri
    //     qui sopra (pool fisso, parametro apvts stabile, metadata reali
    //     solo nello slot) - vedi BoolHostParameter e BoolParamSlot. Il
    //     valore (0.0/1.0) viene spinto al canale Csound assegnato con
    //     csoundSetControlChannel, esattamente come i parametri float.
    static constexpr int numBoolParams = 32;

    struct BoolParamSlot
    {
        juce::String channelName; // vuoto = non assegnato
        bool defaultValue = false;
    };

    static juce::String getBoolParamID (int index);

    BoolParamSlot getBoolParamSlot (int index) const;
    void setBoolParamSlot (int index, const BoolParamSlot& slot);

    // --- 16 "macro" parametri host a scelta multipla, stesso disegno dei
    //     float/bool qui sopra - vedi ChoiceHostParameter e ChoiceParamSlot.
    //     maxChoiceOptions e' il numero FISSO di opzioni che l'host vede per
    //     ciascuno di questi 16 parametri (deciso una volta per tutte, come
    //     numChannelParams/numBoolParams: un AudioParameterChoice non puo'
    //     cambiare il proprio numero di opzioni a runtime in modo affidabile
    //     su molti host) - le etichette VERE (fino a maxChoiceOptions,
    //     eventualmente meno se l'utente ne lascia alcune vuote, che
    //     ricadono su "Option N") vivono nello slot, non nel parametro
    //     apvts. L'indice selezionato e' 0-based OVUNQUE internamente
    //     (optionLabels, defaultIndex, ID del combobox nel Generic Editor)
    //     ma viene spinto al canale Csound assegnato con
    //     csoundSetControlChannel GIA' +1 (1..maxChoiceOptions, non
    //     0..maxChoiceOptions-1) - Cabbage stessa numera le opzioni del
    //     combobox a partire da 1 (vedi anche importCabbageParameters), e
    //     un .csd scritto per Cabbage ha gia' il codice Csound che si
    //     aspetta channel value 1..N: vedi il commento su
    //     pushChannelParametersToCsound nel .cpp.
    static constexpr int numChoiceParams = 16;
    static constexpr int maxChoiceOptions = 8;

    struct ChoiceParamSlot
    {
        juce::String channelName; // vuoto = non assegnato
        juce::StringArray optionLabels; // fino a maxChoiceOptions etichette; vuote = fallback "Option N" (vedi getChoiceOptionLabel)
        int defaultIndex = 0;
    };

    static juce::String getChoiceParamID (int index);

    /** Etichetta VERA dell'opzione "optionIndex" (0-based) dello slot, o
        "Option N" (1-based, per l'utente) se lasciata vuota/non impostata.
        Funzione pura, nessun lock. */
    static juce::String getChoiceOptionLabel (const ChoiceParamSlot& slot, int optionIndex);

    ChoiceParamSlot getChoiceParamSlot (int index) const;
    void setChoiceParamSlot (int index, const ChoiceParamSlot& slot);

private:
    // DEVE essere dichiarato (quindi costruito) PRIMA di apvts qui sotto:
    // l'ordine di inizializzazione dei membri segue l'ordine di
    // DICHIARAZIONE nella classe, non quello nella member-init-list del
    // costruttore (e non dipende da quale blocco public:/private: li
    // contiene). apvts costruisce i 16 ChannelHostParameter (vedi
    // createChannelParamLayout), e questi leggono channelParamSlots
    // tramite owner.getChannelParamSlot() non appena l'host/JUCE
    // interroga un parametro (es. per seminare il valore iniziale della
    // ValueTree, o REAPER che enumera i parametri all'istanziazione) - se
    // channelParamSlots fosse ancora dichiarato DOPO apvts (com'era prima),
    // durante quella finestra l'array non sarebbe ancora stato costruito:
    // leggerlo e' undefined behavior. Causa reale di un crash osservato in
    // Reaper (EXC_BAD_ACCESS su un indirizzo spazzatura dentro una
    // juce::String copiata da memoria non ancora costruita).
    std::array<ChannelParamSlot, (size_t) numChannelParams> channelParamSlots;
    mutable juce::CriticalSection channelParamSlotsLock;

    // Stesso identico ragionamento del commento sopra (ordine di
    // dichiarazione = ordine di costruzione): anche IntHostParameter/
    // BoolHostParameter/ChoiceHostParameter leggono il loro slot tramite
    // owner.getXxxParamSlot() non appena l'host li interroga durante la
    // costruzione di apvts, quindi anche questi array devono essere
    // dichiarati (= costruiti) prima.
    std::array<IntParamSlot, (size_t) numIntParams> intParamSlots;
    mutable juce::CriticalSection intParamSlotsLock;

    std::array<BoolParamSlot, (size_t) numBoolParams> boolParamSlots;
    mutable juce::CriticalSection boolParamSlotsLock;

    std::array<ChoiceParamSlot, (size_t) numChoiceParams> choiceParamSlots;
    mutable juce::CriticalSection choiceParamSlotsLock;

public:
    // Parametri host (VST3/AU/Standalone): pubblico perche' l'editor dei
    // parametri (futuro) e PluginEditor devono poterci costruire sopra
    // Slider/SliderAttachment direttamente.
    juce::AudioProcessorValueTreeState apvts;

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

    // Persistenza dello stato: testo del .csd, stato nativo di apvts (i 64
    // valori normalizzati 0..1 - gia' gestito in automatico dall'host in
    // VST3/AU, ma lo salviamo comunque anche qui perche' la Standalone si
    // appoggia solo a getStateInformation/setStateInformation) e i
    // metadata per-slot (nome canale/range/skew/increment) che apvts non conosce.
    // In Fase 3 qui andra' aggiunto anche il layout della GUI (widget).
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Persistenza ESPLICITA su file, indipendente dal progetto della DAW
    // (getStateInformation/setStateInformation sopra restano l'unico modo
    // con cui l'host salva/ripristina lo stato automaticamente, ma quello
    // stato vive solo DENTRO il progetto: rimuovendo il plugin dalla
    // traccia, o il progetto stesso, codice e mapping dei parametri
    // andrebbero persi senza nessuna copia indipendente).
    //
    // Il file scritto e' un .csd VERO E VALIDO - lo stesso testo che
    // getCsdText() ritorna, apribile/eseguibile anche fuori da questo
    // plugin (Csound stesso, un editor di testo, un altro host) - non un
    // formato proprietario/binario. Il mapping dei 64 parametri (nome
    // canale/range/skew/increment/default per slot) viene scritto in APPENDICE, dopo
    // il codice, dentro un tag <CsoundStudioParams>...</CsoundStudioParams>
    // creato apposta: Csound analizza un .csd cercando i tag <CsOptions>/
    // <CsInstruments>/<CsScore> per nome, quindi ignora senza problemi
    // qualunque tag sconosciuto dopo </CsoundSynthesizer> - il file resta
    // un .csd legittimo anche per chi non ha questo plugin, semplicemente
    // senza il mapping dei parametri. Vedi saveSessionToFile/
    // loadSessionFromFile in PluginProcessor.cpp per il formato esatto.
    bool saveSessionToFile (const juce::File& file);
    bool loadSessionFromFile (const juce::File& file);

private:
    /** Costruisce il juce::ValueTree (stato apvts + metadata dei 64 slot,
        piu' il testo del .csd SOLO se includeCsdText) condiviso da
        getStateInformation (includeCsdText=true: li' e' l'unico posto dove
        il codice vive) e saveSessionToFile (includeCsdText=false: nel file
        il codice e' GIA' scritto in chiaro prima del tag d'appendice,
        ripeterlo anche nell'XML sarebbe solo rumore ridondante). */
    // Non const: juce::AudioProcessorValueTreeState::copyState() (chiamato
    // dentro) non e' const in JUCE (prende un lock interno), anche se
    // logicamente questo metodo non modifica lo stato del processor.
    juce::ValueTree buildStateTree (bool includeCsdText = true);

    /** Applica un juce::ValueTree prodotto da buildStateTree allo stato
        corrente (apvts, metadata dei 64 slot), condiviso da
        setStateInformation e loadSessionFromFile. csdTextToRestore e' il
        testo del .csd da usare - passato esplicitamente dal chiamante
        (invece di leggerlo da state.getProperty("csd",...)) cosi'
        loadSessionFromFile puo' usare il testo LETTERALE che precede il tag
        <CsoundStudioParams> nel file, che resta la fonte di verita' per il
        codice anche se qualcuno modifica il .csd a mano senza toccare il
        tag. Ricompila subito se il motore e' gia' in esecuzione (stesso
        schema di setStateInformation - vedi il commento li' sul perche'). */
    void restoreStateFromTree (const juce::ValueTree& state, const juce::String& csdTextToRestore);

    // Svuota TUTTI gli slot Float/Int/Bool/Choice (channelName tornato
    // vuoto = non assegnato, come un plugin appena istanziato) - chiamata
    // da loadSessionFromFile PRIMA di qualunque altra cosa, incondizionata:
    // il nuovo .csd potrebbe non definire nessun parametro (un .csd
    // "normale" scritto a mano, senza <CsoundStudioParams> ne' <Cabbage>),
    // quindi senza questo reset preventivo i mapping della sessione
    // PRECEDENTE resterebbero appesi a canali che il nuovo file magari non
    // usa nemmeno piu'. I rami che DEFINISCONO davvero dei parametri
    // (restoreStateFromTree, importCabbageParameters) sovrascrivono questi
    // slot vuoti con quelli veri subito dopo - il reset qui e' quindi
    // ridondante ma innocuo in quei casi, e l'unica protezione nel caso
    // "nessun parametro nel file".
    void resetAllParameterSlots();

    // Import automatico dei parametri da un .csd Cabbage "allo stato
    // brado" - chiamato da loadSessionFromFile quando il file contiene un
    // tag <Cabbage> ma NON il nostro <CsoundStudioParams> (cioe' non e' mai
    // stato salvato da questo plugin): legge le dichiarazioni dei widget
    // Cabbage dentro quel tag e popola i 16 slot Float, i 16 Bool e i 16
    // Choice leggendo channel()/range()/value()/text(), cosi' l'utente non
    // deve reimpostare ogni canale a mano. Mappatura:
    //   hslider/vslider/rslider/nslider -> 1 slot Float
    //     channel("nome"), range(min, max, default, skew, increment)
    //   vrange/hrange                   -> 2 slot Float (stesso min/max/
    //     skew/increment, default separati)
    //     channel("nomeA", "nomeB"), range(min, max, defaultA:defaultB, skew, increment)
    //   checkbox                        -> 1 slot Bool
    //     channel("nome"), value(0 o 1)
    //   combobox                        -> 1 slot Choice
    //     channel("nome"), text("A", "B", ...), value(indice di default, letto letteralmente)
    //   listbox                         -> nessun parametro (ignorato)
    // Resetta PRIMA tutti gli slot Float/Bool/Choice (un nuovo import
    // rimpiazza la mappatura precedente, non la somma) - gli slot Int non
    // sono toccati (Cabbage non ha un equivalente diretto nella tabella
    // sopra). Ritorna false se non e' stato trovato nessun tag <Cabbage>.
    bool importCabbageParameters (const juce::String& csdText);

    // Costruisce il layout COMPLETO di apvts: i 16 parametri float (vedi
    // ChannelHostParameter), i 16 interi (IntHostParameter), i 16 booleani
    // (BoolHostParameter) e i 16 a scelta multipla (ChoiceHostParameter), in
    // quest'ordine (Float, Int, Bool, Choice - lo stesso ordine delle tab
    // nell'editor dei parametri). Non puo' essere static: ognuno di questi
    // tiene un riferimento all'owner (per leggere lo slot corrente in
    // getName/getText - vedi sopra), quindi serve *this.
    juce::AudioProcessorValueTreeState::ParameterLayout createChannelParamLayout();

    // Spinge verso Csound (csoundSetControlChannel) il valore denormalizzato
    // di tutti gli slot assegnati a un canale: chiamata una volta per
    // blocco audio, PRIMA del ciclo a ksmps in processBlock(). Gira quindi
    // sul thread audio: channelParamSlotsLock e' un compromesso pragmatico
    // (stesso stile di messageHistoryLock/csdTextLock altrove in questo
    // file) - una sezione critica breve, niente I/O dentro, accettabile per
    // ora; una coda lock-free resta l'upgrade naturale se servissero
    // garanzie real-time piu' rigorose.
    void pushChannelParametersToCsound (CSOUND* cs);

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
