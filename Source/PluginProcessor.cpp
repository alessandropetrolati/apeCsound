#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdlib>

namespace
{
    // Csound carica gli opcode "plugin" aggiuntivi (osc, pvsops, midi, ecc.
    // - i file in CsoundLib64.framework/Resources/Opcodes64, separati dal
    // core) da una cartella di default compilata staticamente dentro il
    // framework come path ASSOLUTO (/Applications/Csound/CsoundLib64.
    // framework/Resources/Opcodes64 - verificato con 'strings' sul binario
    // reale). Siccome il framework e' solo EMBEDDATO nel bundle del plugin,
    // non installato su /Applications sulla macchina di chi lo riceve,
    // senza questo passo Csound cercherebbe quegli opcode in una cartella
    // inesistente. In Csound 7 non serve piu' un trucco con variabili
    // d'ambiente (OPCODE6DIR64, usato per errore in una versione precedente
    // di questo file quando il framework copiato era ancora Csound 6.18):
    // csoundCreate() accetta direttamente un path di override come secondo
    // argomento (vedi doc in csound.h) - questa funzione calcola quel path,
    // puntando alla copia di Opcodes64 che viaggia dentro il bundle stesso
    // (.../Contents/Frameworks/CsoundLib64.framework/Resources/Opcodes64).
   #if JUCE_MAC
    juce::String getEmbeddedOpcodeDir()
    {
        const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

        // exe e' .../<Bundle>.{app,vst3,component}/Contents/MacOS/<Nome>:
        // risaliamo a Contents e scendiamo in Frameworks/...
        const auto contents = exe.getParentDirectory().getParentDirectory();
        const auto opcodeDir = contents.getChildFile ("Frameworks/CsoundLib64.framework/Resources/Opcodes64");

        return opcodeDir.isDirectory() ? opcodeDir.getFullPathName() : juce::String();
    }
   #else
    juce::String getEmbeddedOpcodeDir() { return {}; }
   #endif
}

CsoundAudioProcessor::CsoundAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                 .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    csdText = defaultCsdText();
}

CsoundAudioProcessor::~CsoundAudioProcessor()
{
    stopEngine();
}

//==============================================================================
const juce::String CsoundAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

void CsoundAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    hostBlockSize  = samplesPerBlock;

    // Se il motore e' gia' attivo (es. l'host richiama prepareToPlay per un
    // cambio di sample rate/buffer size mentre il plugin sta suonando) lo
    // lasciamo cosi' com'e': ricompilare qui causerebbe un glitch udibile a
    // meta' esecuzione.
    //
    // Se invece non e' attivo - sempre vero al primo caricamento, e anche
    // quando una DAW ricarica un progetto salvato - lo avviamo da solo con
    // l'ultimo codice conosciuto, cosi' il plugin suona di nuovo senza dover
    // premere "RUN" a mano ad ogni reload del progetto.
    //
    // pendingStateRestore e' vero se setStateInformation ha ripristinato il
    // .csd salvato PRIMA che arrivasse questa prepareToPlay (l'ordine tra le
    // due chiamate dipende dall'host, non e' garantito): in tal caso va
    // ricompilato anche se per qualche motivo il motore risultasse gia'
    // attivo, perche' altrimenti suonerebbe ancora il .csd di default.
    if (pendingStateRestore.exchange (false) || ! isEngineRunning())
        compileAndStart (getCsdText());
}

void CsoundAudioProcessor::releaseResources()
{
    stopEngine();
}

bool CsoundAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto outSet = layouts.getMainOutputChannelSet();

    if (outSet != juce::AudioChannelSet::stereo() && outSet != juce::AudioChannelSet::mono())
        return false;

    // Il bus di Input puo' essere disattivato dall'host (niente audio in
    // ingresso), ma se e' attivo deve avere lo stesso numero di canali
    // dell'output: evita mismatch quando scriviamo spin/spout per canale.
    const auto inSet = layouts.getMainInputChannelSet();

    if (! inSet.isDisabled() && inSet != outSet)
        return false;

    return true;
}

