#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdlib>
#include <cmath>

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
    // CsoundAPI::csoundCreate() accetta direttamente un path di override come secondo
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
                                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createChannelParamLayout())
{
    csdText = defaultCsdText();
}

CsoundAudioProcessor::~CsoundAudioProcessor()
{
    stopEngine();
}

//==============================================================================
// --- ChannelHostParameter (vedi il commento in PluginProcessor.h) ---------
ChannelHostParameter::ChannelHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                                             const juce::ParameterID& paramID, juce::String defaultName)
    : juce::AudioParameterFloat (paramID, defaultName, juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
      owner (ownerProcessor),
      index (slotIndex),
      fallbackName (std::move (defaultName))
{
}

juce::String ChannelHostParameter::getName (int maximumStringLength) const
{
    const auto slot = owner.getChannelParamSlot (index);
    const auto& displayName = slot.channelName.isNotEmpty() ? slot.channelName : fallbackName;
    return displayName.substring (0, maximumStringLength);
}

juce::String ChannelHostParameter::getText (float normalisedValue, int maximumStringLength) const
{
    const auto slot = owner.getChannelParamSlot (index);
    const double real = CsoundAudioProcessor::denormalizeChannelParam (slot, normalisedValue);
    const juce::String text (real, 3);

    // Convenzione JUCE (vedi AudioParameterFloat::getText): maximumStringLength
    // <= 0 significa "nessun limite", NON "lunghezza zero" - troncare sempre
    // e incondizionatamente con .substring(0, maximumStringLength) produceva
    // una stringa VUOTA ogni volta che il chiamante passa 0, che e' esattamente
    // cio' che juce::SliderParameterAttachment fa per il suo textFromValueFunction
    // (vedi juce_ParameterAttachments.cpp: "param.getText (..., 0)") - la vera
    // causa del valore "invisibile" nello slider della tab UI, niente a che
    // fare con i colori rincorsi finora.
    return maximumStringLength > 0 ? text.substring (0, maximumStringLength) : text;
}

float ChannelHostParameter::getValueForText (const juce::String& text) const
{
    const auto slot = owner.getChannelParamSlot (index);
    return CsoundAudioProcessor::normalizeChannelParam (slot, text.getDoubleValue());
}

float ChannelHostParameter::getDefaultValue() const
{
    const auto slot = owner.getChannelParamSlot (index);
    return CsoundAudioProcessor::normalizeChannelParam (slot, (double) slot.defaultValue);
}

//==============================================================================
// --- IntHostParameter (vedi il commento in PluginProcessor.h) ------------
IntHostParameter::IntHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                                     const juce::ParameterID& paramID, juce::String defaultName)
    : juce::AudioParameterInt (paramID, defaultName, 0, CsoundAudioProcessor::intHostRangeMax, 0),
      owner (ownerProcessor),
      index (slotIndex),
      fallbackName (std::move (defaultName))
{
}

juce::String IntHostParameter::getName (int maximumStringLength) const
{
    const auto slot = owner.getIntParamSlot (index);
    const auto& displayName = slot.channelName.isNotEmpty() ? slot.channelName : fallbackName;
    return displayName.substring (0, maximumStringLength);
}

juce::String IntHostParameter::getText (float normalisedValue, int maximumStringLength) const
{
    const auto slot = owner.getIntParamSlot (index);
    const int real = CsoundAudioProcessor::denormalizeIntParam (slot, normalisedValue);
    const juce::String text (real);

    // Stessa convenzione di ChannelHostParameter::getText sopra: 0 = nessun
    // limite, non lunghezza zero.
    return maximumStringLength > 0 ? text.substring (0, maximumStringLength) : text;
}

float IntHostParameter::getValueForText (const juce::String& text) const
{
    const auto slot = owner.getIntParamSlot (index);
    return CsoundAudioProcessor::normalizeIntParam (slot, text.getDoubleValue());
}

float IntHostParameter::getDefaultValue() const
{
    const auto slot = owner.getIntParamSlot (index);
    return CsoundAudioProcessor::normalizeIntParam (slot, (double) slot.defaultValue);
}

//==============================================================================
// --- BoolHostParameter (vedi il commento in PluginProcessor.h) -----------
BoolHostParameter::BoolHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                                       const juce::ParameterID& paramID, juce::String defaultName)
    : juce::AudioParameterBool (paramID, defaultName, false),
      owner (ownerProcessor),
      index (slotIndex),
      fallbackName (std::move (defaultName))
{
}

juce::String BoolHostParameter::getName (int maximumStringLength) const
{
    const auto slot = owner.getBoolParamSlot (index);
    const auto& displayName = slot.channelName.isNotEmpty() ? slot.channelName : fallbackName;
    return displayName.substring (0, maximumStringLength);
}

float BoolHostParameter::getDefaultValue() const
{
    const auto slot = owner.getBoolParamSlot (index);
    return slot.defaultValue ? 1.0f : 0.0f;
}

//==============================================================================
// --- ChoiceHostParameter (vedi il commento in PluginProcessor.h) ---------
ChoiceHostParameter::ChoiceHostParameter (CsoundAudioProcessor& ownerProcessor, int slotIndex,
                                           const juce::ParameterID& paramID, juce::String defaultName,
                                           const juce::StringArray& placeholderChoices, int defaultChoiceIndex)
    : juce::AudioParameterChoice (paramID, defaultName, placeholderChoices, defaultChoiceIndex),
      owner (ownerProcessor),
      index (slotIndex),
      fallbackName (std::move (defaultName))
{
}

