#include "PluginProcessor.h"
#include "PluginEditor.h"

CsoundAudioProcessor::CsoundAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
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

    // Se il motore e' gia' attivo con parametri diversi resta cosi' finche'
    // l'utente non preme di nuovo Run: qui non ricompiliamo in automatico
    // per evitare glitch a meta' esecuzione. E' un compromesso accettabile
    // per l'MVP.
}

void CsoundAudioProcessor::releaseResources()
{
    stopEngine();
}

bool CsoundAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto set = layouts.getMainOutputChannelSet();
    return set == juce::AudioChannelSet::stereo() || set == juce::AudioChannelSet::mono();
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
        csoundStop (old);
        csoundCleanup (old);
        csoundReset (old);
        csoundDestroy (old);
    }

    setCsdText (newCsdText);

    // csoundInitialize() e' idempotente: e' sicuro chiamarla ogni volta che
    // creiamo una nuova istanza (mirror del pattern usato in csGrain).
    csoundInitialize (CSOUNDINIT_NO_ATEXIT);

    auto* cs = csoundCreate (nullptr);

    if (cs == nullptr)
    {
        handleMessage ("Errore: impossibile creare l'istanza Csound.");
        suspendProcessing (false);
        return;
    }

    csoundSetHostData (cs, this);
    csoundSetMessageCallback (cs, messageCallback);

    // Dice a Csound che l'I/O audio e' gestito dall'host (noi, tramite
    // processBlock/spin/spout), non da un device che Csound apre da solo.
    csoundSetHostImplementedAudioIO (cs, 1, 0);

    csoundSetOption (cs, "-+rtaudio=null");
    csoundSetOption (cs, "-+rtmidi=null");
    csoundSetOption (cs, "-d");            // niente finestre di display/grafici
    csoundSetOption (cs, "-+msg_color=0"); // niente codici colore ANSI nei messaggi (rovinerebbero la console)

    // Forziamo sr e ksmps a coincidere con host sample rate e block size:
    // cosi' non serve resampling e il ciclo in processBlock() e' quasi
    // sempre un singolo passaggio. Il .csd puo' comunque dichiarare valori
    // diversi in <CsInstruments>: qui li sovrascriviamo deliberatamente.
    csoundSetOption (cs, ("--sample-rate=" + juce::String (hostSampleRate, 0)).toRawUTF8());
    csoundSetOption (cs, ("--ksmps=" + juce::String (hostBlockSize)).toRawUTF8());

    const int compileResult = csoundCompileCsdText (cs, newCsdText.toRawUTF8());

    if (compileResult != CSOUND_SUCCESS)
    {
        handleMessage ("Compilazione fallita (codice " + juce::String (compileResult) + "). Controlla la console per i dettagli.");
        csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    if (csoundStart (cs) != CSOUND_SUCCESS)
    {
        handleMessage ("Impossibile avviare il motore Csound.");
        csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    csKsmps            = csoundGetKsmps (cs);
    csNumChannels       = csoundGetNchnls (cs);
    spoutReadPos        = 0;
    samplesLeftInBlock  = 0;

    if (csKsmps <= 0 || csNumChannels <= 0)
    {
        handleMessage ("Configurazione audio non valida (ksmps/nchnls). Controlla <CsInstruments>.");
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
        csoundStop (old);
        csoundCleanup (old);
        csoundReset (old);
        csoundDestroy (old);

        suspendProcessing (false);
        listeners.call ([] (Listener& l) { l.csoundEngineStopped(); });
        return;
    }

    suspendProcessing (false);
}

//==============================================================================
void CsoundAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto* cs = activeCsound.load();
    const int numOut     = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (cs == nullptr || ! ready.load())
    {
        buffer.clear();
        return;
    }

    int written = 0;

    while (written < numSamples)
    {
        if (samplesLeftInBlock == 0)
        {
            // Un "tick" di ksmps campioni per ogni canale nel buffer spout.
            if (csoundPerformKsmps (cs) != 0)
            {
                // Performance/score terminati: silenzio per il resto del
                // buffer e segnaliamo lo stop (l'istanza va comunque
                // distrutta esplicitamente con Stop, qui ci limitiamo a
                // smettere di generare audio per evitare di leggere uno
                // spout non piu' valido).
                buffer.clear (written, numSamples - written);
                ready = false;

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
        const MYFLT* spout = csoundGetSpout (cs);

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
        csoundSetControlChannel (cs, channelName.toRawUTF8(), (MYFLT) value);
}

double CsoundAudioProcessor::getControlChannel (const juce::String& channelName) const
{
    if (auto* cs = activeCsound.load())
    {
        int err = 0;
        const MYFLT value = csoundGetControlChannel (cs, channelName.toRawUTF8(), &err);

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

        if (state.isValid())
            setCsdText (state.getProperty ("csd", getCsdText()).toString());
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

    juce::MessageManager::callAsync ([this, msg]
    {
        listeners.call ([&msg] (Listener& l) { l.csoundMessageReceived (msg); });
    });
}

juce::String CsoundAudioProcessor::defaultCsdText()
{
    return R"CSD(<CsoundSynthesizer>
<CsOptions>
</CsOptions>
<CsInstruments>
sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

instr 1
  aenv linen 0.3, 0.05, p3, 0.1
  asig poscil aenv, 440
  outs asig, asig
endin

</CsInstruments>
<CsScore>
i 1 0 3
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