//==============================================================================
void CsoundAudioProcessor::compileAndStart (const juce::String& newCsdText)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    // suspendProcessing() dice all'host/wrapper di non chiamare processBlock()
    // mentre sostituiamo il motore: e' la protezione minima per evitare che
    // il thread audio legga un CSOUND* appena distrutto. Per garanzie
    // real-time piu' rigorose (Fase 4+) l'istanza vecchia andrebbe passata
    // a una coda e distrutta dopo qualche ciclo, non subito qui.
    suspendProcessing (true);

    if (auto* old = activeCsound.exchange (nullptr))
    {
        ready = false;
        csoundReset (old);
        csoundDestroy (old);
    }

    setCsdText (newCsdText);

    // csoundInitialize() e' idempotente: e' sicuro chiamarla ogni volta che
    // creiamo una nuova istanza (mirror del pattern usato in csGrain).
    csoundInitialize (CSOUNDINIT_NO_ATEXIT);

    // Il secondo argomento di csoundCreate() (opcodedir) sovrascrive la
    // cartella di default - assoluta e quindi inutilizzabile in un bundle
    // distribuito - da cui Csound carica gli opcode plugin extra. Vedi
    // getEmbeddedOpcodeDir() piu' sopra.
    const auto opcodeDir = getEmbeddedOpcodeDir();
    auto* cs = csoundCreate (nullptr, opcodeDir.isNotEmpty() ? opcodeDir.toRawUTF8() : nullptr);

    if (cs == nullptr)
    {
        handleMessage ("Error: could not create the Csound instance.");
        suspendProcessing (false);
        return;
    }

    csoundSetHostData (cs, this);
    csoundSetMessageCallback (cs, messageCallback);

    // Dice a Csound che l'I/O audio e' gestito dall'host (noi, tramite
    // processBlock/spin/spout), non da un device che Csound apre da solo.
    // In Csound 7 questa funzione non prende piu' argomenti extra.
    csoundSetHostAudioIO (cs);

    // Idem per il MIDI: i 6 callback sotto collegano i messaggi che arrivano/
    // partono da processBlock() (host o wrapper Standalone) al motore Csound,
    // che li legge/scrive con gli opcode midiin/midiout standard.
    csoundSetHostMIDIIO (cs);
    csoundSetExternalMidiInOpenCallback   (cs, midiInOpenCallback);
    csoundSetExternalMidiReadCallback     (cs, midiInReadCallback);
    csoundSetExternalMidiInCloseCallback  (cs, midiInCloseCallback);
    csoundSetExternalMidiOutOpenCallback  (cs, midiOutOpenCallback);
    csoundSetExternalMidiWriteCallback    (cs, midiOutWriteCallback);
    csoundSetExternalMidiOutCloseCallback (cs, midiOutCloseCallback);

    incomingMidiQueue.clearQuick();
    outgoingMidiBuffer = nullptr;

    csoundSetOption (cs, "-+rtaudio=null");
    csoundSetOption (cs, "-+rtmidi=null");

    // -M0/-Q0 sono quello che davvero "attiva" il motore MIDI realtime di
    // Csound: senza, Csound non chiama MAI midiInOpenCallback/
    // midiOutOpenCallback (i callback da soli, con -+rtmidi=null, non
    // bastano - servono solo a dirgli di non usare un device reale, ma e'
    // -M/-Q a dirgli che un ingresso/uscita MIDI esiste e va aperto). Il
    // numero dopo -M/-Q e' ignorato quando l'I/O e' host-implemented: viene
    // comunque instradato ai nostri 6 callback esterni.
    csoundSetOption (cs, "-M0");
    csoundSetOption (cs, "-Q0");

    csoundSetOption (cs, "-d");            // niente finestre di display/grafici
    csoundSetOption (cs, "-+msg_color=0"); // niente codici colore ANSI nei messaggi (rovinerebbero la console)

    // Forziamo sr e ksmps a coincidere con host sample rate e block size:
    // cosi' non serve resampling e il ciclo in processBlock() e' quasi
    // sempre un singolo passaggio. Il .csd puo' comunque dichiarare valori
    // diversi in <CsInstruments>: qui li sovrascriviamo deliberatamente.
    csoundSetOption (cs, ("--sample-rate=" + juce::String (hostSampleRate, 0)).toRawUTF8());
    csoundSetOption (cs, ("--ksmps=" + juce::String (hostBlockSize)).toRawUTF8());
    csoundSetOption (cs, ("--nchnls=" + juce::String (2)).toRawUTF8());
    
    // csoundCompileCsdText non esiste piu' in Csound 7: csoundCompileCSD
    // con mode=1 fa lo stesso lavoro (il secondo argomento e' codice CSD
    // testuale, non un path di file), async=0 perche' qui csoundStart viene
    // chiamata DOPO (vedi la doc di csoundCompileCSD in csound.h: in questo
    // ordine <CsOptions>/<CsScore> vengono preprocessati normalmente).
    const int compileResult = csoundCompileCSD (cs, newCsdText.toRawUTF8(), 1, 0);

    if (compileResult != CSOUND_SUCCESS)
    {
        handleMessage ("Compilation failed (code " + juce::String (compileResult) + "). Check the console for details.");
        csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    if (csoundStart (cs) != CSOUND_SUCCESS)
    {
        handleMessage ("Could not start the Csound engine.");
        csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    // csoundGetNchnls/csoundGetNchnlsInput non esistono piu': un'unica
    // csoundGetChannels(cs, isInput) le sostituisce entrambe.
    csKsmps            = csoundGetKsmps (cs);
    csNumChannels      = (int) csoundGetChannels (cs, 0);
    csInputChannels    = (int) csoundGetChannels (cs, 1);
    spoutReadPos       = 0;
    samplesLeftInBlock = 0;

    if (csKsmps <= 0 || csNumChannels <= 0)
    {
        handleMessage ("Invalid audio configuration (ksmps/nchnls). Check <CsInstruments>.");
        csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    activeCsound.store (cs);
    ready = true;

    suspendProcessing (false);

    listeners.call ([] (Listener& l) { l.csoundEngineStarted(); });
}

void CsoundAudioProcessor::stopEngine()
{
    suspendProcessing (true);

    if (auto* old = activeCsound.exchange (nullptr))
    {
        ready = false;
        csoundReset (old);
        csoundDestroy (old);

        incomingMidiQueue.clearQuick();
        outgoingMidiBuffer = nullptr;

        suspendProcessing (false);
        listeners.call ([] (Listener& l) { l.csoundEngineStopped(); });
        return;
    }

    suspendProcessing (false);
}

//==============================================================================
void CsoundAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto* cs = activeCsound.load();
    const int numIn       = getTotalNumInputChannels();
    const int numOut      = getTotalNumOutputChannels();
    const int numSamples  = buffer.getNumSamples();

    if (cs == nullptr || ! ready.load())
    {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    // Messaggi MIDI in ingresso di questo blocco: midiInReadCallback li
    // consuma durante csoundPerformKsmps() qui sotto (stesso thread audio).
    incomingMidiQueue.clearQuick();

    for (const auto metadata : midiMessages)
        incomingMidiQueue.add (metadata.getMessage());

    // I messaggi MIDI generati da Csound in questo blocco (es. midiout)
    // finiscono qui tramite midiOutWriteCallback; alla fine rimpiazzano il
    // contenuto di midiMessages, come e' convenzione in JUCE per i plugin
    // che producono MIDI.
    juce::MidiBuffer outgoing;
    outgoingMidiBuffer = &outgoing;

    int written = 0;

    while (written < numSamples)
    {
        if (samplesLeftInBlock == 0)
        {
            const int framesForThisTick = juce::jmin (csKsmps, numSamples - written);
            cs_float* spin = csoundGetSpin (cs);

            // Audio in ingresso (es. microfono, se il bus Input e' attivo)
            // scritto nello spin buffer per il prossimo tick da csKsmps
            // campioni, letto lato .csd con l'opcode "inch".
            for (int ch = 0; ch < csInputChannels; ++ch)
            {
                const int srcChannel = numIn > 0 ? juce::jmin (ch, numIn - 1) : -1;
                const float* src = srcChannel >= 0 ? buffer.getReadPointer (srcChannel) : nullptr;

                for (int i = 0; i < framesForThisTick; ++i)
                    spin[(size_t) i * (size_t) csInputChannels + (size_t) ch] = src != nullptr ? (cs_float) src[written + i] : 0;

                for (int i = framesForThisTick; i < csKsmps; ++i)
                    spin[(size_t) i * (size_t) csInputChannels + (size_t) ch] = 0; // padding se numSamples non e' multiplo di ksmps
            }

            // Un "tick" di ksmps campioni per ogni canale nel buffer spout
            // (e consumo dello spin appena scritto sopra). midiInReadCallback
            // e midiOutWriteCallback vengono chiamati da Csound qui dentro.
            if (csoundPerformKsmps (cs) != 0)
            {
                // Performance/score terminati: silenzio per il resto del
                // buffer e segnaliamo lo stop (l'istanza va comunque
                // distrutta esplicitamente con Stop, qui ci limitiamo a
                // smettere di generare audio per evitare di leggere uno
                // spout non piu' valido).
                buffer.clear (written, numSamples - written);
                ready = false;

                outgoingMidiBuffer = nullptr;
                midiMessages.swapWith (outgoing);

                juce::MessageManager::callAsync ([this]
                {
                    listeners.call ([] (Listener& l) { l.csoundEngineStopped(); });
                });
                return;
            }

            spoutReadPos       = 0;
            samplesLeftInBlock = csKsmps;
        }

        const int framesToCopy = juce::jmin (samplesLeftInBlock, numSamples - written);
        const cs_float* spout = csoundGetSpout (cs);

        for (int ch = 0; ch < numOut; ++ch)
        {
            auto* dest = buffer.getWritePointer (ch, written);
            const int srcChannel = juce::jmin (ch, csNumChannels - 1);

            for (int i = 0; i < framesToCopy; ++i)
                dest[i] = (float) spout[(size_t) (spoutReadPos + i) * (size_t) csNumChannels + (size_t) srcChannel];
        }

        spoutReadPos       += framesToCopy;
        samplesLeftInBlock -= framesToCopy;
        written            += framesToCopy;
    }

    outgoingMidiBuffer = nullptr;
    midiMessages.swapWith (outgoing);
}

//==============================================================================
juce::AudioProcessorEditor* CsoundAudioProcessor::createEditor()
{
    return new CsoundAudioProcessorEditor (*this);
}

//==============================================================================
void CsoundAudioProcessor::setControlChannel (const juce::String& channelName, double value)
{
    if (auto* cs = activeCsound.load())
        csoundSetControlChannel (cs, channelName.toRawUTF8(), (cs_float) value);
}

double CsoundAudioProcessor::getControlChannel (const juce::String& channelName) const
{
    if (auto* cs = activeCsound.load())
    {
        int err = 0;
        const cs_float value = csoundGetControlChannel (cs, channelName.toRawUTF8(), &err);

        if (err == CSOUND_SUCCESS)
            return (double) value;
    }

    return 0.0;
}

//==============================================================================
juce::String CsoundAudioProcessor::getCsdText() const
{
    const juce::ScopedLock sl (csdTextLock);
    return csdText;
}

void CsoundAudioProcessor::setCsdText (const juce::String& text)
{
    const juce::ScopedLock sl (csdTextLock);
    csdText = text;
}

//==============================================================================
void CsoundAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("CSOUND_STUDIO_STATE");
    state.setProperty ("csd", getCsdText(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CsoundAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);

        if (! state.isValid())
            return;

        const auto restoredCsd = state.getProperty ("csd", getCsdText()).toString();
        setCsdText (restoredCsd);

        // Segnala che c'e' un ripristino in sospeso: se prepareToPlay non e'
        // ancora stato chiamato sara' lui a compilare questo testo (vedi
        // CsoundAudioProcessor::prepareToPlay).
        pendingStateRestore = true;

        // Ma se il motore e' GIA' in esecuzione, prepareToPlay e' gia'
        // passato - quasi certamente con il .csd di default, perche' questo
        // e' esattamente il caso in cui l'host chiama setStateInformation
        // DOPO prepareToPlay. In quel caso non possiamo aspettare: bisogna
        // ricompilare subito con il testo appena ripristinato, altrimenti
        // il plugin continuerebbe a suonare il .csd sbagliato.
        //
        // compileAndStart richiede di girare sul message thread (vedi il suo
        // jassert) mentre setStateInformation puo' essere chiamata da host
        // diversi su thread diversi: per sicurezza la richiamiamo sempre
        // tramite MessageManager::callAsync, anche quando si e' gia' sul
        // thread dei messaggi (callAsync gestisce correttamente anche quel
        // caso).
        if (isEngineRunning())
        {
            juce::MessageManager::callAsync ([this, restoredCsd]
            {
                pendingStateRestore = false;
                compileAndStart (restoredCsd);
            });
        }
    }
}

//==============================================================================
void CsoundAudioProcessor::messageCallback (CSOUND* cs, int /*attr*/, const char* fmt, va_list args)
{
    if (auto* self = static_cast<CsoundAudioProcessor*> (csoundGetHostData (cs)))
    {
        char buffer[2048];
        vsnprintf (buffer, sizeof (buffer), fmt, args);
        self->handleMessage (juce::String (buffer).trimEnd());
    }
}

void CsoundAudioProcessor::handleMessage (const juce::String& msg)
{
    if (msg.isEmpty())
        return;

    // handleMessage gira sul thread AUDIO (chiamata da dentro
    // csoundPerformKsmps): qui deve fare il meno possibile. Tutto il
    // bookkeeping della history (lock + StringArray::add/remove, che con
    // messaggi frequenti puo' costare non poco) e' quindi spostato dentro
    // il lambda, che gira sul thread dei messaggi grazie a callAsync - non
    // sul thread audio.
    juce::MessageManager::callAsync ([this, msg]
    {
        {
            const juce::ScopedLock sl (messageHistoryLock);
            messageHistory.add (msg);

            while (messageHistory.size() > messageHistoryCapacity)
                messageHistory.remove (0);
        }

        listeners.call ([&msg] (Listener& l) { l.csoundMessageReceived (msg); });
    });
}

juce::StringArray CsoundAudioProcessor::getMessageHistory() const
{
    const juce::ScopedLock sl (messageHistoryLock);
    return messageHistory;
}

juce::Array<CsoundLiveOpcodeInfo> CsoundAudioProcessor::getOpcodeSignatures() const
{
    juce::Array<CsoundLiveOpcodeInfo> result;

    CSOUND* cs = activeCsound.load();
    if (cs == nullptr)
        return result;

    // csoundNewOpcodeList/opcodeListEntry/csoundDisposeOpcodeList: presenti
    // anche nella vera Csound 7.0.0 (dichiarate in csound_misc.h, non piu'
    // in csound.h, ma stessi campi opname/outypes/intypes - verificato con
    // grep sull'header del framework reale appena copiato) - stessa fonte
    // usata con Csound 6 per coprire TUTTI gli opcode effettivamente
    // disponibili (vedi CsoundOpcodeHelp.h).
    opcodeListEntry* list = nullptr;
    const int count = csoundNewOpcodeList (cs, &list);

    if (count > 0 && list != nullptr)
    {
        result.ensureStorageAllocated (count);

        for (int i = 0; i < count; ++i)
        {
            CsoundLiveOpcodeInfo info;
            info.name     = list[i].opname  != nullptr ? juce::String (list[i].opname)  : juce::String();
            info.outTypes = list[i].outypes != nullptr ? juce::String (list[i].outypes) : juce::String();
            info.inTypes  = list[i].intypes != nullptr ? juce::String (list[i].intypes) : juce::String();

            if (info.name.isNotEmpty())
                result.add (info);
        }
    }

    if (list != nullptr)
        csoundDisposeOpcodeList (cs, list);

    return result;
}

//==============================================================================
// Bridge MIDI: i 6 callback sotto sono registrati in compileAndStart() dopo
// csoundSetHostMIDIIO(cs). Girano tutti sul thread audio,
// dentro la chiamata a csoundPerformKsmps() in processBlock().
int CsoundAudioProcessor::midiInOpenCallback (CSOUND* cs, void** userData, const char* /*devName*/)
{
    *userData = csoundGetHostData (cs);
    return 0;
}

int CsoundAudioProcessor::midiInReadCallback (CSOUND* /*cs*/, void* userData, unsigned char* buffer, int nBytes)
{
    auto* self = static_cast<CsoundAudioProcessor*> (userData);

    if (self == nullptr)
        return 0;

    int written = 0;

    while (self->incomingMidiQueue.size() > 0)
    {
        const auto& msg = self->incomingMidiQueue.getReference (0);
        const int size = msg.getRawDataSize();

        if (written + size > nBytes)
            break;

        std::memcpy (buffer + written, msg.getRawData(), (size_t) size);
        written += size;

        self->incomingMidiQueue.remove (0);
    }

    return written;
}

int CsoundAudioProcessor::midiInCloseCallback (CSOUND* /*cs*/, void* /*userData*/)
{
    return 0;
}

int CsoundAudioProcessor::midiOutOpenCallback (CSOUND* cs, void** userData, const char* /*devName*/)
{
    *userData = csoundGetHostData (cs);
    return 0;
}

int CsoundAudioProcessor::midiOutWriteCallback (CSOUND* /*cs*/, void* userData, const unsigned char* buffer, int nBytes)
{
    auto* self = static_cast<CsoundAudioProcessor*> (userData);

    if (self == nullptr || self->outgoingMidiBuffer == nullptr || nBytes <= 0)
        return 0;

    // Non abbiamo un modo affidabile di sapere la posizione esatta nel
    // blocco da qui: il messaggio viene accodato al campione 0 del blocco
    // corrente (sufficiente per la maggior parte degli usi - note/CC generati
    // dall'opcode midiout non hanno bisogno di un sample-accuracy particolare).
    self->outgoingMidiBuffer->addEvent (juce::MidiMessage (buffer, nBytes), 0);

    return nBytes;
}

int CsoundAudioProcessor::midiOutCloseCallback (CSOUND* /*cs*/, void* /*userData*/)
{
    return 0;
}

juce::String CsoundAudioProcessor::defaultCsdText()
{
    // nchnls_i dichiara i canali di INGRESSO (bus Input del plugin, es.
    // microfono con la Standalone, o sidechain in una DAW): letti nel
    // .csd con l'opcode "inch". nchnls resta per i canali di USCITA.
    return R"CSD(<CsoundSynthesizer>
<CsOptions>
-m0
</CsOptions>
<CsInstruments>
0dbfs    = 1

; instr 1 is intentionally empty
instr 1
endin

</CsInstruments>
<CsScore>
f 0 z
e
</CsScore>
</CsoundSynthesizer>
)CSD";
}

//==============================================================================
// Punto d'ingresso richiesto dal plugin client di JUCE (Standalone/VST3/AU
// chiamano tutti questa funzione per ottenere l'istanza del processor).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CsoundAudioProcessor();
}