juce::String ChoiceHostParameter::getName (int maximumStringLength) const
{
    const auto slot = owner.getChoiceParamSlot (index);
    const auto& displayName = slot.channelName.isNotEmpty() ? slot.channelName : fallbackName;
    return displayName.substring (0, maximumStringLength);
}

juce::String ChoiceHostParameter::getText (float normalisedValue, int maximumStringLength) const
{
    // Stessa logica con cui AudioParameterChoice::getText ricava l'indice da
    // un valore normalizzato (round (x * (numChoices - 1))), ma l'etichetta
    // mostrata e' quella VERA dello slot corrente (con fallback "Option N"),
    // non la stringa segnalibro passata al costruttore.
    const auto slot = owner.getChoiceParamSlot (index);
    const int numChoices = choices.size();
    const int optionIndex = juce::jlimit (0, numChoices - 1,
                                           juce::roundToInt (normalisedValue * (float) (numChoices - 1)));
    const auto label = CsoundAudioProcessor::getChoiceOptionLabel (slot, optionIndex);

    // Stessa convenzione di ChannelHostParameter::getText sopra: 0 = nessun
    // limite, non lunghezza zero.
    return maximumStringLength > 0 ? label.substring (0, maximumStringLength) : label;
}

float ChoiceHostParameter::getDefaultValue() const
{
    const auto slot = owner.getChoiceParamSlot (index);
    const int numChoices = choices.size();

    if (numChoices <= 1)
        return 0.0f;

    const int clampedIndex = juce::jlimit (0, numChoices - 1, slot.defaultIndex);
    return (float) clampedIndex / (float) (numChoices - 1);
}

// --- 16 "macro" parametri host <-> canali Csound --------------------------
// (vedi il commento su ChannelParamSlot in PluginProcessor.h per il perche'
// di questo disegno: parametro apvts sempre 0..1 fisso, metadata reali solo
// qui).
juce::String CsoundAudioProcessor::getChannelParamID (int index)
{
    // Zero-padded ("chanParam01") solo per leggibilita' in un eventuale
    // dump/debug dello stato salvato - l'host la tratta come stringa
    // opaca, non ne deduce un ordine.
    return "chanParam" + juce::String (index + 1).paddedLeft ('0', 2);
}

juce::AudioProcessorValueTreeState::ParameterLayout CsoundAudioProcessor::createChannelParamLayout()
{
    // Non-static per poter passare *this a ChannelHostParameter (vedi nota
    // nel .h): chiamata dal costruttore, nel member-init-list, prima che
    // channelParamSlots sia costruito - va bene perche' nessuno lo legge
    // ancora a questo punto, solo piu' avanti (getName/getText chiamati
    // dall'host dopo la costruzione completa).

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve ((size_t) numChannelParams);

    for (int i = 0; i < numChannelParams; ++i)
    {
        // ID e range apvts restano FISSI ("chanParamNN", 0..1): e' quello
        // che l'host automatizza/salva. Il nome mostrato e il testo del
        // valore (getName/getText, sopra in ChannelHostParameter) seguono
        // invece lo slot corrente, quindi SI aggiornano quando l'utente
        // rinomina il canale o cambia range/curva nell'editor dei parametri
        // - senza bisogno di ricreare il parametro.
        params.push_back (std::make_unique<ChannelHostParameter> (
            *this, i,
            juce::ParameterID (getChannelParamID (i), 1),
            "Float " + juce::String (i + 1)));
    }

    // --- 16 parametri interi, stesso schema dei float ma senza curva (vedi
    //     IntParamSlot in PluginProcessor.h) ----------------------------------
    for (int i = 0; i < numIntParams; ++i)
    {
        params.push_back (std::make_unique<IntHostParameter> (
            *this, i,
            juce::ParameterID (getIntParamID (i), 1),
            "Int " + juce::String (i + 1)));
    }

    // --- 16 parametri booleani, stesso schema (vedi BoolParamSlot in
    //     PluginProcessor.h) ------------------------------------------------
    for (int i = 0; i < numBoolParams; ++i)
    {
        params.push_back (std::make_unique<BoolHostParameter> (
            *this, i,
            juce::ParameterID (getBoolParamID (i), 1),
            "Bool " + juce::String (i + 1)));
    }

    // --- 16 parametri a scelta multipla, stesso schema (vedi ChoiceParamSlot
    //     in PluginProcessor.h) ---------------------------------------------
    // Scelte segnalibro ("1".."maxChoiceOptions"): MAI mostrate all'host,
    // servono solo a dare ad AudioParameterChoice un numero di opzioni
    // valido e fisso - ChoiceHostParameter::getText sovrascrive sempre con
    // l'etichetta vera dello slot (vedi sopra).
    juce::StringArray placeholderChoices;
    for (int c = 0; c < maxChoiceOptions; ++c)
        placeholderChoices.add (juce::String (c + 1));

    for (int i = 0; i < numChoiceParams; ++i)
    {
        params.push_back (std::make_unique<ChoiceHostParameter> (
            *this, i,
            juce::ParameterID (getChoiceParamID (i), 1),
            "Choice " + juce::String (i + 1),
            placeholderChoices, 0));
    }

    return { params.begin(), params.end() };
}

CsoundAudioProcessor::ChannelParamSlot CsoundAudioProcessor::getChannelParamSlot (int index) const
{
    jassert (index >= 0 && index < numChannelParams);
    const juce::ScopedLock sl (channelParamSlotsLock);
    return channelParamSlots[(size_t) juce::jlimit (0, numChannelParams - 1, index)];
}

