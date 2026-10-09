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

        // Chiamato (sul message thread, in modo asincrono) dopo che
        // setStateInformation ha ripristinato una sessione: l'editor, se
        // aperto, deve rileggere codice/mappatura dal processor e mostrare
        // l'eventuale avviso "file non trovato" (vedi isLinkedFileMissing()).
        // Nessun dialogo modale puo' partire da setStateInformation: l'host
        // puo' chiamarla senza UI, durante un bounce offline, o all'apertura
        // di un progetto con molti plugin - per questo e' un avviso in
        // editor, non una finestra.
        virtual void sessionStateRestored() {}

        // Chiamato (sul message thread) da un'azione di undo/redo della
        // cronologia di sessione (vedi getSessionUndoManager): le azioni
        // vivono nel PROCESSOR e sopravvivono alla chiusura dell'editor,
        // quindi non possono riferirsi a componenti UI - notificano qui, e
        // pannello/editor (se aperti) si aggiornano. structureReplaced = true
        // per una sostituzione di sessione intera (Load/Initialize/Relocate):
        // il pannello scarta anche una riga "in sospeso".
        virtual void sessionEditedByUndoRedo (bool /*structureReplaced*/) {}
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
    static constexpr int maxChoiceOptions = 16; // esteso da 8 a 16 (richiesta esplicita)

    struct ChoiceParamSlot
    {
        juce::String channelName; // vuoto = non assegnato
        juce::StringArray optionLabels; // fino a maxChoiceOptions etichette; vuote = fallback "<unassigned>" (richiesta esplicita, vedi getChoiceOptionLabel)
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

    // Il testo CODICE = SOLO il blocco <CsoundSynthesizer>...</CsoundSynthesizer>
    // (richiesta esplicita: "filtrare tutto tranne <CsoundSynthesizer>"): e'
    // cio' che l'editor mostra e che viene compilato. Tutto cio' che nel
    // file sta PRIMA (es. una sezione <Cabbage>, commenti) o DOPO il blocco
    // (escluso il nostro <CsoundParams>) viene conservato a parte, verbatim,
    // e riscritto al suo posto da buildSessionTextFor() - vedi
    // setCsdCodeFromFullText(). Csound ignora comunque le sezioni che non
    // riconosce, quindi compilare il solo blocco e' equivalente.
    juce::String getCsdText() const;
    void setCsdText (const juce::String& text);

    // Divide un testo .csd completo (gia' SENZA <CsoundParams>) in
    // preambolo / blocco <CsoundSynthesizer> / coda e imposta csdText al
    // solo blocco, conservando preambolo e coda (vedi getCsdText). Se il
    // tag non c'e', tutto il testo e' "codice" e le due parti restano vuote.
    void setCsdCodeFromFullText (const juce::String& fullCode);

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

    // Persistenza dello stato nel progetto dell'host - NUOVA logica
    // (richiesta esplicita, in sostituzione del vecchio blob con codice +
    // struttura + apvts tutto dentro): lo stato contiene
    //
    //   <PluginState csd="FFTFreeze/FFTFreeze.csd" csdIsRelative="1"
    //                dirty="0" baseline="sha256..." fileModTime="...">
    //     <EmbeddedCsd> ...copia completa della SESSIONE .csd... </EmbeddedCsd>
    //     <P channel="Frame1" type="f" value="2"/>
    //     ...
    //   </PluginState>
    //
    //   - csd: path del file .csd collegato, RELATIVO alla cartella base
    //     (getBaseFolder(), ~/Documents/apeCsound) se il file sta li'
    //     dentro, altrimenti assoluto (csdIsRelative="0") come fallback;
    //     vuoto se la sessione non e' collegata a nessun file.
    //   - EmbeddedCsd: copia di SCORTA di cio' che il progetto suonava
    //     quando e' stato salvato - usata SOLO se il file non c'e' piu'.
    //   - P: i VALORI correnti dei parametri assegnati, per NOME CANALE (non
    //     per indice di slot) in unita' reali, cosi' sopravvivono a un
    //     riordino degli slot nel file. Un parametro presente nel file ma
    //     non nel progetto riparte dal suo default.
    //
    // La STRUTTURA dei parametri (min/max/default/skew/step/opzioni) NON e'
    // piu' nello stato: vive SOLO dentro il tag <CsoundParams> del .csd
    // (su disco, o nella copia incorporata), vedi saveSessionToFile/
    // buildSessionText().
    //
    // Modello (semplificato, richiesta esplicita: "e' troppo articolato"):
    // IL FILE E' LA VERITA', salvo modifiche non ancora salvate su file.
    // Ripristino (setStateInformation), SENZA MAI un dialogo modale:
    //   file esiste, sessione pulita al salvataggio -> il file da disco
    //   file esiste, sessione "•" al salvataggio    -> copia incorporata +
    //        bozza dell'editor (<EditorDraft>), "•" resta acceso - le
    //        modifiche non salvate su file non si perdono
    //   file mancante                  -> copia incorporata + avviso in editor
    //                                     ("file non trovato", Relocate/Save As)
    //   nessun path (istanza nuova)    -> template di default
    // Attributi aggiuntivi: dirty (stato "•"), baseline (hash della sessione
    // all'ultimo Save/Load) e fileModTime (data del file a quel momento), per
    // far ripartire "•" e il controllo "modificato da fuori" da dove erano.
    // L'esito e' esposto con isLinkedFileMissing() e notificato con
    // Listener::sessionStateRestored(). Un vecchio stato APE_CSOUND_STATE
    // (versioni precedenti) viene ancora letto, come sessione non collegata.
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Sessione collegata a un file .csd -----------------------------
    // NON crea la cartella (punto 10: prima la creava a ogni salvataggio del
    // progetto): la crea solo il file chooser dell'editor, quando serve.
    // Cartella base FISSA per i path relativi: ~/Documents/apeCsound su
    // macOS (userDocumentsDirectory), la Documents dell'app su iOS (stessa
    // chiamata JUCE, dentro la sandbox - un App Group richiederebbe un
    // entitlement da aggiungere al progetto). Creata se manca.
    static juce::File getBaseFolder();

    // File .csd a cui la sessione e' collegata (Save sovrascrive QUESTO,
    // vedi CsoundAudioProcessorEditor::performSaveLinked) - juce::File{}
    // = non collegata (istanza nuova, Initialize Session, vecchio stato).
    // Impostato da saveSessionToFile/loadSessionFromFile/setStateInformation,
    // o esplicitamente da setLinkedCsdFile ("Relink" dall'avviso).
    juce::File getLinkedCsdFile() const;
    void setLinkedCsdFile (const juce::File& file);
    bool isSessionLinked() const { return getLinkedCsdFile() != juce::File{}; }

    // Path come viene scritto nello stato (relativo alla base se possibile)
    // - per i messaggi in consolle/avviso.
    juce::String getLinkedCsdDisplayPath() const;

    // Testo COMPLETO della sessione come lo scriverebbe saveSessionToFile:
    // codice (getCsdText()) + "\n\n<CsoundParams>" + struttura + "</CsoundParams>".
    // E' anche cio' che viene incorporato nello stato e di cui si calcola
    // l'hash. Non const per lo stesso motivo di buildParamsStructureTree.
    juce::String buildSessionText();

    // Stessa cosa, ma con un codice passato dal chiamante al posto di
    // getCsdText(): l'editor la usa con il testo ATTUALE del document (anche
    // non ancora applicato) per confrontarlo col file su disco.
    juce::String buildSessionTextFor (const juce::String& codeText);

    // Hash della sessione (buildSessionText()) com'era subito DOPO l'ultimo
    // Save/Load/ripristino riuscito - la "baseline" rispetto a cui l'editor
    // decide se la sessione e' stata modificata (codice o struttura dei
    // parametri). Le modifiche ESTERNE al file si rilevano invece con la
    // data di modifica (hasLinkedFileChangedOnDisk), non col contenuto.
    juce::String getSessionBaselineHash() const;

    // Carica una sessione da TESTO (stesso formato di un file .csd salvato
    // da questo plugin, o un .csd "normale"/Cabbage) - corpo ex
    // loadSessionFromFile, che ora legge il file e delega qui. NON tocca il
    // collegamento al file.
    bool loadSessionFromText (const juce::String& fullText);

    // SHA-256 esadecimale di un testo (juce::SHA256) - usato per hash nello
    // stato e per il confronto disco/progetto.
    static juce::String computeTextHash (const juce::String& text);

    // Legge un .csd da disco normalizzando i fine riga a "\n" (CRLF/CR ->
    // LF): e' la forma in cui il testo vive in memoria, viene hashato e
    // viene riscritto da saveSessionToFile - cosi' un file scritto da
    // Windows/altri editor non risulta "diverso" solo per i fine riga.
    // Usarla OVUNQUE si legge un .csd per confrontarlo (performSaveLinked).
    static juce::String readSessionFile (const juce::File& file);

    // true se l'ultimo ripristino ha trovato un path ma NON il file: e' in
    // uso la copia incorporata, e l'editor mostra l'unico avviso previsto
    // ("file non trovato" + Save As, che ricrea il file e azzera questo
    // flag tramite saveSessionToFile). Azzerato anche da Load/Initialize.
    bool isLinkedFileMissing() const;

    // true se il file collegato risulta modificato DA FUORI dopo l'ultimo
    // Save/Load/ripristino: confronto della DATA DI MODIFICA del file (non
    // del contenuto - BUG corretto: l'hash del contenuto risentiva di fine
    // riga/codifica e poteva differire anche senza alcuna modifica esterna,
    // facendo chiedere "sovrascrivere?" a ogni Save). E' la stessa tecnica
    // di qualunque editor di testo. performSaveLinked lo usa per l'unica
    // conferma rimasta prima di sovrascrivere.
    bool hasLinkedFileChangedOnDisk() const;

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
    // il codice, dentro un tag <CsoundParams>...</CsoundParams>
    // creato apposta: Csound analizza un .csd cercando i tag <CsOptions>/
    // <CsInstruments>/<CsScore> per nome, quindi ignora senza problemi
    // qualunque tag sconosciuto dopo </CsoundSynthesizer> - il file resta
    // un .csd legittimo anche per chi non ha questo plugin, semplicemente
    // senza il mapping dei parametri. Vedi saveSessionToFile/
    // loadSessionFromFile in PluginProcessor.cpp per il formato esatto.
    // codeText: il codice da scrivere. L'editor passa il testo ATTUALE del
    // suo document (anche se non ancora applicato con Apply) - BUG corretto:
    // prima si scriveva getCsdText(), cioe' l'ultimo codice COMPILATO, quindi
    // dopo un Save con modifiche non applicate il file non conteneva le
    // modifiche e l'indicatore restava "unsaved changes". Il testo in
    // esecuzione (getCsdText()) NON viene toccato: Apply resta l'unico modo
    // per compilare.
    bool saveSessionToFile (const juce::File& file, const juce::String& codeText);
    bool loadSessionFromFile (const juce::File& file);

    // "Initialize Session" del menu hamburger (richiesta esplicita: "pulisce
    // tutto e carica il CSD hard coded") - stesso spirito di
    // loadSessionFromFile, ma senza un file: svuota la mappatura dei 4 tipi
    // di parametro (resetAllParameterSlots(), IDENTICO comportamento di
    // "Remove Parameters") e sostituisce il codice con defaultCsdText()
    // (lo stesso testo usato per il primissimo avvio del plugin, prima di
    // qualunque sessione salvata/caricata - vedi il costruttore). NON
    // tocca il documento dell'editor di codice (quello e' testo UI, vive
    // solo in CsoundAudioProcessorEditor): il chiamante (vedi
    // CsoundAudioProcessorEditor::performInitializeSession()) deve ancora
    // rileggere getCsdText() e aggiornare document/parameterPanel, come fa
    // gia' dopo loadSessionFromFile.
    void initializeSession();

    // --- Bozza dell'editor ---------------------------------------------
    // Il testo che l'utente vede nell'editor di codice, anche NON ancora
    // applicato con Apply (BUG corretto: prima viveva SOLO nel document
    // dell'editor, quindi chiudere la finestra del plugin o salvare il
    // progetto della DAW perdeva ogni modifica non applicata). L'editor la
    // aggiorna a ogni modifica del document; alla (ri)apertura la rilegge con
    // getEditorDraft(). Azzerata a ogni caricamento di codice (Load,
    // ripristino, Initialize - vedi setCsdCodeFromFullText): da li'
    // getEditorDraft() torna a coincidere con getCsdText().
    void setEditorDraft (const juce::String& code);
    juce::String getEditorDraft() const;

    // true se la sessione com'e' ADESSO (bozza dell'editor + struttura dei
    // parametri) differisce dall'ultimo Save/Load, oppure non e' collegata a
    // nessun file, oppure il file collegato non si trova. Unica fonte per
    // il "•" nella toolbar, per il dialogo di Load CSD e per l'attributo
    // "dirty" nello stato del progetto (vedi getStateInformation).
    bool isSessionDirty();

    // Scollega la sessione da qualunque file (diventa "Untitled"): usata da
    // Initialize Session e dall'import di un .csd incollato nell'editor.
    void unlinkSession();

    // Riporta il VALORE di ogni parametro assegnato al suo default/init
    // (slot.defaultValue/defaultIndex) via setValueNotifyingHost - chiamata
    // a ogni caricamento di una struttura (Load CSD, incolla, ripristino;
    // in quest'ultimo i valori salvati nel progetto vengono riapplicati
    // subito dopo) e dalla voce "Reset to INIT Values" del pannello. BUG
    // corretto: prima un Load lasciava i valori normalizzati del CSD
    // precedente sugli slot, rimappati sui range nuovi.
    void resetParameterValuesToDefaults();

    // true se al ripristino dal progetto la sessione era "•" (modifiche non
    // salvate su file): la Relocate... la usa per chiedere conferma prima di
    // sostituire un contenuto che non esiste su nessun file. Azzerata da
    // Save/Load/Initialize/unlink.
    bool wasRestoredDirty() const;

    // Istantanea di tutto cio' che un caricamento di sessione (Load CSD,
    // Initialize, incolla, Relocate) cambia OLTRE al testo dell'editor - per
    // renderlo annullabile come UN solo passo coerente (BUG corretto:
    // l'undo ripristinava solo il testo, lasciando collegamento al file e
    // struttura dei parametri quelli nuovi). Esclusi di proposito: il
    // codice in esecuzione (l'undo non ricompila: Apply resta all'utente,
    // bordo rosso) e i VALORI dei parametri (gestiti dalla DAW, mai nella
    // cronologia del plugin).
    struct SessionSnapshot
    {
        juce::ValueTree structure;
        juce::String preamble, postamble;
        juce::File linkedFile;
        juce::Time linkedModTime;
        juce::String baselineHash;
        bool fileMissing = false;
        bool restoredDirty = false;

        // Valori normalizzati 0..1 di TUTTI i parametri dei 4 pool, nello
        // stesso ordine (Float, Int, Bool, Choice): servono SOLO all'undo/redo
        // di una sostituzione di sessione (BUG corretto: annullare un Load
        // lasciava sui parametri i default del file appena annullato). Le
        // modifiche ai valori dal pannello/DAW restano fuori dalla cronologia.
        std::vector<float> values;
    };

    SessionSnapshot captureSessionSnapshot();
    void restoreSessionSnapshot (const SessionSnapshot& snapshot);

    // --- Cronologia di undo e documento del codice ----------------------
    // Vivono nel PROCESSOR, non nell'editor (BUG corretto, richiesta
    // esplicita: "quando chiudo l'editor del plugin e lo riapro, l'undo
    // history si perde" - prima erano membri dell'editor, distrutti alla
    // chiusura della finestra). L'editor li usa per riferimento. Da usare
    // SOLO sul message thread (juce::CodeDocument non e' thread-safe): per le
    // letture da altri thread c'e' la bozza (getEditorDraft).
    juce::CodeDocument& getCodeDocument()     { return codeDocument; }
    juce::UndoManager&  getSessionUndoManager() { return sessionUndoManager; }

    // Guardia di rientranza del ponte document -> cronologia condivisa (vedi
    // CsoundAudioProcessorEditor::bridgeCodeEditIntoSharedUndo): vive qui
    // perche' le azioni in cronologia la referenziano anche a editor chiuso.
    bool& getCodeUndoGuard() { return codeUndoGuard; }

    // Vedi Listener::sessionEditedByUndoRedo. Solo message thread.
    void notifySessionEditedByUndoRedo (bool structureReplaced);

    // true (una volta sola) se dall'ultima apertura dell'editor la sessione
    // e' stata sostituita dall'host (setStateInformation) o se e' la prima
    // apertura: l'editor deve ricaricare il documento dalla bozza e azzerare
    // la cronologia. Altrimenti documento e cronologia restano quelli di
    // prima della chiusura.
    bool consumeDocumentResyncRequest() { return documentResyncRequested.exchange (false); }

    // Hash della sessione (bozza + struttura) subito dopo l'ultimo
    // ripristino dal progetto: Relocate... lo confronta con lo stato attuale
    // per sapere se ci sono modifiche fatte DOPO il ripristino. Nel
    // processor (non nell'editor) cosi' sopravvive alla chiusura della finestra.
    juce::String getRestoredSessionHash() const;

private:
    // Corpo di loadSessionFromText SENZA il reset dei valori ai default
    // (vedi resetParameterValuesToDefaults): carica struttura + codice.
    bool loadSessionStructureAndCode (const juce::String& fullText);

    // Applica SOLO la struttura dei parametri (slot) da un ValueTree prodotto
    // da buildParamsStructureTree - usata da restoreStateFromTree e da
    // restoreSessionSnapshot.
    void applyParamsStructureTree (const juce::ValueTree& state);
    /** Costruisce il juce::ValueTree con la SOLA STRUTTURA dei parametri
        (metadata dei 64 slot: nome canale/range/skew/increment/default/
        opzioni) - e' cio' che finisce nel tag <CsoundParams> del .csd (vedi
        buildSessionText()). Niente apvts ne' testo del codice: i VALORI
        vivono nello stato del progetto per nome canale (getStateInformation),
        il codice e' scritto in chiaro prima del tag. */
    juce::ValueTree buildParamsStructureTree();

    /** Applica un juce::ValueTree prodotto da buildParamsStructureTree (o da
        una versione precedente del plugin, che includeva anche un figlio
        PARAMETERS di apvts: se c'e' viene ancora applicato, per compatibilita')
        ai 64 slot, e imposta il testo del codice. csdTextToRestore e' il
        testo LETTERALE che precede il tag <CsoundParams>, che resta la fonte
        di verita' per il codice anche se qualcuno modifica il .csd a mano
        senza toccare il tag. Ricompila subito se il motore e' gia' in
        esecuzione (vedi scheduleRecompileAfterRestore). */
    void restoreStateFromTree (const juce::ValueTree& state, const juce::String& csdTextToRestore);

    // Coda della logica di ripristino: segna pendingStateRestore e, se il
    // motore e' gia' in esecuzione, ricompila subito (via callAsync) il
    // testo appena ripristinato - vedi il commento esteso nel .cpp.
    void scheduleRecompileAfterRestore (const juce::String& restoredCsd);

    // Imposta in UN solo passaggio il valore di ogni parametro assegnato:
    // quello salvato nel progetto (<P channel type value/>, per nome canale)
    // se c'e', altrimenti il default dello slot. Va chiamata DOPO che la
    // struttura e' stata ripristinata. Sostituisce "reset ai default + valori
    // salvati", che notificava l'host due volte per parametro (punto 7).
    void applyRestoredValues (const juce::XmlElement& pluginStateXml);

    // Svuota TUTTI gli slot Float/Int/Bool/Choice (channelName tornato
    // vuoto = non assegnato, come un plugin appena istanziato) - chiamata
    // da loadSessionFromFile PRIMA di qualunque altra cosa, incondizionata:
    // il nuovo .csd potrebbe non definire nessun parametro (un .csd
    // "normale" scritto a mano, senza <CsoundParams> ne' <Cabbage>),
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
    // tag <Cabbage> ma NON il nostro <CsoundParams> (cioe' non e' mai
    // stato salvato da questo plugin): legge le dichiarazioni dei widget
    // Cabbage dentro quel tag e popola i 16 slot Float, i 16 Bool e i 16
    // Choice leggendo channel()/range()/value()/text(), cosi' l'utente non
    // deve reimpostare ogni canale a mano. Mappatura:
    //   hslider/vslider/rslider/nslider -> 1 slot Float
    //     channel("nome"), range(min, max, default, skew, increment)
    //   vrange/hrange                   -> 2 slot Float (stesso min/max/
    //     skew/increment, default separati)
    //     channel("nomeA", "nomeB"), range(min, max, defaultA:defaultB, skew, increment)
    //   xypad                           -> 2 slot Float (uno per asse,
    //     min/max/default INDIPENDENTI per asse - niente sintassi "A:B" qui,
    //     ciascun asse ha gia' il proprio range(); skew/increment NON sono
    //     esposti da questo widget in Cabbage, fissi a 1.0/0.001 per
    //     entrambi gli assi, mai letti dalla riga)
    //     channel("nomeX", "nomeY"), rangeX(minX, maxX, defaultX), rangeY(minY, maxY, defaultY)
    //   encoder (endless encoder)       -> 1 slot Float, min/max da due
    //     proprieta' SEPARATE (non range()); default non esposto da questo
    //     widget in Cabbage - assunto uguale a min; skew/increment fissi a
    //     1.0/0.001 come xypad sopra, mai letti dalla riga
    //     channel("nome"), min(min), max(max)
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
    // Testo del file PRIMA e DOPO il blocco <CsoundSynthesizer> (es. una
    // sezione <Cabbage>), conservato verbatim e riscritto da
    // buildSessionTextFor() - vedi setCsdCodeFromFullText(). Stesso lock
    // di csdText.
    juce::String csdPreamble, csdPostamble;

    // Documento del codice + cronologia di sessione - vedi getCodeDocument().
    // Ordine di dichiarazione voluto: la cronologia (che contiene azioni con
    // riferimenti al documento) viene distrutta PRIMA del documento.
    juce::CodeDocument codeDocument;
    juce::UndoManager sessionUndoManager;
    bool codeUndoGuard = false;
    std::atomic<bool> documentResyncRequested { true }; // prima apertura: sincronizza
    juce::String restoredSessionHash;                    // vedi getRestoredSessionHash() - sotto sessionLock

    // Vedi setEditorDraft()/getEditorDraft(). hasEditorDraft distingue "nessuna
    // bozza" da "bozza vuota" (l'utente ha cancellato tutto). Stesso lock di
    // csdText.
    juce::String editorDraft;
    bool hasEditorDraft = false;
    mutable juce::CriticalSection csdTextLock;

    // Sessione collegata a un file - vedi getLinkedCsdFile() e il commento
    // su getStateInformation(). Tutti protetti da sessionLock perche'
    // setStateInformation puo' arrivare da un thread qualsiasi dell'host
    // mentre l'editor legge dal message thread.
    juce::File linkedCsdFile;
    juce::Time linkedFileModTimeAtLoad;       // vedi hasLinkedFileChangedOnDisk()
    juce::String sessionBaselineHash;         // vedi getSessionBaselineHash()
    bool linkedFileMissing = false;           // vedi isLinkedFileMissing()
    bool restoredDirty = false;               // vedi wasRestoredDirty()

    // Le lambda passate a juce::MessageManager::callAsync catturano `this`:
    // se l'host distrugge il processor prima che il messaggio venga eseguito,
    // accederebbero a un oggetto distrutto. Ogni lambda cattura anche una
    // COPIA di questo flag e non fa nulla se e' diventato false - il
    // distruttore lo azzera per primo (stesso thread dei messaggi su cui
    // girano le lambda, quindi nessuna corsa). Un flag condiviso invece di
    // juce::WeakReference per non dipendere da come la classe base
    // juce::AudioProcessor dichiara (o no) la propria master reference.
    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>> (true);
    mutable juce::CriticalSection sessionLock;

    // Ricalcola sessionBaselineHash dallo stato ATTUALE (da chiamare dopo
    // ogni load/ripristino riuscito, FUORI da sessionLock: buildSessionText
    // prende i lock degli slot).
    void updateSessionBaselineHash();

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