void CsoundAudioProcessor::setChannelParamSlot (int index, const ChannelParamSlot& slot)
{
    jassert (index >= 0 && index < numChannelParams);

    if (index < 0 || index >= numChannelParams)
        return;

    {
        const juce::ScopedLock sl (channelParamSlotsLock);
        channelParamSlots[(size_t) index] = slot;
    }

    // ChannelHostParameter::getName/getText leggono lo slot al volo (niente
    // da aggiornare su di loro), ma l'host spesso mette in cache nome/testo
    // mostrati: questo e' il modo standard JUCE per dirgli di rileggerli
    // subito (il supporto effettivo varia da host a host).
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
}

// --- 16 "macro" parametri host interi (vedi il commento su IntParamSlot in
// PluginProcessor.h) ---------------------------------------------------------
juce::String CsoundAudioProcessor::getIntParamID (int index)
{
    return "intParam" + juce::String (index + 1).paddedLeft ('0', 2);
}

CsoundAudioProcessor::IntParamSlot CsoundAudioProcessor::getIntParamSlot (int index) const
{
    jassert (index >= 0 && index < numIntParams);
    const juce::ScopedLock sl (intParamSlotsLock);
    return intParamSlots[(size_t) juce::jlimit (0, numIntParams - 1, index)];
}

void CsoundAudioProcessor::setIntParamSlot (int index, const IntParamSlot& slot)
{
    jassert (index >= 0 && index < numIntParams);

    if (index < 0 || index >= numIntParams)
        return;

    {
        const juce::ScopedLock sl (intParamSlotsLock);
        intParamSlots[(size_t) index] = slot;
    }

    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
}

int CsoundAudioProcessor::denormalizeIntParam (const IntParamSlot& slot, float normalized)
{
    normalized = juce::jlimit (0.0f, 1.0f, normalized);
    const int real = juce::roundToInt (juce::jmap ((double) normalized, 0.0, 1.0,
                                                     (double) slot.minValue, (double) slot.maxValue));
    return juce::jlimit (juce::jmin (slot.minValue, slot.maxValue), juce::jmax (slot.minValue, slot.maxValue), real);
}

float CsoundAudioProcessor::normalizeIntParam (const IntParamSlot& slot, double real)
{
    if (slot.maxValue == slot.minValue)
        return 0.0f;

    return (float) juce::jlimit (0.0, 1.0, juce::jmap (real, (double) slot.minValue, (double) slot.maxValue, 0.0, 1.0));
}

// --- 16 "macro" parametri host booleani (vedi il commento su BoolParamSlot
// in PluginProcessor.h) -----------------------------------------------------
juce::String CsoundAudioProcessor::getBoolParamID (int index)
{
    return "boolParam" + juce::String (index + 1).paddedLeft ('0', 2);
}

CsoundAudioProcessor::BoolParamSlot CsoundAudioProcessor::getBoolParamSlot (int index) const
{
    jassert (index >= 0 && index < numBoolParams);
    const juce::ScopedLock sl (boolParamSlotsLock);
    return boolParamSlots[(size_t) juce::jlimit (0, numBoolParams - 1, index)];
}

void CsoundAudioProcessor::setBoolParamSlot (int index, const BoolParamSlot& slot)
{
    jassert (index >= 0 && index < numBoolParams);

    if (index < 0 || index >= numBoolParams)
        return;

    {
        const juce::ScopedLock sl (boolParamSlotsLock);
        boolParamSlots[(size_t) index] = slot;
    }

    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
}

// --- 16 "macro" parametri host a scelta multipla (vedi il commento su
// ChoiceParamSlot in PluginProcessor.h) --------------------------------------
juce::String CsoundAudioProcessor::getChoiceParamID (int index)
{
    return "choiceParam" + juce::String (index + 1).paddedLeft ('0', 2);
}

juce::String CsoundAudioProcessor::getChoiceOptionLabel (const ChoiceParamSlot& slot, int optionIndex)
{
    if (optionIndex >= 0 && optionIndex < slot.optionLabels.size())
    {
        const auto& label = slot.optionLabels[optionIndex];
        if (label.isNotEmpty())
            return label;
    }

    return "Option " + juce::String (optionIndex + 1);
}

CsoundAudioProcessor::ChoiceParamSlot CsoundAudioProcessor::getChoiceParamSlot (int index) const
{
    jassert (index >= 0 && index < numChoiceParams);
    const juce::ScopedLock sl (choiceParamSlotsLock);
    return choiceParamSlots[(size_t) juce::jlimit (0, numChoiceParams - 1, index)];
}

void CsoundAudioProcessor::setChoiceParamSlot (int index, const ChoiceParamSlot& slot)
{
    jassert (index >= 0 && index < numChoiceParams);

    if (index < 0 || index >= numChoiceParams)
        return;

    {
        const juce::ScopedLock sl (choiceParamSlotsLock);
        choiceParamSlots[(size_t) index] = slot;
    }

    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
}

double CsoundAudioProcessor::denormalizeChannelParam (const ChannelParamSlot& slot, float normalized)
{
    normalized = juce::jlimit (0.0f, 1.0f, normalized);

    // Esponenziale ha senso solo se min e max sono entrambi positivi
    // (es. una frequenza 20-2000 Hz): altrimenti ripieghiamo su lineare,
    // non c'e' un logaritmo di un numero <= 0 che abbia senso qui.
    if (slot.curve == ChannelParamCurve::exponential && slot.minValue > 0.0f && slot.maxValue > 0.0f)
        return (double) slot.minValue * std::pow ((double) (slot.maxValue / slot.minValue), (double) normalized);

    // Logaritmica: reshape di x PRIMA dell'interpolazione lineare (vedi il
    // commento su ChannelParamCurve in PluginProcessor.h) - funziona con
    // QUALSIASI min/max, nessun vincolo di segno.
    if (slot.curve == ChannelParamCurve::logarithmic)
    {
        const double shaped = std::log10 (1.0 + 9.0 * (double) normalized);
        return juce::jmap (shaped, 0.0, 1.0, (double) slot.minValue, (double) slot.maxValue);
    }

    return juce::jmap ((double) normalized, 0.0, 1.0, (double) slot.minValue, (double) slot.maxValue);
}

float CsoundAudioProcessor::normalizeChannelParam (const ChannelParamSlot& slot, double real)
{
    if (slot.curve == ChannelParamCurve::exponential && slot.minValue > 0.0f && slot.maxValue > 0.0f && real > 0.0)
    {
        const double ratio = (double) slot.maxValue / (double) slot.minValue;
        if (ratio > 0.0 && ! juce::approximatelyEqual (ratio, 1.0))
            return (float) juce::jlimit (0.0, 1.0, std::log (real / (double) slot.minValue) / std::log (ratio));
    }

    if (juce::approximatelyEqual ((double) slot.maxValue, (double) slot.minValue))
        return 0.0f;

    if (slot.curve == ChannelParamCurve::logarithmic)
    {
        // Inversa di denormalizeChannelParam: dal valore reale alla forma
        // "shaped" (0..1, interpolazione lineare tra min/max), poi inversa
        // di shaped = log10(1+9x) => x = (10^shaped - 1) / 9.
        const double shaped = juce::jlimit (0.0, 1.0, juce::jmap (real, (double) slot.minValue, (double) slot.maxValue, 0.0, 1.0));
        return (float) juce::jlimit (0.0, 1.0, (std::pow (10.0, shaped) - 1.0) / 9.0);
    }

    return (float) juce::jlimit (0.0, 1.0, juce::jmap (real, (double) slot.minValue, (double) slot.maxValue, 0.0, 1.0));
}

void CsoundAudioProcessor::pushChannelParametersToCsound (CSOUND* cs)
{
    {
        const juce::ScopedLock sl (channelParamSlotsLock);

        for (int i = 0; i < numChannelParams; ++i)
        {
            const auto& slot = channelParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue;

            if (auto* rawValue = apvts.getRawParameterValue (getChannelParamID (i)))
            {
                const double value = denormalizeChannelParam (slot, rawValue->load());
                CsoundAPI::csoundSetControlChannel (cs, slot.channelName.toRawUTF8(), (cs_float) value);
            }
        }
    }

    // Interi: apvts.getRawParameterValue() per un AudioParameterInt torna il
    // valore NATIVO gia' denormalizzato nel range nativo 0..intHostRangeMax
    // (non 0..1) - va prima rinormalizzato a 0..1 (nativeMin e' sempre 0, la
    // divisione basta) e poi smontato nel range reale dello slot, con la
    // stessa formula di IntHostParameter::getText.
    {
        const juce::ScopedLock sl (intParamSlotsLock);

        for (int i = 0; i < numIntParams; ++i)
        {
            const auto& slot = intParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue;

            if (auto* rawValue = apvts.getRawParameterValue (getIntParamID (i)))
            {
                const float normalized = rawValue->load() / (float) intHostRangeMax;
                const int value = denormalizeIntParam (slot, normalized);
                CsoundAPI::csoundSetControlChannel (cs, slot.channelName.toRawUTF8(), (cs_float) value);
            }
        }
    }

    // Booleani: 0.0/1.0 diretto, nessuna denormalizzazione (apvts gia' li
    // tiene come 0.0f/1.0f per un AudioParameterBool).
    {
        const juce::ScopedLock sl (boolParamSlotsLock);

        for (int i = 0; i < numBoolParams; ++i)
        {
            const auto& slot = boolParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue;

            if (auto* rawValue = apvts.getRawParameterValue (getBoolParamID (i)))
                CsoundAPI::csoundSetControlChannel (cs, slot.channelName.toRawUTF8(), (cs_float) rawValue->load());
        }
    }

    // Scelta multipla: il canale riceve l'INDICE selezionato (0..maxChoiceOptions-1).
    // apvts.getRawParameterValue() per un juce::AudioParameterChoice NON e' il
    // valore normalizzato 0..1 (a differenza di Channel/Bool, il cui range
    // nativo e' gia' 0..1): AudioParameterChoice ha un NormalisableRange
    // nativo (0, numChoices-1, 1), quindi getRawParameterValue() restituisce
    // GIA' l'indice intero scelto. Moltiplicarlo di nuovo per
    // (maxChoiceOptions - 1), come faceva la versione precedente, produceva
    // 0,7,14,21,28,35,42,49 invece di 0..7 (indice gia' corretto moltiplicato
    // per 7 una seconda volta). getText() faceva la conversione giusta solo
    // perche' riceve un vero valore normalizzato 0..1 come parametro, non da
    // getRawParameterValue().
    {
        const juce::ScopedLock sl (choiceParamSlotsLock);

        for (int i = 0; i < numChoiceParams; ++i)
        {
            const auto& slot = choiceParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue;

            if (auto* rawValue = apvts.getRawParameterValue (getChoiceParamID (i)))
            {
                const int optionIndex = juce::jlimit (0, maxChoiceOptions - 1, juce::roundToInt (rawValue->load()));
                CsoundAPI::csoundSetControlChannel (cs, slot.channelName.toRawUTF8(), (cs_float) optionIndex);
            }
        }
    }
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

    // CsoundAPI::load() e' idempotente (ritorna true subito se gia'
    // riuscita prima): va chiamata qui, prima di ogni altro uso di
    // CsoundAPI::csoundXxx, perche' e' il punto da cui dlopen() apre
    // CsoundLib64 dal bundle del plugin - vedi CsoundDynamicLib.h/.cpp per
    // il perche' di questo disegno (niente piu' -framework/@rpath/fase di
    // embedding gestita da Xcode). Se fallisce (file non trovato, simbolo
    // mancante...) non c'e' nessun motore da compilare: meglio fermarsi
    // qui con un messaggio chiaro in console che un crash piu' avanti.
    if (! CsoundAPI::isLoaded())
    {
        juce::String errorMessage;

        if (! CsoundAPI::load (&errorMessage))
        {
            handleMessage ("Error: " + errorMessage);
            return;
        }
    }

    // suspendProcessing() dice all'host/wrapper di non chiamare processBlock()
    // mentre sostituiamo il motore: e' la protezione minima per evitare che
    // il thread audio legga un CSOUND* appena distrutto. Per garanzie
    // real-time piu' rigorose (Fase 4+) l'istanza vecchia andrebbe passata
    // a una coda e distrutta dopo qualche ciclo, non subito qui.
    suspendProcessing (true);

    if (auto* old = activeCsound.exchange (nullptr))
    {
        ready = false;
        CsoundAPI::csoundReset (old);
        CsoundAPI::csoundDestroy (old);
    }

    setCsdText (newCsdText);

    // CsoundAPI::csoundInitialize() e' idempotente: e' sicuro chiamarla ogni volta che
    // creiamo una nuova istanza (mirror del pattern usato in csGrain).
    CsoundAPI::csoundInitialize (CSOUNDINIT_NO_ATEXIT);

    // Il secondo argomento di CsoundAPI::csoundCreate() (opcodedir) sovrascrive la
    // cartella di default - assoluta e quindi inutilizzabile in un bundle
    // distribuito - da cui Csound carica gli opcode plugin extra. Vedi
    // getEmbeddedOpcodeDir() piu' sopra.
    const auto opcodeDir = getEmbeddedOpcodeDir();
    auto* cs = CsoundAPI::csoundCreate (nullptr, opcodeDir.isNotEmpty() ? opcodeDir.toRawUTF8() : nullptr);

    if (cs == nullptr)
    {
        handleMessage ("Error: could not create the Csound instance.");
        suspendProcessing (false);
        return;
    }

    CsoundAPI::csoundSetHostData (cs, this);
    CsoundAPI::csoundSetMessageCallback (cs, messageCallback);

    // Dice a Csound che l'I/O audio e' gestito dall'host (noi, tramite
    // processBlock/spin/spout), non da un device che Csound apre da solo.
    // In Csound 7 questa funzione non prende piu' argomenti extra.
    CsoundAPI::csoundSetHostAudioIO (cs);

    // Idem per il MIDI: i 6 callback sotto collegano i messaggi che arrivano/
    // partono da processBlock() (host o wrapper Standalone) al motore Csound,
    // che li legge/scrive con gli opcode midiin/midiout standard.
    CsoundAPI::csoundSetHostMIDIIO (cs);
    CsoundAPI::csoundSetExternalMidiInOpenCallback   (cs, midiInOpenCallback);
    CsoundAPI::csoundSetExternalMidiReadCallback     (cs, midiInReadCallback);
    CsoundAPI::csoundSetExternalMidiInCloseCallback  (cs, midiInCloseCallback);
    CsoundAPI::csoundSetExternalMidiOutOpenCallback  (cs, midiOutOpenCallback);
    CsoundAPI::csoundSetExternalMidiWriteCallback    (cs, midiOutWriteCallback);
    CsoundAPI::csoundSetExternalMidiOutCloseCallback (cs, midiOutCloseCallback);

    incomingMidiQueue.clearQuick();
    outgoingMidiBuffer = nullptr;

    CsoundAPI::csoundSetOption (cs, "-+rtaudio=null");
    CsoundAPI::csoundSetOption (cs, "-+rtmidi=null");

    // -M0/-Q0 sono quello che davvero "attiva" il motore MIDI realtime di
    // Csound: senza, Csound non chiama MAI midiInOpenCallback/
    // midiOutOpenCallback (i callback da soli, con -+rtmidi=null, non
    // bastano - servono solo a dirgli di non usare un device reale, ma e'
    // -M/-Q a dirgli che un ingresso/uscita MIDI esiste e va aperto). Il
    // numero dopo -M/-Q e' ignorato quando l'I/O e' host-implemented: viene
    // comunque instradato ai nostri 6 callback esterni.
    CsoundAPI::csoundSetOption (cs, "-M0");
    CsoundAPI::csoundSetOption (cs, "-Q0");

    CsoundAPI::csoundSetOption (cs, "-d");            // niente finestre di display/grafici
    CsoundAPI::csoundSetOption (cs, "-+msg_color=0"); // niente codici colore ANSI nei messaggi (rovinerebbero la console)

    // Forziamo sr e ksmps a coincidere con host sample rate e block size:
    // cosi' non serve resampling e il ciclo in processBlock() e' quasi
    // sempre un singolo passaggio. Il .csd puo' comunque dichiarare valori
    // diversi in <CsInstruments>: qui li sovrascriviamo deliberatamente.
    CsoundAPI::csoundSetOption (cs, ("--sample-rate=" + juce::String (hostSampleRate, 0)).toRawUTF8());
    //CsoundAPI::csoundSetOption (cs, ("--ksmps=" + juce::String (hostBlockSize)).toRawUTF8());
    CsoundAPI::csoundSetOption (cs, ("--nchnls=" + juce::String (2)).toRawUTF8());
    
    // csoundCompileCsdText non esiste piu' in Csound 7: csoundCompileCSD
    // con mode=1 fa lo stesso lavoro (il secondo argomento e' codice CSD
    // testuale, non un path di file), async=0 perche' qui csoundStart viene
    // chiamata DOPO (vedi la doc di csoundCompileCSD in csound.h: in questo
    // ordine <CsOptions>/<CsScore> vengono preprocessati normalmente).
    const int compileResult = CsoundAPI::csoundCompileCSD (cs, newCsdText.toRawUTF8(), 1, 0);

    if (compileResult != CSOUND_SUCCESS)
    {
        handleMessage ("Compilation failed (code " + juce::String (compileResult) + "). Check the console for details.");
        CsoundAPI::csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    if (CsoundAPI::csoundStart (cs) != CSOUND_SUCCESS)
    {
        handleMessage ("Could not start the Csound engine.");
        CsoundAPI::csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    // csoundGetNchnls/csoundGetNchnlsInput non esistono piu': un'unica
    // CsoundAPI::csoundGetChannels(cs, isInput) le sostituisce entrambe.
    csKsmps            = CsoundAPI::csoundGetKsmps (cs);
    csNumChannels      = (int) CsoundAPI::csoundGetChannels (cs, 0);
    csInputChannels    = (int) CsoundAPI::csoundGetChannels (cs, 1);
    spoutReadPos       = 0;
    samplesLeftInBlock = 0;

    if (csKsmps <= 0 || csNumChannels <= 0)
    {
        handleMessage ("Invalid audio configuration (ksmps/nchnls). Check <CsInstruments>.");
        CsoundAPI::csoundDestroy (cs);
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
        CsoundAPI::csoundReset (old);
        CsoundAPI::csoundDestroy (old);

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

    // Una volta per blocco (non per ogni tick a ksmps: la UI/automazione
    // host non cambia cosi' in fretta da servire piu' di cosi') spinge i
    // 64 "macro" parametri (float/int/bool/choice) verso i canali Csound a
    // cui sono assegnati.
    pushChannelParametersToCsound (cs);

    // Messaggi MIDI in ingresso di questo blocco: midiInReadCallback li
    // consuma durante CsoundAPI::csoundPerformKsmps() qui sotto (stesso thread audio).
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
            cs_float* spin = CsoundAPI::csoundGetSpin (cs);

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
            if (CsoundAPI::csoundPerformKsmps (cs) != 0)
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
        const cs_float* spout = CsoundAPI::csoundGetSpout (cs);

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
        CsoundAPI::csoundSetControlChannel (cs, channelName.toRawUTF8(), (cs_float) value);
}

double CsoundAudioProcessor::getControlChannel (const juce::String& channelName) const
{
    if (auto* cs = activeCsound.load())
    {
        int err = 0;
        const cs_float value = CsoundAPI::csoundGetControlChannel (cs, channelName.toRawUTF8(), &err);

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
juce::ValueTree CsoundAudioProcessor::buildStateTree (bool includeCsdText)
{
    juce::ValueTree state ("CSOUND_STUDIO_STATE");

    // includeCsdText=false per saveSessionToFile: nel file il codice e'
    // GIA' scritto per intero, in chiaro, PRIMA del tag d'appendice (vedi
    // il commento in PluginProcessor.h) - ripeterlo anche qui dentro, come
    // proprieta' XML con ogni ritorno a capo escappato in &#10;, sarebbe
    // solo rumore ridondante e illeggibile, segnalato esplicitamente.
    // getStateInformation invece lo tiene (includeCsdText=true, default):
    // li' e' l'UNICO posto dove il codice vive, non c'e' nessun testo in
    // chiaro altrove nel blob binario che l'host salva.
    if (includeCsdText)
        state.setProperty ("csd", getCsdText(), nullptr);

    // Stato nativo di apvts (i 64 valori normalizzati 0..1): in VST3/AU
    // l'host li salva/ripristina gia' da solo tramite il proprio
    // meccanismo nativo, ma lo salviamo comunque anche qui perche' la
    // Standalone non ha un host che lo faccia per noi - si appoggia solo a
    // questo getStateInformation/setStateInformation.
    state.appendChild (apvts.copyState(), nullptr);

    // Metadata per-slot (nome canale/range/curva) che apvts non conosce -
    // vedi il commento su ChannelParamSlot in PluginProcessor.h sul perche'
    // non vivono nel parametro apvts stesso.
    {
        const juce::ScopedLock sl (channelParamSlotsLock);
        juce::ValueTree slotsTree ("CHANNEL_PARAM_SLOTS");

        for (int i = 0; i < numChannelParams; ++i)
        {
            const auto& slot = channelParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue; // slot non assegnato: niente da salvare

            juce::ValueTree slotTree ("SLOT");
            slotTree.setProperty ("index", i, nullptr);
            slotTree.setProperty ("channel", slot.channelName, nullptr);
            slotTree.setProperty ("min", (double) slot.minValue, nullptr);
            slotTree.setProperty ("max", (double) slot.maxValue, nullptr);
            slotTree.setProperty ("default", (double) slot.defaultValue, nullptr);
            slotTree.setProperty ("curve", (int) slot.curve, nullptr);
            slotsTree.appendChild (slotTree, nullptr);
        }

        state.appendChild (slotsTree, nullptr);
    }

    // Idem per i 16 slot interi.
    {
        const juce::ScopedLock sl (intParamSlotsLock);
        juce::ValueTree slotsTree ("INT_PARAM_SLOTS");

        for (int i = 0; i < numIntParams; ++i)
        {
            const auto& slot = intParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue;

            juce::ValueTree slotTree ("SLOT");
            slotTree.setProperty ("index", i, nullptr);
            slotTree.setProperty ("channel", slot.channelName, nullptr);
            slotTree.setProperty ("min", slot.minValue, nullptr);
            slotTree.setProperty ("max", slot.maxValue, nullptr);
            slotTree.setProperty ("default", slot.defaultValue, nullptr);
            slotsTree.appendChild (slotTree, nullptr);
        }

        state.appendChild (slotsTree, nullptr);
    }

    // Idem per i 16 slot booleani.
    {
        const juce::ScopedLock sl (boolParamSlotsLock);
        juce::ValueTree slotsTree ("BOOL_PARAM_SLOTS");

        for (int i = 0; i < numBoolParams; ++i)
        {
            const auto& slot = boolParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue;

            juce::ValueTree slotTree ("SLOT");
            slotTree.setProperty ("index", i, nullptr);
            slotTree.setProperty ("channel", slot.channelName, nullptr);
            slotTree.setProperty ("default", slot.defaultValue, nullptr);
            slotsTree.appendChild (slotTree, nullptr);
        }

        state.appendChild (slotsTree, nullptr);
    }

    // Idem per i 16 slot a scelta multipla (etichette comprese).
    {
        const juce::ScopedLock sl (choiceParamSlotsLock);
        juce::ValueTree slotsTree ("CHOICE_PARAM_SLOTS");

        for (int i = 0; i < numChoiceParams; ++i)
        {
            const auto& slot = choiceParamSlots[(size_t) i];

            if (slot.channelName.isEmpty())
                continue;

            juce::ValueTree slotTree ("SLOT");
            slotTree.setProperty ("index", i, nullptr);
            slotTree.setProperty ("channel", slot.channelName, nullptr);
            slotTree.setProperty ("defaultIndex", slot.defaultIndex, nullptr);
            slotTree.setProperty ("options", slot.optionLabels.joinIntoString ("\n"), nullptr);
            slotsTree.appendChild (slotTree, nullptr);
        }

        state.appendChild (slotsTree, nullptr);
    }

    return state;
}

void CsoundAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = buildStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void CsoundAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);

        if (state.isValid())
            restoreStateFromTree (state, state.getProperty ("csd", getCsdText()).toString());
    }
}

namespace
{
    // Tag d'appendice che racchiude il mapping dei 64 parametri dentro un
    // .csd altrimenti normale - vedi il commento su saveSessionToFile in
    // PluginProcessor.h sul perche' (Csound stesso ignora tag che non
    // riconosce, quindi il file resta un .csd valido anche senza questo
    // plugin). Scelto un tag "a frase" invece di XML attributes per essere
    // immediato da individuare anche solo guardando il file con un editor
    // di testo qualsiasi.
    const juce::String kParamsTagOpen  = "<CsoundStudioParams>";
    const juce::String kParamsTagClose = "</CsoundStudioParams>";
}

bool CsoundAudioProcessor::saveSessionToFile (const juce::File& file)
{
    // includeCsdText=false: il codice e' GIA' scritto per intero, in
    // chiaro, qui sotto PRIMA del tag - non va ripetuto anche dentro l'XML
    // (sarebbe un'intera copia del codice con ogni ritorno a capo
    // escappato in &#10;, illeggibile e ridondante, segnalato esplicitamente).
    auto xml = buildStateTree (false).createXml();

    if (xml == nullptr)
        return false;

    // Il codice vero e proprio resta testo Csound PURO in testa al file -
    // quello che Csound/un editor di testo si aspettano di trovare. Il
    // mapping dei parametri (nome canale/range/curva/default per slot, piu'
    // una copia dello stato apvts) va in appendice, dopo una riga vuota,
    // dentro il tag dedicato - mai mescolato dentro il codice stesso.
    juce::String fileContents;
    fileContents << getCsdText() << "\n\n"
                 << kParamsTagOpen << "\n"
                 << xml->toString() << "\n"
                 << kParamsTagClose << "\n";

    return file.replaceWithText (fileContents);
}

bool CsoundAudioProcessor::loadSessionFromFile (const juce::File& file)
{
    const auto fullText = file.loadFileAsString();

    if (fullText.isEmpty())
        return false;

    const auto tagStart = fullText.indexOf (kParamsTagOpen);

    if (tagStart < 0)
    {
        // Nessun tag d'appendice: e' un .csd "normale", magari scritto a
        // mano o esportato da un'altra sessione - carichiamo comunque il
        // codice (molto meglio che fallire del tutto), lasciando INVARIATO
        // il mapping dei parametri attuale, visto che qui non ce n'e' uno
        // da ripristinare.
        setCsdText (fullText);
        return true;
    }

    // Il codice Csound e' tutto cio' che precede il tag: resta la fonte di
    // verita' per cosa suona, anche se il tag dopo fosse incoerente o
    // qualcuno avesse modificato il codice a mano senza toccarlo.
    const auto codeText = fullText.substring (0, tagStart).trimEnd();

    const auto innerStart = tagStart + kParamsTagOpen.length();
    const auto tagEnd = fullText.indexOf (innerStart, kParamsTagClose);

    if (tagEnd < 0)
    {
        // Tag apertura presente ma non la chiusura (file troncato/
        // corrotto): carichiamo almeno il codice.
        setCsdText (codeText);
        return true;
    }

    const auto xmlText = fullText.substring (innerStart, tagEnd).trim();

    if (auto xml = juce::XmlDocument::parse (xmlText))
    {
        auto state = juce::ValueTree::fromXml (*xml);

        if (state.isValid())
        {
            restoreStateFromTree (state, codeText);
            return true;
        }
    }

    // XML d'appendice malformato: ancora meglio caricare il codice da solo
    // che fallire l'intero caricamento.
    setCsdText (codeText);
    return true;
}

void CsoundAudioProcessor::restoreStateFromTree (const juce::ValueTree& state, const juce::String& csdTextToRestore)
{
    const auto restoredCsd = csdTextToRestore;
    setCsdText (restoredCsd);

        if (auto apvtsState = state.getChildWithName ("PARAMETERS"); apvtsState.isValid())
            apvts.replaceState (apvtsState);

        {
            const juce::ScopedLock sl (channelParamSlotsLock);

            // Reset preliminare: uno stato salvato con MENO slot assegnati
            // di quelli attualmente in memoria non deve lasciare residui
            // del progetto precedente su uno slot che il nuovo stato non
            // menziona piu'.
            channelParamSlots.fill (ChannelParamSlot{});

            if (auto slotsTree = state.getChildWithName ("CHANNEL_PARAM_SLOTS"); slotsTree.isValid())
            {
                for (auto slotTree : slotsTree)
                {
                    const int index = slotTree.getProperty ("index", -1);

                    if (index >= 0 && index < numChannelParams)
                    {
                        auto& slot = channelParamSlots[(size_t) index];
                        slot.channelName = slotTree.getProperty ("channel", "").toString();
                        slot.minValue    = (float) (double) slotTree.getProperty ("min", 0.0);
                        slot.maxValue    = (float) (double) slotTree.getProperty ("max", 1.0);
                        slot.defaultValue = (float) (double) slotTree.getProperty ("default", 0.0);
                        slot.curve       = (ChannelParamCurve) (int) slotTree.getProperty ("curve", 0);
                    }
                }
            }
        }

        {
            const juce::ScopedLock sl (intParamSlotsLock);
            intParamSlots.fill (IntParamSlot{});

            if (auto slotsTree = state.getChildWithName ("INT_PARAM_SLOTS"); slotsTree.isValid())
            {
                for (auto slotTree : slotsTree)
                {
                    const int index = slotTree.getProperty ("index", -1);

                    if (index >= 0 && index < numIntParams)
                    {
                        auto& slot = intParamSlots[(size_t) index];
                        slot.channelName = slotTree.getProperty ("channel", "").toString();
                        slot.minValue    = slotTree.getProperty ("min", 0);
                        slot.maxValue    = slotTree.getProperty ("max", 127);
                        slot.defaultValue = slotTree.getProperty ("default", 0);
                    }
                }
            }
        }

        {
            const juce::ScopedLock sl (boolParamSlotsLock);
            boolParamSlots.fill (BoolParamSlot{});

            if (auto slotsTree = state.getChildWithName ("BOOL_PARAM_SLOTS"); slotsTree.isValid())
            {
                for (auto slotTree : slotsTree)
                {
                    const int index = slotTree.getProperty ("index", -1);

                    if (index >= 0 && index < numBoolParams)
                    {
                        auto& slot = boolParamSlots[(size_t) index];
                        slot.channelName  = slotTree.getProperty ("channel", "").toString();
                        slot.defaultValue = (bool) slotTree.getProperty ("default", false);
                    }
                }
            }
        }

        {
            const juce::ScopedLock sl (choiceParamSlotsLock);
            choiceParamSlots.fill (ChoiceParamSlot{});

            if (auto slotsTree = state.getChildWithName ("CHOICE_PARAM_SLOTS"); slotsTree.isValid())
            {
                for (auto slotTree : slotsTree)
                {
                    const int index = slotTree.getProperty ("index", -1);

                    if (index >= 0 && index < numChoiceParams)
                    {
                        auto& slot = choiceParamSlots[(size_t) index];
                        slot.channelName  = slotTree.getProperty ("channel", "").toString();
                        slot.defaultIndex = slotTree.getProperty ("defaultIndex", 0);
                        slot.optionLabels = juce::StringArray::fromLines (slotTree.getProperty ("options", "").toString());
                    }
                }
            }
        }

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

//==============================================================================
void CsoundAudioProcessor::messageCallback (CSOUND* cs, int /*attr*/, const char* fmt, va_list args)
{
    if (auto* self = static_cast<CsoundAudioProcessor*> (CsoundAPI::csoundGetHostData (cs)))
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
    const int count = CsoundAPI::csoundNewOpcodeList (cs, &list);

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
        CsoundAPI::csoundDisposeOpcodeList (cs, list);

    return result;
}

//==============================================================================
// Bridge MIDI: i 6 callback sotto sono registrati in compileAndStart() dopo
// CsoundAPI::csoundSetHostMIDIIO(cs). Girano tutti sul thread audio,
// dentro la chiamata a CsoundAPI::csoundPerformKsmps() in processBlock().
int CsoundAudioProcessor::midiInOpenCallback (CSOUND* cs, void** userData, const char* /*devName*/)
{
    *userData = CsoundAPI::csoundGetHostData (cs);
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
    *userData = CsoundAPI::csoundGetHostData (cs);
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

; NOTE: "sr" is overridden by the DAW's sample rate.
; NOTE: "ksmps" is respected as declared and must be a multiple of "sr".
; NOTE: "nchnls" is currently fixed to stereo.

sr     = 44100
ksmps  = 10
nchnls = 2
0dbfs  = 1


instr 1
    ; is intentionally empty
endin

</CsInstruments>
<CsScore>
i1 0 z
;f 0 z
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
