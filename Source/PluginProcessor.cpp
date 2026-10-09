#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <limits>
#include <regex>
#include <map>
#include <set>
#include <functional>

namespace
{
    // Csound e' linkata STATICAMENTE con tutti gli opcode nel core (vedi
    // CsoundDynamicLib.h, build con BUILD_PLUGINS=OFF): nessuna cartella di
    // plugin .dylib da caricare. Passiamo comunque a csoundCreate() una
    // cartella VUOTA come opcodedir, altrimenti Csound userebbe la sua
    // cartella di default (es. /usr/local/lib/csound/plugins64-7.0 o
    // OPCODE7DIR) e, se sulla macchina c'e' un Csound installato, caricherebbe
    // i SUOI plugin dinamici dentro il nostro processo - versione diversa,
    // crash garantito prima o poi. La cartella vuota la creiamo noi.
    juce::String getEmbeddedOpcodeDir()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("apeCsound-no-plugins");
        dir.createDirectory();
        return dir.getFullPathName();
    }
}

CsoundAudioProcessor::CsoundAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                 .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createChannelParamLayout())
{
    // Svuotamento periodico della coda dei messaggi Csound, filtrata (vedi
    // il commento sul filtro della consolle in PluginProcessor.h).
    startTimer (kMessageFlushIntervalMs);

    for (int i = 0; i < kMaxBusChannels; ++i)
        hostOutputIndex[(size_t) i] = hostInputIndex[(size_t) i] = i; // identita' finche' non c'e' un layout

    // Le DUE cronologie di undo (quella interna di CodeDocument e quella
    // condivisa con il pannello Parametri) sono accoppiate uno-a-uno da
    // CodeEditTransactionProxy (vedi PluginEditor.cpp): ogni proxy nella
    // cronologia condivisa corrisponde a UNA transazione del document, e
    // l'undo di un proxy fa undo della transazione "corrente" del document.
    // Il limite di default di juce::UndoManager (30000 unita', dove un
    // inserimento di testo vale text.length() + 32) faceva scartare al
    // document le transazioni piu' vecchie - bastava un Load/Paste di un
    // .csd di 20-30 KB - mentre i proxy corrispondenti restavano nella
    // cronologia condivisa: da li' in poi ogni Undo annullava la transazione
    // SBAGLIATA ("l'undo si incasina e perde la history"). Nessun limite
    // su entrambe: la memoria e' quella del testo digitato, trascurabile.
    codeDocument.getUndoManager().setMaxNumberOfStoredUnits (std::numeric_limits<int>::max(), 0);
    sessionUndoManager.setMaxNumberOfStoredUnits (std::numeric_limits<int>::max(), 0);
    // Template iniziale con i suoi parametri (<apeCsoundParams>): stesso
    // percorso di Load/Initialize. Solo dati (slot + testo), nessuna
    // notifica all'host: i valori ai default li applica l'editor/il
    // ripristino dello stato, qui restano quelli iniziali dell'APVTS.
    if (! loadSessionStructureAndCode (defaultCsdText()))
        csdText = defaultCsdText();
}

CsoundAudioProcessor::~CsoundAudioProcessor()
{
    // Per primo: le callAsync gia' in coda non devono piu' toccare `this`
    // (vedi aliveFlag in PluginProcessor.h).
    aliveFlag->store (false);
    stopTimer(); // filtro della consolle (vedi flushPendingMessages)
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

// --- 64 "macro" parametri host <-> canali Csound --------------------------
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
        // rinomina il canale o cambia range/skew/increment nell'editor dei parametri
        // - senza bisogno di ricreare il parametro.
        params.push_back (std::make_unique<ChannelHostParameter> (
            *this, i,
            juce::ParameterID (getChannelParamID (i), 1),
            "Float " + juce::String (i + 1)));
    }

    // --- 32 parametri interi, stesso schema dei float ma senza skew/increment
    //     (vedi IntParamSlot in PluginProcessor.h) -----------------------------
    for (int i = 0; i < numIntParams; ++i)
    {
        params.push_back (std::make_unique<IntHostParameter> (
            *this, i,
            juce::ParameterID (getIntParamID (i), 1),
            "Int " + juce::String (i + 1)));
    }

    // --- 32 parametri booleani, stesso schema (vedi BoolParamSlot in
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

// --- 32 "macro" parametri host interi (vedi il commento su IntParamSlot in
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

// --- 32 "macro" parametri host booleani (vedi il commento su BoolParamSlot
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

    // Richiesta esplicita: un'opzione che l'utente non ha rinominato deve
    // mostrare "<unassigned>", non piu' un generico "Option N".
    return "<unassigned>";
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

    // Skew: stesso significato di juce::Slider::setSkewFactor (la stessa
    // convenzione usata da Cabbage per il campo skew di range()) - reshape
    // di x (0..1) PRIMA dell'interpolazione lineare tra min e max, con
    // x' = pow(x, 1/skew), NON pow(x, skew) (esponente invertito rispetto
    // a quanto scritto qui in precedenza: skew<1 deve dare PIU'
    // risoluzione vicino a MIN, skew>1 vicino a MAX - verificato contro il
    // comportamento di Cabbage, che segnalava lo skew letto "al contrario"
    // - es. 0.5 in Cabbage si comportava come se fosse 2, cioe' 1/0.5).
    // skew<=0 non ha senso (pow indefinito/instabile), ripieghiamo su 1
    // (lineare) in quel caso.
    const double skew = slot.skew > 0.0f ? (double) slot.skew : 1.0;
    const double shaped = ! juce::approximatelyEqual (skew, 1.0) ? std::pow ((double) normalized, 1.0 / skew)
                                                                   : (double) normalized;
    double real = juce::jmap (shaped, 0.0, 1.0, (double) slot.minValue, (double) slot.maxValue);

    // Increment: quantizza al multiplo piu' vicino a partire da minValue -
    // <= 0 disabilita lo snap (valore continuo, comportamento di prima).
    if (slot.increment > 0.0f)
    {
        const double steps = std::round ((real - (double) slot.minValue) / (double) slot.increment);
        real = (double) slot.minValue + steps * (double) slot.increment;
        real = juce::jlimit ((double) juce::jmin (slot.minValue, slot.maxValue),
                              (double) juce::jmax (slot.minValue, slot.maxValue), real);
    }

    return real;
}

float CsoundAudioProcessor::normalizeChannelParam (const ChannelParamSlot& slot, double real)
{
    if (juce::approximatelyEqual ((double) slot.maxValue, (double) slot.minValue))
        return 0.0f;

    // Inversa di denormalizeChannelParam: dal valore reale alla forma
    // "shaped" (0..1, interpolazione lineare tra min/max), poi inversa di
    // shaped = pow(x, 1/skew) => x = pow(shaped, skew).
    // Lo snap dell'increment non ha un'inversa esatta (e' una perdita di
    // informazione voluta): qui si normalizza il valore reale cosi' com'e'.
    const double shaped = juce::jlimit (0.0, 1.0, juce::jmap (real, (double) slot.minValue, (double) slot.maxValue, 0.0, 1.0));
    const double skew = slot.skew > 0.0f ? (double) slot.skew : 1.0;

    if (! juce::approximatelyEqual (skew, 1.0))
        return (float) juce::jlimit (0.0, 1.0, std::pow (shaped, skew));

    return (float) shaped;
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

                // +1: Cabbage stesso numera le opzioni del combobox a
                // partire da 1 (stessa convenzione gia' verificata per
                // value() nell'import - vedi importCabbageParameters),
                // quindi un .csd scritto per Cabbage ha il codice Csound
                // (if/elseif su chnget di questo canale) che si aspetta
                // 1..N, non 0..N-1. optionIndex resta 0-based ovunque
                // ALTROVE (array optionLabels, ID del combobox nel Generic
                // Editor, defaultIndex) - solo il valore scritto nel
                // canale Csound e' spostato di 1, qui, al limite esatto in
                // cui serve. Prima di questo fix, selezionare la PRIMA
                // opzione (indice 0) mandava un channel value di 0, che
                // nel codice Csound (scritto assumendo 1..N) non
                // combinava con nessun "if kChan == 1 ... N" e cadeva
                // nell'ultimo ramo "else" - il sintomo esatto segnalato
                // (selezionando la prima voce partiva l'ultimo ramo).
                CsoundAPI::csoundSetControlChannel (cs, slot.channelName.toRawUTF8(), (cs_float) (optionIndex + 1));
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
    rebuildHostChannelMaps();

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
    // Cambio del numero di canali della traccia (la DAW richiama
    // prepareToPlay dopo aver cambiato il layout dei bus): Csound va
    // ricompilato con il nuovo nchnls/nchnls_i, vedi compileAndStart.
    const bool channelsChanged = isEngineRunning()
                              && (compiledHostOutputs != juce::jmax (1, getMainBusNumOutputChannels())
                                  || compiledHostInputs != getMainBusNumInputChannels());

    if (pendingStateRestore.exchange (false) || ! isEngineRunning() || channelsChanged)
        compileAndStart (getCsdText());
}

void CsoundAudioProcessor::releaseResources()
{
    stopEngine();
}

bool CsoundAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Da 1 a kMaxBusChannels canali in uscita (qualunque set: mono, stereo,
    // quad, 5.1, discreti...); ingresso disattivato o 1..kMaxBusChannels,
    // anche diverso dall'uscita: Csound ha nchnls e nchnls_i separati e
    // processBlock copia spin/spout canale per canale.
    const auto outSet = layouts.getMainOutputChannelSet();
    const auto inSet  = layouts.getMainInputChannelSet();

    if (outSet.isDisabled() || outSet.size() > kMaxBusChannels)
        return false;

    if (! inSet.isDisabled() && inSet.size() > kMaxBusChannels)
        return false;

    return true;
}

void CsoundAudioProcessor::buildPhysicalOrderMap (const juce::AudioChannelSet& set, std::array<int, kMaxBusChannels>& map)
{
    using CT = juce::AudioChannelSet::ChannelType;

    // Posizione (bit) di ciascun tipo di canale nello speaker arrangement
    // VST3 (Steinberg::Vst::Speaker): e' l'ordine fisico con cui l'host
    // consegna i canali. Stessa convenzione di JUCE (juce_VST3_Common.h):
    // leftSurroundRear/rightSurroundRear usano i bit di Ls/Rs quando il
    // layout non ha leftSurround/rightSurround (e' il caso del 7.1 di JUCE).
    auto vst3Bit = [] (CT t) -> int
    {
        switch (t)
        {
            case CT::left:               return 0;
            case CT::right:              return 1;
            case CT::centre:             return 2;
            case CT::LFE:                return 3;
            case CT::leftSurround:       return 4;
            case CT::rightSurround:      return 5;
            case CT::leftCentre:         return 6;
            case CT::rightCentre:        return 7;
            case CT::centreSurround:     return 8;
            case CT::leftSurroundSide:   return 9;
            case CT::rightSurroundSide:  return 10;
            case CT::topMiddle:          return 11;
            case CT::topFrontLeft:       return 12;
            case CT::topFrontCentre:     return 13;
            case CT::topFrontRight:      return 14;
            case CT::topRearLeft:        return 15;
            case CT::topRearCentre:      return 16;
            case CT::topRearRight:       return 17;
            case CT::LFE2:               return 18;
            case CT::leftSurroundRear:   return 4;
            case CT::rightSurroundRear:  return 5;
            case CT::wideLeft:           return 19;
            case CT::wideRight:          return 20;
            default:                     return -1; // discreti, ambisonics, altro: identita'
        }
    };

    const int n = juce::jmin (kMaxBusChannels, set.size());

    for (int i = 0; i < kMaxBusChannels; ++i)
        map[(size_t) i] = i;

    // (bit, indice JUCE) ordinati per bit = ordine fisico.
    std::vector<std::pair<int, int>> order;

    for (int i = 0; i < n; ++i)
    {
        const int bit = vst3Bit (set.getTypeOfChannel (i));

        if (bit < 0)
            return; // layout non "con nome": ordine JUCE = ordine fisico

        order.emplace_back (bit, i);
    }

    std::sort (order.begin(), order.end());

    for (int physical = 0; physical < (int) order.size(); ++physical)
        map[(size_t) physical] = order[(size_t) physical].second;
}

void CsoundAudioProcessor::rebuildHostChannelMaps()
{
    buildPhysicalOrderMap (getChannelLayoutOfBus (false, 0), hostOutputIndex);
    buildPhysicalOrderMap (getChannelLayoutOfBus (true, 0),  hostInputIndex);
}

void CsoundAudioProcessor::setFollowCsdChannels (bool shouldFollow)
{
    if (followCsdChannels.exchange (shouldFollow) == shouldFollow)
        return;

    // Cambia il modo in cui si calcola nchnls: ricompila subito.
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        compileAndStart (getCsdText());
}

int CsoundAudioProcessor::parseDeclaredHeaderValue (const juce::String& csdText, const juce::String& name)
{
    // "nchnls = 4" / "nchnls=4" a inizio riga (spazi a parte), fuori dai
    // commenti; prima occorrenza. Niente regex: una scansione riga per riga.
    juce::StringArray lines;
    lines.addLines (csdText);

    for (auto line : lines)
    {
        const int comment = line.indexOfChar (';');
        if (comment >= 0)
            line = line.substring (0, comment);

        line = line.trim();

        if (! line.startsWith (name))
            continue;

        auto rest = line.substring (name.length()).trimStart();

        if (! rest.startsWith ("="))
            continue;

        rest = rest.substring (1).trim();
        const int value = rest.getIntValue();

        if (value > 0 && rest.isNotEmpty() && juce::CharacterFunctions::isDigit (rest[0]))
            return value;
    }

    return 0;
}

//==============================================================================
void CsoundAudioProcessor::compileAndStart (const juce::String& newCsdText)
{
    jassert (juce::MessageManager::getInstance()->isThisTheMessageThread());

    // CsoundAPI::load() aggancia i puntatori CsoundAPI::csoundXxx ai simboli
    // di Csound linkati STATICAMENTE (vedi CsoundDynamicLib.h): idempotente,
    // non puo' fallire, ma va chiamata prima di ogni altro uso. Il
    // controllo dell'esito resta per sicurezza: senza motore meglio fermarsi
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
    // Canali: vedi setFollowCsdChannels in PluginProcessor.h.
    const int hostOut = juce::jmax (1, getMainBusNumOutputChannels());
    const int hostIn  = getMainBusNumInputChannels();
    const int csdOut  = parseDeclaredHeaderValue (newCsdText, "nchnls");
    const int csdIn   = parseDeclaredHeaderValue (newCsdText, "nchnls_i");
    compiledHostOutputs = hostOut;
    compiledHostInputs  = hostIn;
    rebuildHostChannelMaps();

    // Valori dalla traccia (usati quando non si segue il .csd o quando il
    // .csd non li dichiara). Min 2 in uscita: "outs" con nchnls = 1 e' un
    // errore di init in Csound, e quasi tutti i .csd lo usano; su una
    // traccia mono processBlock prende il solo canale sinistro.
    const int trackOut = juce::jmax (2, hostOut);
    const int trackIn  = hostIn;
    const bool follow  = followCsdChannels.load();

    const int csOut = follow && csdOut > 0 ? csdOut : trackOut;
    const int csIn  = follow && csdIn  > 0 ? csdIn  : trackIn;

    CsoundAPI::csoundSetOption (cs, ("--nchnls=" + juce::String (csOut)).toRawUTF8());

    if (csIn > 0)
        CsoundAPI::csoundSetOption (cs, ("--nchnls_i=" + juce::String (csIn)).toRawUTF8());

    if (follow)
    {
        if (csdOut > 0 && csdOut != hostOut)
            handleMessage ("Channels: nchnls = " + juce::String (csdOut) + " from the CSD, the track has "
                           + juce::String (hostOut) + " output channel(s): extra channels on either side are silent"
                           " (disable Config > \"Follow CSD nchnls\" to let Csound follow the track).");

        if (csdOut == 0)
            handleMessage ("Channels: the CSD does not declare nchnls, using the track: nchnls = " + juce::String (csOut) + ".");

        if (csdIn > 0 && csdIn != hostIn)
            handleMessage ("Channels: nchnls_i = " + juce::String (csdIn) + " from the CSD, the track has "
                           + juce::String (hostIn) + " input channel(s).");
    }
    else
    {
        if (csdOut > 0 && csdOut != csOut)
            handleMessage ("Channels: the CSD declares nchnls = " + juce::String (csdOut)
                           + ", the track has " + juce::String (hostOut) + " output channel(s): Csound runs with nchnls = "
                           + juce::String (csOut) + " (enable Config > \"Follow CSD nchnls\" to keep the CSD value).");

        if (csdIn > 0 && hostIn > 0 && csdIn != hostIn)
            handleMessage ("Channels: the CSD declares nchnls_i = " + juce::String (csdIn)
                           + ", the track has " + juce::String (hostIn) + " input channel(s): Csound runs with nchnls_i = "
                           + juce::String (hostIn) + ".");
    }
    
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
    ksmpsPos           = 0;

    if (csKsmps <= 0 || csNumChannels <= 0)
    {
        handleMessage ("Invalid audio configuration (ksmps/nchnls). Check <CsInstruments>.");
        CsoundAPI::csoundDestroy (cs);
        suspendProcessing (false);
        return;
    }

    // spout appena allocato da Csound e' a zero (in Csound 7 e' const per
    // l'host): il primo tick restituisce silenzio mentre spin si riempie,
    // quindi l'output e' in ritardo di esattamente csKsmps campioni.
    setLatencySamples (csKsmps);

    // A) latenza introdotta dal plugin, sempre in console a ogni Apply.
    // B) avviso se ksmps e' alto: la latenza pesa sul monitoraggio live.
    {
        const double sr = hostSampleRate > 0.0 ? hostSampleRate : 44100.0;
        const double latencyMs = 1000.0 * (double) csKsmps / sr;
        const auto msText = juce::String (latencyMs, 2) + " ms @ "
                          + juce::String (sr / 1000.0, 1) + " kHz";

        handleMessage ("Latency: " + juce::String (csKsmps) + " samples ("
                       + msText + ", compensated by the host)");
        handleMessage ("Channels: nchnls = " + juce::String (csNumChannels) + ", nchnls_i = " + juce::String (csInputChannels)
                       + " (track: " + juce::String (compiledHostOutputs) + " out, " + juce::String (compiledHostInputs) + " in, layout "
                       + getChannelLayoutOfBus (false, 0).getDescription() + ")");

        // Layout surround: mostra la corrispondenza canale fisico -> canale
        // JUCE se non e' l'identita' (per capire dove finisce "outch n").
        juce::String remap;
        for (int i = 0; i < juce::jmin (kMaxBusChannels, compiledHostOutputs); ++i)
            if (hostOutputIndex[(size_t) i] != i)
                remap << (remap.isEmpty() ? "" : ", ") << "outch " << (i + 1) << " -> buffer " << (hostOutputIndex[(size_t) i] + 1);

        if (remap.isNotEmpty())
            handleMessage ("Channels: surround layout reordered by the host wrapper, compensated: " + remap);

        if (csKsmps > kHighKsmpsWarningThreshold)
            handleMessage ("Warning: ksmps " + juce::String (csKsmps) + " adds "
                           + msText + " latency on live input."
                           " Use a lower ksmps (16-64) for live monitoring.");
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
        // Quanti campioni mancano per completare il tick corrente.
        const int frames = juce::jmin (csKsmps - ksmpsPos, numSamples - written);

        cs_float*       spin  = CsoundAPI::csoundGetSpin  (cs);
        const cs_float* spout = CsoundAPI::csoundGetSpout (cs);

        // 1) Input host -> spin, alla stessa posizione del tick. Va fatto
        //    PRIMA di scrivere l'output: in JUCE il buffer e' in-place, i
        //    canali di uscita sovrascrivono quelli di ingresso.
        for (int ch = 0; ch < csInputChannels; ++ch)
        {
            // Canali Csound oltre quelli dell'host: silenzio (non la copia
            // dell'ultimo canale host). hostInputIndex: ordine fisico della
            // traccia -> indice del buffer JUCE (vedi rebuildHostChannelMaps).
            const int hostCh = ch < numIn && ch < kMaxBusChannels ? hostInputIndex[(size_t) ch] : -1;
            const float* src = hostCh >= 0 && hostCh < numIn ? buffer.getReadPointer (hostCh, written) : nullptr;

            for (int i = 0; i < frames; ++i)
                spin[(size_t) (ksmpsPos + i) * (size_t) csInputChannels + (size_t) ch] = src != nullptr ? (cs_float) src[i] : (cs_float) 0;
        }

        // 2) spout (calcolato al tick precedente) -> output host.
        for (int ch = 0; ch < numOut; ++ch)
        {
            // ch = canale FISICO della traccia (1 = "outch 1"); hostCh =
            // dove sta nel buffer JUCE (vedi rebuildHostChannelMaps).
            const int hostCh = ch < kMaxBusChannels ? hostOutputIndex[(size_t) ch] : ch;

            if (hostCh < 0 || hostCh >= numOut)
                continue;

            auto* dest = buffer.getWritePointer (hostCh, written);

            // Canali host oltre quelli di Csound: silenzio.
            if (ch >= csNumChannels)
            {
                juce::FloatVectorOperations::clear (dest, frames);
                continue;
            }

            for (int i = 0; i < frames; ++i)
                dest[i] = (float) spout[(size_t) (ksmpsPos + i) * (size_t) csNumChannels + (size_t) ch];
        }

        ksmpsPos += frames;
        written  += frames;

        // 3) spin pieno: un tick di Csound. Consuma spin e produce il nuovo
        //    spout, che verra' letto nei prossimi csKsmps campioni (latenza
        //    = csKsmps, nessun campione perso qualunque sia il block size
        //    dell'host). midiInReadCallback/midiOutWriteCallback vengono
        //    chiamati da Csound qui dentro.
        if (ksmpsPos >= csKsmps)
        {
            ksmpsPos = 0;

            if (CsoundAPI::csoundPerformKsmps (cs) != 0)
            {
                // Performance/score terminati: silenzio per il resto del
                // buffer e segnaliamo lo stop.
                buffer.clear (written, numSamples - written);
                ready = false;

                outgoingMidiBuffer = nullptr;
                midiMessages.swapWith (outgoing);

                juce::MessageManager::callAsync ([this, alive = aliveFlag]
                {
                    if (! alive->load())
                        return;

                    listeners.call ([] (Listener& l) { l.csoundEngineStopped(); });
                });
                return;
            }
        }
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

void CsoundAudioProcessor::setCsdCodeFromFullText (const juce::String& fullCode)
{
    static const juce::String kSynthOpen  = "<CsoundSynthesizer>";
    static const juce::String kSynthClose = "</CsoundSynthesizer>";

    const auto openAt = fullCode.indexOf (kSynthOpen);
    const auto closeAt = openAt >= 0 ? fullCode.indexOf (openAt, kSynthClose) : -1;

    const juce::ScopedLock sl (csdTextLock);

    if (openAt < 0 || closeAt < 0)
    {
        // Nessun blocco riconoscibile (o troncato): niente da filtrare,
        // tutto il testo e' codice - come prima di questa modifica.
        csdPreamble.clear();
        csdPostamble.clear();
        csdText = fullCode;
        editorDraft.clear();     // codice appena caricato: nessuna bozza
        hasEditorDraft = false;  // (vedi setEditorDraft)
        return;
    }

    const auto blockEnd = closeAt + kSynthClose.length();
    csdPreamble  = fullCode.substring (0, openAt);
    csdText      = fullCode.substring (openAt, blockEnd);
    csdPostamble = fullCode.substring (blockEnd);
    editorDraft.clear();
    hasEditorDraft = false;
}

void CsoundAudioProcessor::setEditorDraft (const juce::String& code)
{
    const juce::ScopedLock sl (csdTextLock);
    editorDraft = code;
    hasEditorDraft = true;
}

juce::String CsoundAudioProcessor::getEditorDraft() const
{
    const juce::ScopedLock sl (csdTextLock);
    return hasEditorDraft ? editorDraft : csdText;
}

//==============================================================================
juce::ValueTree CsoundAudioProcessor::buildParamsStructureTree()
{
    // SOLO la struttura (metadata per-slot): niente testo del codice (nel
    // .csd e' scritto in chiaro prima del tag <CsoundParams>) e niente stato
    // apvts (i VALORI vivono nello stato del progetto, per nome canale -
    // vedi getStateInformation). Il nome radice resta quello storico
    // APE_CSOUND_STATE cosi' i .csd salvati dalle versioni precedenti
    // (che dentro avevano anche un figlio PARAMETERS) restano leggibili da
    // restoreStateFromTree, e viceversa.
    juce::ValueTree state ("APE_CSOUND_STATE");

    // Metadata per-slot (nome canale/range/skew/increment) che apvts non
    // conosce - vedi il commento su ChannelParamSlot in PluginProcessor.h
    // sul perche' non vivono nel parametro apvts stesso.
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
            slotTree.setProperty ("skew", (double) slot.skew, nullptr);
            slotTree.setProperty ("step", (double) slot.increment, nullptr);
            slotsTree.appendChild (slotTree, nullptr);
        }

        state.appendChild (slotsTree, nullptr);
    }

    // Idem per i 32 slot interi.
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

    // Idem per i 32 slot booleani.
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

namespace
{
    // Tag d'appendice che racchiude il mapping dei 64 parametri dentro un
    // .csd altrimenti normale - vedi il commento su saveSessionToFile in
    // PluginProcessor.h sul perche' (Csound stesso ignora tag che non
    // riconosce, quindi il file resta un .csd valido anche senza questo
    // plugin). Scelto un tag "a frase" invece di XML attributes per essere
    // immediato da individuare anche solo guardando il file con un editor
    // di testo qualsiasi.
    const juce::String kParamsTagOpen  = "<apeCsoundParams>";
    const juce::String kParamsTagClose = "</apeCsoundParams>";

    // Nomi degli elementi/attributi dello stato del progetto (NUOVO formato,
    // vedi il commento su getStateInformation in PluginProcessor.h).
    const char* const kStateRoot       = "PluginState";
    const char* const kStateEmbedded   = "EmbeddedCsd";
    const char* const kStateParam      = "P";
    const char* const kStateDraft      = "EditorDraft";
}

//==============================================================================
// Sessione collegata a un file / stato del progetto - vedi il commento esteso
// su getStateInformation in PluginProcessor.h.
juce::File CsoundAudioProcessor::getBaseFolder()
{
   #if JUCE_IOS
    // iOS: l'estensione AUv3 (dentro la DAW) e l'app Standalone sono due
    // sandbox diverse; i .csd devono stare nel contenitore condiviso
    // dell'App Group (abilitato nel .jucer con lo stesso identificatore),
    // altrimenti un file salvato dall'app non si vede dal plugin e
    // viceversa. Se il contenitore non e' disponibile (entitlement
    // mancante) si ripiega sui Documents della singola sandbox.
    auto container = juce::File::getContainerForSecurityApplicationGroupIdentifier (kIOSAppGroupId);
    auto folder = (container != juce::File{} ? container.getChildFile ("Documents")
                                             : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory))
                      .getChildFile ("apeCsound");
   #else
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("apeCsound");
   #endif

    // NON la crea qui (punto 10): la crea il file chooser dell'editor quando
    // serve (vedi getCsdChooserStartDirectory in PluginEditor.cpp).
    return folder;
}

juce::File CsoundAudioProcessor::getLinkedCsdFile() const
{
    const juce::ScopedLock sl (sessionLock);
    return linkedCsdFile;
}

void CsoundAudioProcessor::setLinkedCsdFile (const juce::File& file)
{
    const juce::ScopedLock sl (sessionLock);
    linkedCsdFile = file;
}

juce::String CsoundAudioProcessor::getLinkedCsdDisplayPath() const
{
    const auto file = getLinkedCsdFile();

    if (file == juce::File{})
        return {};

    const auto base = getBaseFolder();
    return file.isAChildOf (base) ? file.getRelativePathFrom (base) : file.getFullPathName();
}

juce::String CsoundAudioProcessor::readSessionFile (const juce::File& file)
{
    // Lettura tramite juce::URL + bookmark (vedi SecurityScopedFile::
    // makeURLWithBookmark): su iOS JUCE apre/chiude l'accesso security-
    // scoped da solo. Senza bookmark e' una normale lettura del file.
    juce::String text;
    {
        auto url = makeFileURL (file);

        if (auto in = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                 .withConnectionTimeoutMs (2000)))
        {
            text = in->readEntireStreamAsString();

            if (text.isEmpty())
                handleMessage ("Load: " + url.toString (false) + " read as empty (not downloaded from iCloud yet?)");
        }
        else
        {
            handleMessage ("Load: could not open an input stream for " + url.toString (false) + ", trying direct file access");
            const auto access = accessFile (file);
            text = access->resolvedFile.loadFileAsString();

            if (text.isEmpty())
                handleMessage ("Load: direct read of " + access->resolvedFile.getFullPathName() + " is empty"
                               + (access->active ? "" : " (security-scoped access not granted)"));
        }
    }

    return text.replace ("\r\n", "\n").replace ("\r", "\n");
}

juce::URL CsoundAudioProcessor::makeFileURL (const juce::File& file) const
{
    juce::MemoryBlock bookmark;
    {
        const juce::ScopedLock sl (sessionLock);

        if (file != juce::File{} && file == bookmarkedFile)
        {
            // Stessa sessione del picker: la SUA URL, intatta (schema NP2).
            if (! chooserUrl.isEmpty())
                return chooserUrl;

            bookmark = fileBookmark;
        }
    }

    return SecurityScopedFile::makeURLWithBookmark (file, bookmark);
}

void CsoundAudioProcessor::setFileBookmark (const juce::File& file, const juce::MemoryBlock& bookmark)
{
    const juce::ScopedLock sl (sessionLock);

    if (bookmarkedFile != file)
        chooserUrl = {}; // la URL del picker vale solo per il suo file

    bookmarkedFile = file;
    fileBookmark   = bookmark;
}

void CsoundAudioProcessor::setFileURL (const juce::File& file, const juce::URL& url)
{
    auto copy = url;
    const auto bookmark = SecurityScopedFile::bookmarkFromChooserURL (copy);

    const juce::ScopedLock sl (sessionLock);
    bookmarkedFile = file;
    fileBookmark   = bookmark;
    chooserUrl     = url;
}

std::unique_ptr<SecurityScopedFile::ScopedAccess> CsoundAudioProcessor::accessFile (const juce::File& file) const
{
    juce::MemoryBlock bookmark;
    {
        const juce::ScopedLock sl (sessionLock);

        if (file != juce::File{} && file == bookmarkedFile)
            bookmark = fileBookmark;
    }

    auto access = std::make_unique<SecurityScopedFile::ScopedAccess> (bookmark, file);

    // Bookmark da rinnovare (file spostato/rinominato dall'utente): il
    // path risolto e' quello buono da ora in poi.
    if (access->active && access->stale)
    {
        auto fresh = SecurityScopedFile::makeBookmark (access->resolvedFile);

        if (fresh.getSize() > 0)
        {
            const juce::ScopedLock sl (sessionLock);
            fileBookmark = fresh;
        }
    }

    return access;
}

juce::String CsoundAudioProcessor::computeTextHash (const juce::String& text)
{
    // UTF-8 del testo cosi' com'e': stesso risultato di un `shasum -a 256`
    // sul file, se il file e' stato scritto da saveSessionToFile.
    return juce::SHA256 (text.toRawUTF8(), text.getNumBytesAsUTF8()).toHexString();
}

juce::String CsoundAudioProcessor::buildSessionText()
{
    return buildSessionTextFor (getCsdText());
}

juce::String CsoundAudioProcessor::buildSessionTextFor (const juce::String& rawCodeText)
{
    // Il codice NON deve contenere una sezione <apeCsoundParams> (BUG
    // corretto): incollando un .csd completo nell'editor quella del file
    // incollato restava nel codice, il file salvato ne conteneva DUE e al
    // Load successivo vinceva la prima (quella vecchia), perdendo la
    // mappatura reale. La struttura vera e' solo quella in coda, scritta qui.
    juce::String codeText = rawCodeText;
    for (int open = codeText.indexOf (kParamsTagOpen); open >= 0; open = codeText.indexOf (kParamsTagOpen))
    {
        const int close = codeText.indexOf (open, kParamsTagClose);
        const int end = close >= 0 ? close + kParamsTagClose.length() : codeText.length();
        codeText = codeText.substring (0, open).trimEnd() + codeText.substring (end);
    }

    auto xml = buildParamsStructureTree().createXml();

    // Il codice vero e proprio resta testo Csound PURO in testa - quello
    // che Csound/un editor di testo si aspettano di trovare. La STRUTTURA
    // dei parametri (nome canale/range/skew/increment/default/opzioni per
    // slot) va in appendice, dopo una riga vuota, dentro il tag dedicato -
    // mai mescolata dentro il codice stesso.
    // Preambolo/coda (es. <Cabbage>) rimessi al loro posto attorno al
    // blocco <CsoundSynthesizer> - vedi setCsdCodeFromFullText().
    juce::String preamble, postamble;
    {
        const juce::ScopedLock sl (csdTextLock);
        preamble  = csdPreamble;
        postamble = csdPostamble;
    }

    // trimEnd(): salvataggio e ricaricamento devono produrre lo STESSO testo
    // (il loader taglia gli spazi prima di <apeCsoundParams>) - altrimenti
    // l'hash di un file appena salvato non coincideva con quello dopo il
    // ricaricamento, con un falso "changed on disk" alla riapertura.
    juce::String text;
    text << (preamble + codeText + postamble).trimEnd() << "\n\n"
         << kParamsTagOpen << "\n"
         << (xml != nullptr ? xml->toString() : juce::String()) << "\n"
         << kParamsTagClose << "\n";
    return text;
}

juce::String CsoundAudioProcessor::getSessionBaselineHash() const
{
    const juce::ScopedLock sl (sessionLock);
    return sessionBaselineHash;
}

void CsoundAudioProcessor::updateSessionBaselineHash()
{
    const auto hash = computeTextHash (buildSessionText());
    const juce::ScopedLock sl (sessionLock);
    sessionBaselineHash = hash;
}

bool CsoundAudioProcessor::isLinkedFileMissing() const
{
    const juce::ScopedLock sl (sessionLock);
    return linkedFileMissing;
}

bool CsoundAudioProcessor::wasRestoredDirty() const
{
    const juce::ScopedLock sl (sessionLock);
    return restoredDirty;
}

juce::String CsoundAudioProcessor::getRestoredSessionHash() const
{
    const juce::ScopedLock sl (sessionLock);
    return restoredSessionHash;
}

void CsoundAudioProcessor::notifySessionEditedByUndoRedo (bool structureReplaced)
{
    jassert (juce::MessageManager::existsAndIsCurrentThread());
    listeners.call ([structureReplaced] (Listener& l) { l.sessionEditedByUndoRedo (structureReplaced); });
}

bool CsoundAudioProcessor::isSessionDirty()
{
    if (! isSessionLinked() || isLinkedFileMissing())
        return true;

    return computeTextHash (buildSessionTextFor (getEditorDraft())) != getSessionBaselineHash();
}

void CsoundAudioProcessor::unlinkSession()
{
    const juce::ScopedLock sl (sessionLock);
    linkedCsdFile = juce::File();
    linkedFileModTimeAtLoad = juce::Time();
    linkedFileMissing = false;
    restoredDirty = false;
}

CsoundAudioProcessor::SessionSnapshot CsoundAudioProcessor::captureSessionSnapshot()
{
    SessionSnapshot snap;
    snap.structure = buildParamsStructureTree();

    auto captureValues = [this, &snap] (int count, juce::String (*idFn) (int))
    {
        for (int i = 0; i < count; ++i)
            if (auto* param = apvts.getParameter (idFn (i)))
                snap.values.push_back (param->getValue());
            else
                snap.values.push_back (-1.0f);
    };
    captureValues (numChannelParams, &CsoundAudioProcessor::getChannelParamID);
    captureValues (numIntParams,     &CsoundAudioProcessor::getIntParamID);
    captureValues (numBoolParams,    &CsoundAudioProcessor::getBoolParamID);
    captureValues (numChoiceParams,  &CsoundAudioProcessor::getChoiceParamID);

    {
        const juce::ScopedLock sl (csdTextLock);
        snap.preamble  = csdPreamble;
        snap.postamble = csdPostamble;
    }

    const juce::ScopedLock sl (sessionLock);
    snap.linkedFile    = linkedCsdFile;
    snap.linkedModTime = linkedFileModTimeAtLoad;
    snap.baselineHash  = sessionBaselineHash;
    snap.fileMissing   = linkedFileMissing;
    snap.restoredDirty = restoredDirty;
    return snap;
}

void CsoundAudioProcessor::restoreSessionSnapshot (const SessionSnapshot& snap)
{
    // Solo struttura + preambolo/coda + collegamento: il codice in
    // esecuzione e la bozza NON si toccano qui (la bozza segue da sola il
    // document dell'editor, che il chiamante riporta indietro/avanti).
    applyParamsStructureTree (snap.structure);

    // Valori, nello stesso ordine di captureSessionSnapshot.
    {
        size_t k = 0;
        auto restoreValues = [this, &snap, &k] (int count, juce::String (*idFn) (int))
        {
            for (int i = 0; i < count; ++i, ++k)
                if (k < snap.values.size() && snap.values[k] >= 0.0f)
                    if (auto* param = apvts.getParameter (idFn (i)))
                        param->setValueNotifyingHost (snap.values[k]);
        };
        restoreValues (numChannelParams, &CsoundAudioProcessor::getChannelParamID);
        restoreValues (numIntParams,     &CsoundAudioProcessor::getIntParamID);
        restoreValues (numBoolParams,    &CsoundAudioProcessor::getBoolParamID);
        restoreValues (numChoiceParams,  &CsoundAudioProcessor::getChoiceParamID);
    }

    {
        const juce::ScopedLock sl (csdTextLock);
        csdPreamble  = snap.preamble;
        csdPostamble = snap.postamble;
    }

    const juce::ScopedLock sl (sessionLock);
    linkedCsdFile           = snap.linkedFile;
    linkedFileModTimeAtLoad = snap.linkedModTime;
    sessionBaselineHash     = snap.baselineHash;
    linkedFileMissing       = snap.fileMissing;
    restoredDirty           = snap.restoredDirty;
}

void CsoundAudioProcessor::resetParameterValuesToDefaults()
{
    // Stessa conversione a normalizzato 0..1 usata ovunque per
    // setValueNotifyingHost (vedi normalizeChannelParam/normalizeIntParam).
    auto push = [] (juce::RangedAudioParameter* param, float normalized)
    {
        if (param != nullptr)
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
    };

    for (int i = 0; i < numChannelParams; ++i)
    {
        const auto slot = getChannelParamSlot (i);
        if (slot.channelName.isNotEmpty())
            push (apvts.getParameter (getChannelParamID (i)), normalizeChannelParam (slot, (double) slot.defaultValue));
    }

    for (int i = 0; i < numIntParams; ++i)
    {
        const auto slot = getIntParamSlot (i);
        if (slot.channelName.isNotEmpty())
            push (apvts.getParameter (getIntParamID (i)), normalizeIntParam (slot, (double) slot.defaultValue));
    }

    for (int i = 0; i < numBoolParams; ++i)
    {
        const auto slot = getBoolParamSlot (i);
        if (slot.channelName.isNotEmpty())
            push (apvts.getParameter (getBoolParamID (i)), slot.defaultValue ? 1.0f : 0.0f);
    }

    for (int i = 0; i < numChoiceParams; ++i)
    {
        const auto slot = getChoiceParamSlot (i);
        if (slot.channelName.isNotEmpty())
            push (apvts.getParameter (getChoiceParamID (i)),
                  maxChoiceOptions > 1 ? (float) slot.defaultIndex / (float) (maxChoiceOptions - 1) : 0.0f);
    }
}

bool CsoundAudioProcessor::hasLinkedFileChangedOnDisk() const
{
    juce::File file;
    juce::Time modTimeAtLoad;
    {
        const juce::ScopedLock sl (sessionLock);
        file = linkedCsdFile;
        modTimeAtLoad = linkedFileModTimeAtLoad;
    }

    if (file == juce::File{})
        return false;

    const auto access = accessFile (file);
    const auto& real = access->resolvedFile;

    if (! real.existsAsFile())
        return false;

    // Data sconosciuta (es. il file mancava al ripristino ed e' ricomparso):
    // non sappiamo cosa contenga rispetto alla sessione - meglio chiedere.
    if (modTimeAtLoad == juce::Time())
        return true;

    return real.getLastModificationTime() != modTimeAtLoad;
}

void CsoundAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Vedi il commento esteso in PluginProcessor.h per il formato.
    // juce::XmlElement costruito a mano (non un ValueTree): il testo
    // incorporato va in un elemento di TESTO figlio (addTextElement), che
    // un ValueTree non sa rappresentare.
    const auto sessionText = buildSessionText();
    const auto linked = getLinkedCsdFile();
    const auto base = getBaseFolder();

    juce::XmlElement root (kStateRoot);

    if (linked != juce::File{})
    {
        const bool relative = linked.isAChildOf (base);
        root.setAttribute ("csd", relative ? linked.getRelativePathFrom (base) : linked.getFullPathName());

        // iOS: bookmark security-scoped del file (vedi SecurityScopedFile.h).
        {
            const juce::ScopedLock sl (sessionLock);
            if (fileBookmark.getSize() > 0 && bookmarkedFile == linked)
                root.setAttribute ("csdBookmark", fileBookmark.toBase64Encoding());
        }
        root.setAttribute ("csdIsRelative", relative ? 1 : 0);
    }
    else
    {
        root.setAttribute ("csd", "");
        root.setAttribute ("csdIsRelative", 0);
    }

    // Stato "•" al momento del salvataggio del progetto (vedi
    // setStateInformation): se la sessione aveva modifiche non scritte su
    // file, al ripristino si usera' la copia incorporata invece del disco.
    // baseline/fileModTime servono a far ripartire l'indicatore "•" e il
    // controllo "modificato da fuori" esattamente da dove erano. Con il
    // file mancante la baseline non ha senso (nessun file la rappresenta):
    // vuota, cosi' alla riapertura la sessione risulta comunque "•".
    {
        const bool dirty = isSessionDirty();
        const juce::ScopedLock sl (sessionLock);
        root.setAttribute ("dirty", dirty ? 1 : 0);
        root.setAttribute ("followCsdChannels", followCsdChannels.load() ? 1 : 0);
        root.setAttribute ("baseline", linkedFileMissing ? juce::String() : sessionBaselineHash);
        root.setAttribute ("fileModTime", juce::String (linkedFileModTimeAtLoad.toMilliseconds()));
    }

    auto* embedded = root.createNewChildElement (kStateEmbedded);
    embedded->addTextElement (sessionText);

    // Bozza dell'editor, SOLO se diversa dal codice in esecuzione (il caso
    // normale dopo un Apply non la scrive): al ripristino l'editor mostra
    // di nuovo il testo non applicato, con il bordo rosso su Apply.
    {
        const auto draft = getEditorDraft();
        if (draft != getCsdText())
            root.createNewChildElement (kStateDraft)->addTextElement (draft);
    }

    // VALORI correnti per nome canale, in unita' reali (non normalizzati):
    // cosi' un riordino degli slot nel .csd, o un cambio di range, non
    // sposta i valori su un parametro sbagliato. type: f/i/b/c = Float/
    // Int/Bool/Choice, per rimappare al pool giusto al ripristino.
    auto addParam = [&root] (const juce::String& channel, const char* type, double value)
    {
        auto* p = root.createNewChildElement (kStateParam);
        p->setAttribute ("channel", channel);
        p->setAttribute ("type", type);
        p->setAttribute ("value", value);
    };

    for (int i = 0; i < numChannelParams; ++i)
    {
        const auto slot = getChannelParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        if (auto* raw = apvts.getRawParameterValue (getChannelParamID (i)))
            addParam (slot.channelName, "f", (double) denormalizeChannelParam (slot, raw->load()));
    }

    for (int i = 0; i < numIntParams; ++i)
    {
        const auto slot = getIntParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        if (auto* raw = apvts.getRawParameterValue (getIntParamID (i)))
            addParam (slot.channelName, "i", (double) denormalizeIntParam (slot, raw->load() / (float) intHostRangeMax));
    }

    for (int i = 0; i < numBoolParams; ++i)
    {
        const auto slot = getBoolParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        if (auto* raw = apvts.getRawParameterValue (getBoolParamID (i)))
            addParam (slot.channelName, "b", raw->load() >= 0.5f ? 1.0 : 0.0);
    }

    for (int i = 0; i < numChoiceParams; ++i)
    {
        const auto slot = getChoiceParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        if (auto* raw = apvts.getRawParameterValue (getChoiceParamID (i)))
            addParam (slot.channelName, "c", (double) juce::roundToInt (raw->load()));
    }

    copyXmlToBinary (root, destData);
}

void CsoundAudioProcessor::applyRestoredValues (const juce::XmlElement& pluginStateXml)
{
    // Valori salvati, per tipo + nome canale (vedi getStateInformation).
    std::map<juce::String, double> saved;
    for (auto* p : pluginStateXml.getChildWithTagNameIterator (kStateParam))
        saved[p->getStringAttribute ("type") + ":" + p->getStringAttribute ("channel")] = p->getDoubleAttribute ("value");

    auto find = [&saved] (const char* type, const juce::String& channel, double& out)
    {
        const auto it = saved.find (juce::String (type) + ":" + channel);
        if (it == saved.end())
            return false;
        out = it->second;
        return true;
    };

    auto push = [] (juce::RangedAudioParameter* param, float normalized)
    {
        if (param != nullptr)
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalized));
    };

    // UNA sola notifica per parametro: valore salvato se c'e', altrimenti il
    // default dello slot (un parametro aggiunto al file dopo il salvataggio
    // del progetto riparte dal suo default).
    for (int i = 0; i < numChannelParams; ++i)
    {
        const auto slot = getChannelParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        double v = slot.defaultValue;
        find ("f", slot.channelName, v);
        push (apvts.getParameter (getChannelParamID (i)), normalizeChannelParam (slot, v));
    }

    for (int i = 0; i < numIntParams; ++i)
    {
        const auto slot = getIntParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        double v = slot.defaultValue;
        find ("i", slot.channelName, v);
        push (apvts.getParameter (getIntParamID (i)), normalizeIntParam (slot, v));
    }

    for (int i = 0; i < numBoolParams; ++i)
    {
        const auto slot = getBoolParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        double v = slot.defaultValue ? 1.0 : 0.0;
        find ("b", slot.channelName, v);
        push (apvts.getParameter (getBoolParamID (i)), v >= 0.5 ? 1.0f : 0.0f);
    }

    for (int i = 0; i < numChoiceParams; ++i)
    {
        const auto slot = getChoiceParamSlot (i);
        if (slot.channelName.isEmpty())
            continue;
        double v = slot.defaultIndex;
        find ("c", slot.channelName, v);
        push (apvts.getParameter (getChoiceParamID (i)),
              maxChoiceOptions > 1 ? (float) v / (float) (maxChoiceOptions - 1) : 0.0f);
    }
}

void CsoundAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    if (! xml->hasTagName (kStateRoot))
        return;

    // --- 1. Risolve il file collegato ---------------------------------
    const auto csdPath = xml->getStringAttribute ("csd");
    const bool isRelative = xml->getIntAttribute ("csdIsRelative", 1) != 0;
    followCsdChannels.store (xml->getIntAttribute ("followCsdChannels", 1) != 0);

    juce::File linked;
    if (csdPath.isNotEmpty())
        linked = isRelative ? getBaseFolder().getChildFile (csdPath) : juce::File (csdPath);

    // --- 2. Quale versione usare (MAI un dialogo qui) -------------------
    //   file presente e sessione pulita al salvataggio -> il file (la verita')
    //   file presente ma sessione "•" al salvataggio   -> copia incorporata
    //        + bozza: le modifiche non salvate su file non si perdono (BUG
    //        corretto: prima vinceva sempre il disco), "•" resta acceso
    //   file mancante                                  -> copia incorporata + barra
    //   nessun file collegato                          -> copia incorporata
    const bool dirtyAtSave = xml->getIntAttribute ("dirty", 0) != 0;

    // "Utilizzabile" = esiste E ha un contenuto (BUG corretto: un file vuoto
    // - sync interrotta, conflitto, troncamento - veniva "caricato" come
    // nulla, la sessione risultava collegata e salvata, e la copia
    // incorporata andava persa al salvataggio successivo del progetto). Un
    // file vuoto o illeggibile e' trattato come mancante: copia incorporata
    // + barra di avviso.
    // iOS: bookmark salvato -> accesso al file fuori dalla sandbox (e path
    // aggiornato se l'utente lo ha spostato).
    {
        juce::MemoryBlock bm;
        if (bm.fromBase64Encoding (xml->getStringAttribute ("csdBookmark")) && bm.getSize() > 0 && linked != juce::File{})
        {
            setFileBookmark (linked, bm);
            const auto access = accessFile (linked);

            if (access->active && access->resolvedFile != linked)
            {
                // Spostato/rinominato: da ora il path e' quello nuovo.
                juce::MemoryBlock current;
                {
                    const juce::ScopedLock sl (sessionLock);
                    current = fileBookmark;
                }
                setFileBookmark (access->resolvedFile, current);
                linked = access->resolvedFile;
            }
        }
    }

    juce::String diskText;
    bool linkedExists = false;
    if (linked != juce::File{})
    {
        const auto access = accessFile (linked);
        linkedExists = access->resolvedFile.existsAsFile();
    }
    if (linkedExists)
        diskText = readSessionFile (linked);

    const bool fileExists = diskText.trim().isNotEmpty();
    const bool useDisk    = fileExists && ! dirtyAtSave;

    juce::String textToLoad;

    if (useDisk)
        textToLoad = diskText;
    else if (auto* embedded = xml->getChildByName (kStateEmbedded))
        textToLoad = embedded->getAllSubText();

    // --- 3. Carica struttura + codice (SENZA reset ai default), poi i
    //        valori in un solo passaggio (vedi applyRestoredValues). -------
    if (textToLoad.isNotEmpty())
        loadSessionStructureAndCode (textToLoad);

    applyRestoredValues (*xml);

    // Bozza non applicata (solo con la copia incorporata: se si usa il
    // disco, il codice e' quello del file).
    if (! useDisk)
        if (auto* draft = xml->getChildByName (kStateDraft))
            setEditorDraft (draft->getAllSubText());

    if (useDisk)
    {
        juce::Time modTime;
        {
            const auto access = accessFile (linked);
            modTime = access->resolvedFile.getLastModificationTime();
        }
        {
            const juce::ScopedLock sl (sessionLock);
            linkedCsdFile = linked;
            linkedFileModTimeAtLoad = modTime;
            linkedFileMissing = false;
            restoredDirty = false;
        }

        updateSessionBaselineHash();

        // Punto 4: il file e' cambiato da quando il progetto e' stato salvato
        // (git pull, altro editor)? E' caricato comunque (il file e' la
        // verita'), ma lo si segnala con UNA riga in consolle - niente barre
        // ne' dialoghi.
        const auto savedBaseline = xml->getStringAttribute ("baseline");
        if (savedBaseline.isNotEmpty() && savedBaseline != getSessionBaselineHash())
            handleMessage ("--- " + linked.getFileName()
                           + " changed on disk since this project was saved: the disk version was loaded ---");
    }
    else
    {
        const juce::ScopedLock sl (sessionLock);
        linkedCsdFile = linked;
        linkedFileMissing = linked != juce::File{} && ! fileExists;
        // Solo "aveva modifiche non salvate su file": un file semplicemente
        // spostato con la sessione pulita NON deve far chiedere conferma a
        // Relocate... (prima chiedeva sempre, anche con contenuto identico).
        restoredDirty = dirtyAtSave;

        // Riparte da dove era: baseline del file e sua data di modifica al
        // momento del salvataggio del progetto (vedi getStateInformation).
        sessionBaselineHash = xml->getStringAttribute ("baseline");
        linkedFileModTimeAtLoad = juce::Time (xml->getStringAttribute ("fileModTime").getLargeIntValue());
    }

    // loadSessionFromText non ricompila da solo (e' usato anche da Load
    // CSD, dove e' l'editor a chiamare Apply): qui invece, come nel vecchio
    // setStateInformation, va schedulata la ricompilazione del testo
    // appena ripristinato.
    // Sessione sostituita dall'host: alla prossima apertura l'editor
    // ricarica il documento e azzera la cronologia (se e' gia' aperto lo fa
    // sessionStateRestored). E si fotografa lo stato "appena ripristinato"
    // per Relocate... (vedi getRestoredSessionHash).
    documentResyncRequested = true;
    {
        const auto hash = computeTextHash (buildSessionTextFor (getEditorDraft()));
        const juce::ScopedLock sl (sessionLock);
        restoredSessionHash = hash;
    }

    scheduleRecompileAfterRestore (getCsdText());

    // L'editor (se aperto) rilegge tutto e mostra l'eventuale avviso.
    // callAsync: setStateInformation puo' arrivare da un thread dell'host.
    juce::MessageManager::callAsync ([this, alive = aliveFlag]
    {
        if (! alive->load())
            return;

        listeners.call ([] (Listener& l) { l.sessionStateRestored(); });
    });
}

//==============================================================================
// Import automatico da <Cabbage> - vedi il commento su importCabbageParameters
// in PluginProcessor.h per la mappatura widget -> tipo di parametro. Piccoli
// helper regex isolati qui (std::regex, non juce - JUCE non ha un tokenizer
// per espressioni "nomeCampo(...)" pronto all'uso) che leggono UNA riga alla
// volta: lo stile Cabbage compatto (una dichiarazione di widget per riga) e'
// quello assunto, righe spezzate su piu' linee non sono gestite.
namespace
{
    bool cabbageMatchSingleChannel (const juce::String& line, juce::String& channelName)
    {
        // Delimitatore custom "re(...)re" (non il default "(...)"): il
        // pattern contiene ")\"" al suo interno (chiusura del gruppo subito
        // seguita dalla virgoletta letterale), che con il delimitatore di
        // default avrebbe chiuso la raw string PRIMA della fine vera - il
        // resto del pattern sarebbe finito fuori dalle virgolette come
        // codice C++ invece che come testo, mandando in errore tutto cio'
        // che segue (esattamente l'errore "undeclared identifier"/"expected
        // ';'" segnalato).
        static const std::regex re (R"re(channel\(\s*"([^"]*)"\s*\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            channelName = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    bool cabbageMatchChannelPair (const juce::String& line, juce::String& channelA, juce::String& channelB)
    {
        // Stesso motivo del delimitatore custom spiegato in
        // cabbageMatchSingleChannel sopra - qui il pattern ha ADDIRITTURA
        // due occorrenze di ")\"" al suo interno.
        static const std::regex re (R"re(channel\(\s*"([^"]*)"\s*,\s*"([^"]*)"\s*\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            channelA = juce::String (m[1].str());
            channelB = juce::String (m[2].str());
            return true;
        }

        return false;
    }

    bool cabbageMatchRangeContents (const juce::String& line, juce::String& contents)
    {
        static const std::regex re (R"re(range\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            contents = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    // rangeX(...)/rangeY(...) - SOLO per il widget xypad (vedi il branch
    // "xypad" in importCabbageParameters): a differenza di range(...) sopra
    // (riusato invariato da hslider/vslider/rslider/nslider/vrange/hrange),
    // xypad ha due direttive SEPARATE, una per asse, invece di una singola
    // range() condivisa - regex dedicate, "rangeX\(" non puo' confondersi
    // con "range\(" (richiede la X subito dopo "range", non presente nella
    // sintassi degli altri widget).
    bool cabbageMatchRangeXContents (const juce::String& line, juce::String& contents)
    {
        static const std::regex re (R"re(rangeX\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            contents = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    bool cabbageMatchRangeYContents (const juce::String& line, juce::String& contents)
    {
        static const std::regex re (R"re(rangeY\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            contents = juce::String (m[1].str());
            return true;
        }

        return false;
    }

    bool cabbageMatchValueNumber (const juce::String& line, double& value)
    {
        static const std::regex re (R"re(value\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            value = juce::String (m[1].str()).trim().getDoubleValue();
            return true;
        }

        return false;
    }

    // min(...)/max(...) - SOLO per il widget encoder (vedi il branch
    // "encoder" in importCabbageParameters): a differenza di
    // hslider/vslider/rslider/nslider/vrange/hrange, che prendono
    // min/max/default/skew/increment tutti insieme da un'unica range(...),
    // l'endless encoder di Cabbage li espone come due proprieta' separate
    // e NIENTE default/skew/increment (fissi, vedi il branch).
    bool cabbageMatchMinNumber (const juce::String& line, double& value)
    {
        static const std::regex re (R"re(min\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            value = juce::String (m[1].str()).trim().getDoubleValue();
            return true;
        }

        return false;
    }

    bool cabbageMatchMaxNumber (const juce::String& line, double& value)
    {
        static const std::regex re (R"re(max\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
        {
            value = juce::String (m[1].str()).trim().getDoubleValue();
            return true;
        }

        return false;
    }

    bool cabbageMatchItems (const juce::String& line, juce::StringArray& items)
    {
        // La proprieta' Cabbage con le etichette del combobox e'
        // text("A", "B", ...), NON items(...) (quest'ultimo tentato prima
        // per sbaglio - un combobox Cabbage reale usa sempre text()).
        // Proviamo comunque anche items(...) come fallback, nel caso un
        // file scritto a mano/esportato da un'altra versione lo usi.
        static const std::regex reText  (R"re(text\(([^)]*)\))re");
        static const std::regex reItems (R"re(items\(([^)]*)\))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (! std::regex_search (s, m, reText) && ! std::regex_search (s, m, reItems))
            return false;

        juce::StringArray rawTokens;
        rawTokens.addTokens (juce::String (m[1].str()), ",", "");

        for (auto token : rawTokens)
        {
            token = token.trim();

            if (token.startsWithChar ('"') && token.endsWithChar ('"'))
                token = token.substring (1, token.length() - 1);

            items.add (token);
        }

        return true;
    }

    // Spezza "min, max, default, skew, increment" (il contenuto grezzo di
    // range(...), vedi cabbageMatchRangeContents) in fino a 5 numeri. Il
    // campo "default" (indice 2) puo' contenere un secondo numero separato
    // da ':' - solo per vrange/hrange (vedi il commento in
    // PluginProcessor.h) - che finisce in defaultB; altrimenti defaultB
    // resta identico a values[2].
    //
    // Cabbage accetta range() anche con MENO di 5 campi (tipicamente solo
    // min, max, default - skew e increment sono opzionali con un loro
    // default): qui sovrascriviamo SOLO i campi effettivamente presenti,
    // lasciando gli altri al valore che il chiamante ha gia' messo in
    // "values" prima di chiamare questa funzione (gli inizializzatori
    // {0, 1, 0, 1, 0.001} nei tre punti da cui viene chiamata) - cosi' un
    // range(-10, 10, 6.791) con solo 3 campi imposta min/max/default e
    // lascia skew=1.0/increment=0.001 come se non fossero stati scritti.
    bool cabbageParseFiveFieldRange (const juce::String& contents, double values[5], double& defaultB)
    {
        juce::StringArray tokens;
        tokens.addTokens (contents, ",", "");

        if (tokens.isEmpty())
            return false;

        const int fieldCount = juce::jmin (tokens.size(), 5);
        defaultB = values[2]; // valore di partenza se il campo 2 non e' presente o non ha ':'

        for (int i = 0; i < fieldCount; ++i)
        {
            auto token = tokens[i].trim();

            if (i == 2 && token.containsChar (':'))
            {
                const auto colon = token.indexOfChar (':');
                values[2] = token.substring (0, colon).trim().getDoubleValue();
                defaultB  = token.substring (colon + 1).trim().getDoubleValue();
            }
            else
            {
                values[i] = token.getDoubleValue();

                if (i == 2)
                    defaultB = values[2];
            }
        }

        return true;
    }

    // Spezza "min, max, default" (il contenuto grezzo di rangeX(...)/
    // rangeY(...), vedi cabbageMatchRangeXContents/cabbageMatchRangeYContents
    // sopra) in fino a 3 numeri - SOLO per xypad: a differenza di
    // cabbageParseFiveFieldRange sopra, qui non c'e' ne' skew/increment
    // (fissi, vedi il branch "xypad" in importCabbageParameters) ne' la
    // sintassi "default1:default2" di vrange/hrange (ogni asse ha gia' il
    // proprio default separato, tramite rangeX/rangeY distinti). Stessa
    // tolleranza di cabbageParseFiveFieldRange per un numero di campi
    // inferiore al massimo: i campi assenti restano al valore che il
    // chiamante ha gia' messo in "values" prima di chiamare questa funzione.
    bool cabbageParseThreeFieldRange (const juce::String& contents, double values[3])
    {
        juce::StringArray tokens;
        tokens.addTokens (contents, ",", "");

        if (tokens.isEmpty())
            return false;

        const int fieldCount = juce::jmin (tokens.size(), 3);

        for (int i = 0; i < fieldCount; ++i)
            values[i] = tokens[i].trim().getDoubleValue();

        return true;
    }

    // Identifica il tipo di widget dal primo token della riga (dopo aver
    // scartato spazi iniziali) - non basta un contains() perche' "hslider"
    // compare anche dentro commenti o altri nomi; serve il token di testa.
    juce::String cabbageLeadingIdentifier (const juce::String& line)
    {
        static const std::regex re (R"re(^\s*([A-Za-z_][A-Za-z0-9_]*))re");
        std::smatch m;
        const auto s = line.toStdString();

        if (std::regex_search (s, m, re))
            return juce::String (m[1].str());

        return {};
    }
}

bool CsoundAudioProcessor::saveSessionToFile (const juce::File& file, const juce::String& codeText)
{
    // Formato: vedi buildSessionTextFor() (codice in chiaro + <CsoundParams>
    // con la sola STRUTTURA). codeText e' il testo dell'editor (vedi il
    // commento nel .h), non necessariamente quello in esecuzione.
    const auto text = buildSessionTextFor (codeText);

    // lineEndings = "\n" ESPLICITO (BUG corretto): il default di
    // juce::File::replaceWithText e' "\r\n", che riscriveva il file con fine
    // riga diversi dal testo appena hashato - rileggendolo, l'hash non
    // coincideva mai e performSaveLinked chiedeva "modificato da fuori,
    // sovrascrivere?" a OGNI Save. Lettura simmetrica: readSessionFile().
    // Scrittura tramite juce::URL::createOutputStream() (stesso schema di
    // NP2 per i file scelti dal document picker): su iOS JUCE apre e chiude
    // l'accesso security-scoped intorno alla scrittura usando il bookmark
    // agganciato alla URL. NON File::replaceWithText: quella passa da un
    // TemporaryFile nella STESSA cartella e poi lo rinomina sopra il target,
    // ma l'accesso vale per il solo file, non per la sua cartella (iCloud
    // Drive, provider esterni) e il Save falliva. Lo stream di JUCE e' un
    // FileOutputStream in APPEND: va troncato prima di scrivere.
    // Fine riga "\n" come prima (vedi readSessionFile, simmetrica).
    {
        const auto normalised = text.replace ("\r\n", "\n").replace ("\r", "\n");
        auto url = makeFileURL (file);
        auto out = url.createOutputStream();

        if (out == nullptr)
        {
            handleMessage ("Save: could not open an output stream for " + url.toString (false));
            return false;
        }

        if (auto* fileOut = dynamic_cast<juce::FileOutputStream*> (out.get()))
        {
            if (! fileOut->openedOk())
            {
                handleMessage ("Save: cannot open for writing: " + fileOut->getFile().getFullPathName()
                               + " (" + fileOut->getStatus().getErrorMessage() + ")");
                return false;
            }

            fileOut->setPosition (0);
            fileOut->truncate();
        }

        if (! out->write (normalised.toRawUTF8(), normalised.getNumBytesAsUTF8()))
        {
            handleMessage ("Save: write failed on " + url.toString (false));
            return false;
        }

        out->flush();
        out.reset(); // chiude (e su iOS rilascia l'accesso) PRIMA di leggere data/hash
    }

    const auto access = accessFile (file);
    const auto& real = access->resolvedFile;

    const auto modTime = real.getLastModificationTime(); // appena scritto da noi

    // Nuovo file dentro la sandbox/App Group (nessun bookmark dal picker):
    // se ne crea uno comunque, cosi' la logica e' uniforme su iOS.
    {
        bool hasBookmark;
        {
            const juce::ScopedLock sl (sessionLock);
            hasBookmark = fileBookmark.getSize() > 0 && bookmarkedFile == file;
        }

        if (! hasBookmark)
        {
            const auto bm = SecurityScopedFile::makeBookmark (real);
            if (bm.getSize() > 0)
                setFileBookmark (file, bm);
        }
    }

    const auto hash = computeTextHash (text);
    const juce::ScopedLock sl (sessionLock);
    linkedCsdFile = file;
    linkedFileModTimeAtLoad = modTime;
    sessionBaselineHash = hash; // appena scritto: sessione e file coincidono
    linkedFileMissing = false;
    restoredDirty = false;
    return true;
}

void CsoundAudioProcessor::resetAllParameterSlots()
{
    for (int i = 0; i < numChannelParams; ++i)
        setChannelParamSlot (i, ChannelParamSlot{});
    for (int i = 0; i < numIntParams; ++i)
        setIntParamSlot (i, IntParamSlot{});
    for (int i = 0; i < numBoolParams; ++i)
        setBoolParamSlot (i, BoolParamSlot{});
    for (int i = 0; i < numChoiceParams; ++i)
        setChoiceParamSlot (i, ChoiceParamSlot{});
}

bool CsoundAudioProcessor::loadSessionFromFile (const juce::File& file)
{
    const auto fullText = readSessionFile (file);

    if (fullText.isEmpty() || ! loadSessionFromText (fullText))
        return false;

    juce::Time modTime;
    {
        const auto access = accessFile (file);
        modTime = access->resolvedFile.getLastModificationTime();

        bool hasBookmark;
        {
            const juce::ScopedLock sl (sessionLock);
            hasBookmark = fileBookmark.getSize() > 0 && bookmarkedFile == file;
        }

        if (! hasBookmark)
        {
            const auto bm = SecurityScopedFile::makeBookmark (access->resolvedFile);
            if (bm.getSize() > 0)
                setFileBookmark (file, bm);
        }
    }

    // Collega la sessione al file appena letto: da ora "Save" sovrascrive
    // questo file (vedi CsoundAudioProcessorEditor::performSaveLinked) e
    // lo stato del progetto ne salva il path.
    {
        const juce::ScopedLock sl (sessionLock);
        linkedCsdFile = file;
        linkedFileModTimeAtLoad = modTime;
        linkedFileMissing = false;
        restoredDirty = false;
    }

    updateSessionBaselineHash();
    return true;
}

bool CsoundAudioProcessor::loadSessionFromText (const juce::String& fullText)
{
    if (! loadSessionStructureAndCode (fullText))
        return false;

    // Struttura nuova -> valori ai default (vedi resetParameterValuesToDefaults).
    resetParameterValuesToDefaults();
    return true;
}

bool CsoundAudioProcessor::loadSessionStructureAndCode (const juce::String& fullText)
{
    if (fullText.isEmpty())
        return false;

    // Ripulisce SEMPRE la mappatura dei parametri prima di qualunque altra
    // cosa, qualunque sia il formato del file che segue - vedi il commento
    // su resetAllParameterSlots() in PluginProcessor.h sul perche'.
    resetAllParameterSlots();

    const auto tagStart = fullText.indexOf (kParamsTagOpen);

    if (tagStart < 0)
    {
        // Nessun tag d'appendice: e' un .csd "normale", magari scritto a
        // mano o esportato da un'altra sessione - carichiamo comunque il
        // codice (molto meglio che fallire del tutto).
        setCsdCodeFromFullText (fullText);

        // Se contiene un <Cabbage>, non e' mai stato salvato da questo
        // plugin ma e' probabilmente un .csd Cabbage scritto a mano/da
        // un'altra app: proviamo a dedurre il mapping dei parametri dai
        // widget, invece di lasciare INVARIATO (o vuoto) quello attuale -
        // vedi importCabbageParameters in PluginProcessor.h. Se non c'e'
        // un <Cabbage>, si prova con un pannello di CsoundQt (<bsbPanel>) -
        // vedi importCsoundQtParameters.
        if (! importCabbageParameters (fullText))
            importCsoundQtParameters (fullText);
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
        setCsdCodeFromFullText (codeText);
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
    setCsdCodeFromFullText (codeText);
    return true;
}

void CsoundAudioProcessor::initializeSession (bool useTemplate)
{
    // Stesso ordine di loadSessionFromFile sopra: azzera SEMPRE la
    // mappatura prima di toccare il codice, incondizionatamente.
    // useTemplate = true ("Init Session Template"): il template
    // (defaultCsdText) porta con se' un blocco <apeCsoundParams> (Delay,
    // Feedback, Wave): lo stesso percorso di Load, cosi' i parametri del
    // template vengono creati e portati ai loro default.
    // useTemplate = false ("Init Session Clear"): scheletro .csd vuoto,
    // nessun parametro. (loadSessionStructureAndCode azzera gia' gli slot.)
    const auto text = useTemplate ? defaultCsdText() : emptyCsdText();

    if (! loadSessionStructureAndCode (text))
    {
        resetAllParameterSlots();
        setCsdCodeFromFullText (text);
    }

    resetParameterValuesToDefaults();

    // Sessione nuova = NON collegata a nessun file: il prossimo "Save" si
    // comporta come "Save As" (vedi performSaveLinked nell'editor).
    unlinkSession();
    updateSessionBaselineHash();
}

bool CsoundAudioProcessor::importCsoundQtParameters (const juce::String& csdText)
{
    // Vedi il commento sulla dichiarazione in PluginProcessor.h per la
    // tabella completa widget -> parametro.
    const auto panelStart = csdText.indexOf ("<bsbPanel");
    if (panelStart < 0)
        return false;

    static const juce::String kPanelClose = "</bsbPanel>";
    const auto panelEnd = csdText.indexOf (panelStart, kPanelClose);
    if (panelEnd < 0)
        return false;

    // CsoundQt scrive il pannello con QXmlStreamWriter: e' XML valido, si
    // usa il parser di JUCE invece di regex riga per riga come per Cabbage.
    const auto panelText = csdText.substring (panelStart, panelEnd + kPanelClose.length());
    auto panel = juce::XmlDocument::parse (panelText);

    if (panel == nullptr)
    {
        handleMessage ("--- CsoundQt import: <bsbPanel> found but it is not valid XML - no parameters mapped ---");
        return false;
    }

    // Un nuovo import RIMPIAZZA la mappatura precedente (stessa regola di
    // importCabbageParameters).
    for (int i = 0; i < numChannelParams; ++i)
        setChannelParamSlot (i, ChannelParamSlot{});
    for (int i = 0; i < numBoolParams; ++i)
        setBoolParamSlot (i, BoolParamSlot{});
    for (int i = 0; i < numChoiceParams; ++i)
        setChoiceParamSlot (i, ChoiceParamSlot{});

    int floatIndex = 0, boolIndex = 0, choiceIndex = 0;
    int skippedFloat = 0, skippedBool = 0, skippedChoice = 0, skippedDuplicate = 0;
    int limitedRanges = 0;
    std::set<juce::String> usedChannels;

    auto text = [] (const juce::XmlElement& obj, const char* tag)
    {
        return obj.getChildElementAllSubText (tag, {}).trim();
    };

    auto number = [&text] (const juce::XmlElement& obj, const char* tag, double fallback)
    {
        const auto t = text (obj, tag);
        return t.isEmpty() ? fallback : t.getDoubleValue();
    };

    // true se il canale e' nuovo (e lo prenota), false se va saltato.
    auto claim = [&usedChannels, &skippedDuplicate] (const juce::String& channel)
    {
        if (channel.isEmpty())
            return false;
        if (! usedChannels.insert (channel).second)
        {
            ++skippedDuplicate;
            return false;
        }
        return true;
    };

    auto addFloat = [this, &floatIndex, &skippedFloat] (const juce::String& channel,
                                                       double minV, double maxV, double defV, double step,
                                                       bool exponential = false)
    {
        if (floatIndex >= numChannelParams)
        {
            ++skippedFloat;
            return;
        }

        if (maxV < minV)
            std::swap (minV, maxV);   // come fa CsoundQt stesso (QuteSlider/QuteKnob)

        ChannelParamSlot slot;
        slot.channelName  = channel;
        slot.minValue     = (float) minV;
        slot.maxValue     = (float) maxV;
        slot.defaultValue = (float) juce::jlimit (minV, maxV, defV);
        slot.skew         = 1.0f;
        slot.increment    = (float) (step > 0.0 ? step : 0.001);

        // <mode>exp</mode> di slider/knob: CsoundQt mappa la corsa in modo
        // esponenziale (il centro corsa vale sqrt(min*max), possibile solo
        // con estremi positivi). Lo skew di JUCE/Cabbage non e' la stessa
        // curva, ma con lo skew "dal punto medio" (stessa formula di
        // juce::Slider::setSkewFactorFromMidPoint) il centro coincide.
        if (exponential && minV > 0.0 && maxV > minV)
        {
            const double mid = std::sqrt (minV * maxV);
            slot.skew = (float) (std::log (0.5) / std::log ((mid - minV) / (maxV - minV)));
        }

        setChannelParamSlot (floatIndex++, slot);
    };

    // Checkbox e bottoni "value": in CsoundQt mandano pressedValue (default
    // 1) quando attivi e 0 altrimenti. Con pressedValue 1 e' un Bool; con
    // un altro valore serve un Float a due posizioni {0, pressedValue}.
    auto addSwitch = [this, &boolIndex, &skippedBool, &addFloat] (const juce::String& channel,
                                                                  bool on, double pressedValue)
    {
        if (! juce::approximatelyEqual (pressedValue, 1.0) && ! juce::approximatelyEqual (pressedValue, 0.0))
        {
            addFloat (channel, juce::jmin (0.0, pressedValue), juce::jmax (0.0, pressedValue),
                      on ? pressedValue : 0.0, std::abs (pressedValue));
            return;
        }

        if (boolIndex >= numBoolParams)
        {
            ++skippedBool;
            return;
        }

        BoolParamSlot slot;
        slot.channelName  = channel;
        slot.defaultValue = on;
        setBoolParamSlot (boolIndex++, slot);
    };

    // Ricorsivo: i bsbObject sono figli diretti di <bsbPanel>, ma cercarli a
    // qualunque profondita' non costa nulla e regge eventuali raggruppamenti.
    std::function<void (const juce::XmlElement&)> visit = [&] (const juce::XmlElement& parent)
    {
        for (auto* obj : parent.getChildIterator())
        {
            if (! obj->hasTagName ("bsbObject"))
            {
                visit (*obj);
                continue;
            }

            const auto type    = obj->getStringAttribute ("type");
            const auto channel = text (*obj, "objectName");

            if (type == "BSBVSlider" || type == "BSBHSlider" || type == "BSBSlider" || type == "BSBKnob")
            {
                if (claim (channel))
                {
                    // resolution: -1 (slider, "nessuna") o 0.01 (knob);
                    // knob in integerMode -> passo 1.
                    double step = number (*obj, "resolution", -1.0);

                    if (type == "BSBKnob" && text (*obj, "integerMode") == "true")
                        step = 1.0;

                    addFloat (channel, number (*obj, "minimum", 0.0), number (*obj, "maximum", 1.0),
                              number (*obj, "value", 0.0), step > 0.0 ? step : 0.001,
                              text (*obj, "mode") == "exp");
                }
            }
            else if (type == "BSBSpinBox" || type == "BSBScrollNumber")
            {
                if (claim (channel))
                {
                    // Range limitato (richiesta esplicita): CsoundQt di
                    // default scrive +-1e12 / +-999999999999 per "nessun
                    // limite", che come slider DAW sarebbe inutilizzabile (e
                    // lo step perderebbe precisione in float). Ogni estremo
                    // oltre kNumberBoxRangeLimit viene riportato al limite;
                    // il default resta sempre dentro il range risultante.
                    constexpr double kNumberBoxRangeLimit = 10000.0;

                    double minV = number (*obj, "minimum", 0.0);
                    double maxV = number (*obj, "maximum", 1.0);
                    const double defV = number (*obj, "value", 0.0);

                    if (maxV < minV)
                        std::swap (minV, maxV);

                    if (std::abs (minV) > kNumberBoxRangeLimit || std::abs (maxV) > kNumberBoxRangeLimit)
                    {
                        ++limitedRanges;
                        minV = juce::jlimit (-kNumberBoxRangeLimit, kNumberBoxRangeLimit, minV);
                        maxV = juce::jlimit (-kNumberBoxRangeLimit, kNumberBoxRangeLimit, maxV);
                        minV = juce::jmin (minV, defV);   // mai tagliare fuori il default
                        maxV = juce::jmax (maxV, defV);

                        if (maxV <= minV)                 // entrambi gli estremi dallo stesso lato
                        {
                            minV = defV - kNumberBoxRangeLimit;
                            maxV = defV + kNumberBoxRangeLimit;
                        }
                    }

                    addFloat (channel, minV, maxV, defV, number (*obj, "resolution", 0.001));
                }
            }
            else if (type == "BSBController")
            {
                if (claim (channel))
                    addFloat (channel, number (*obj, "xMin", 0.0), number (*obj, "xMax", 1.0),
                              number (*obj, "xValue", 0.0), 0.001);

                const auto channel2 = text (*obj, "objectName2");
                if (claim (channel2))
                    addFloat (channel2, number (*obj, "yMin", 0.0), number (*obj, "yMax", 1.0),
                              number (*obj, "yValue", 0.0), 0.001);
            }
            else if (type == "BSBCheckBox")
            {
                if (claim (channel))
                    addSwitch (channel, text (*obj, "selected") == "true", number (*obj, "pressedValue", 1.0));
            }
            else if (type == "BSBButton")
            {
                // Solo i bottoni di tipo "value"/"pictvalue" scrivono un
                // valore sul canale (pressedValue premuto, 0 rilasciato;
                // con latch e' un interruttore). Quelli "event"/"pictevent"
                // lanciano eventi di score e "pict" e' solo un'immagine:
                // nessun parametro. I canali riservati "_Browse*" aprono
                // un file dialog: nessun parametro.
                const auto buttonType = text (*obj, "type");

                if ((buttonType == "value" || buttonType == "pictvalue")
                     && ! channel.startsWith ("_") && claim (channel))
                {
                    addSwitch (channel, text (*obj, "latched") == "true", number (*obj, "pressedValue", 1.0));
                }
            }
            else if (type == "BSBDropdown")
            {
                if (! claim (channel))
                    continue;

                if (choiceIndex >= numChoiceParams)
                {
                    ++skippedChoice;
                    continue;
                }

                ChoiceParamSlot slot;
                slot.channelName = channel;

                if (auto* list = obj->getChildByName ("bsbDropdownItemList"))
                    for (auto* item : list->getChildWithTagNameIterator ("bsbDropdownItem"))
                        if (slot.optionLabels.size() < maxChoiceOptions)
                            slot.optionLabels.add (item->getChildElementAllSubText ("name", {}).trim());

                slot.defaultIndex = juce::jlimit (0, juce::jmax (0, slot.optionLabels.size() - 1),
                                                  text (*obj, "selectedIndex").getIntValue());
                setChoiceParamSlot (choiceIndex++, slot);
            }
            // Altri tipi (BSBLabel, BSBDisplay, BSBLineEdit, BSBGraph,
            // BSBScope, BSBConsole...): nessun parametro.
        }
    };

    visit (*panel);

    juce::String summary;
    summary << "--- CsoundQt import: " << floatIndex << " float, " << boolIndex << " bool, "
            << choiceIndex << " choice parameters mapped";

    if (skippedFloat > 0)
        summary << " (" << skippedFloat << " float widget(s) skipped: over the limit of " << numChannelParams << ")";
    if (skippedBool > 0)
        summary << " (" << skippedBool << " checkbox(es) skipped: over the limit of " << numBoolParams << ")";
    if (skippedChoice > 0)
        summary << " (" << skippedChoice << " menu(s) skipped: over the limit of " << numChoiceParams << ")";
    if (skippedDuplicate > 0)
        summary << " - " << skippedDuplicate << " widget(s) skipped: channel name already used";
    if (limitedRanges > 0)
        summary << " - " << limitedRanges << " spin box/scroll number range(s) limited to +/-10000";

    summary << " ---";
    handleMessage (summary);
    return true;
}

bool CsoundAudioProcessor::importCabbageParameters (const juce::String& csdText)
{
    static const juce::String kCabbageOpen  = "<Cabbage>";
    static const juce::String kCabbageClose = "</Cabbage>";

    const auto sectionStart = csdText.indexOf (kCabbageOpen);

    if (sectionStart < 0)
        return false;

    const auto innerStart = sectionStart + kCabbageOpen.length();
    const auto sectionEnd = csdText.indexOf (innerStart, kCabbageClose);
    const auto cabbageSection = sectionEnd < 0 ? csdText.substring (innerStart)
                                                 : csdText.substring (innerStart, sectionEnd);

    // Un nuovo import RIMPIAZZA la mappatura precedente (non la somma):
    // gli indici assegnati qui sotto partono da zero, quindi senza questo
    // reset uno slot scritto da una sessione precedente ma non toccato da
    // questo file resterebbe incollato alla mappatura vecchia.
    for (int i = 0; i < numChannelParams; ++i)
        setChannelParamSlot (i, ChannelParamSlot{});
    for (int i = 0; i < numBoolParams; ++i)
        setBoolParamSlot (i, BoolParamSlot{});
    for (int i = 0; i < numChoiceParams; ++i)
        setChoiceParamSlot (i, ChoiceParamSlot{});

    int channelIndex = 0, boolIndex = 0, choiceIndex = 0;
    int skippedFloat = 0, skippedBool = 0, skippedChoice = 0, skippedListbox = 0;

    // setChannelParamSlot/setBoolParamSlot/setChoiceParamSlot aggiornano SOLO
    // i metadata dello slot (vedi il commento su ChannelParamSlot in
    // PluginProcessor.h: il parametro apvts resta sempre dov'era, i VALORI
    // sono affidati all'automazione host) - per questo, dopo un normale
    // rename/modifica range dal pannello Parametri, lo slider NON si sposta.
    // Qui invece e' un IMPORT di un file nuovo: non c'e' nessuna automazione
    // precedente da preservare, quindi il valore attuale del parametro va
    // spinto esplicitamente al suo nuovo default appena importato, altrimenti
    // resta a quello con cui l'host l'aveva istanziato (es. 0) e la UI
    // (slider della tab Generic Editor, agganciato al VALORE del parametro,
    // non al suo default) continua a mostrare quello vecchio anche se lo
    // slot e' stato importato bene - esattamente il problema segnalato.
    // getDefaultValue() di ciascun HostParameter legge gia' lo slot
    // CORRENTE (appena scritto dalla setXxxParamSlot qui sopra), quindi
    // basta richiederlo e rispedirlo come valore attuale - nessuna
    // duplicazione delle formule di normalizzazione.
    auto pushDefaultToHost = [this] (const juce::String& paramID)
    {
        if (auto* param = apvts.getParameter (paramID))
            param->setValueNotifyingHost (param->getDefaultValue());
    };

    for (auto line : juce::StringArray::fromLines (cabbageSection))
    {
        const auto identifier = cabbageLeadingIdentifier (line);

        if (identifier.isEmpty())
            continue;

        if (identifier == "hslider" || identifier == "vslider"
            || identifier == "rslider" || identifier == "nslider")
        {
            juce::String channelName;
            juce::String rangeContents;

            if (! cabbageMatchSingleChannel (line, channelName) || channelName.isEmpty())
                continue;

            double values[5] = { 0.0, 1.0, 0.0, 1.0, 0.001 };
            double unusedDefaultB = 0.0;

            if (cabbageMatchRangeContents (line, rangeContents))
                cabbageParseFiveFieldRange (rangeContents, values, unusedDefaultB);

            if (channelIndex >= numChannelParams)
            {
                ++skippedFloat;
                continue;
            }

            ChannelParamSlot slot;
            slot.channelName  = channelName;
            slot.minValue     = (float) values[0];
            slot.maxValue     = (float) values[1];
            slot.defaultValue = (float) values[2];
            slot.skew         = (float) values[3];
            slot.increment    = (float) values[4];
            setChannelParamSlot (channelIndex, slot);
            pushDefaultToHost (getChannelParamID (channelIndex));
            ++channelIndex;
        }
        else if (identifier == "vrange" || identifier == "hrange")
        {
            juce::String channelA, channelB, rangeContents;

            if (! cabbageMatchChannelPair (line, channelA, channelB))
                continue;

            double values[5] = { 0.0, 1.0, 0.0, 1.0, 0.001 };
            double defaultB = 0.0;

            if (cabbageMatchRangeContents (line, rangeContents))
                cabbageParseFiveFieldRange (rangeContents, values, defaultB);
            else
                defaultB = values[2];

            if (channelA.isNotEmpty())
            {
                if (channelIndex >= numChannelParams)
                {
                    ++skippedFloat;
                }
                else
                {
                    ChannelParamSlot slot;
                    slot.channelName  = channelA;
                    slot.minValue     = (float) values[0];
                    slot.maxValue     = (float) values[1];
                    slot.defaultValue = (float) values[2];
                    slot.skew         = (float) values[3];
                    slot.increment    = (float) values[4];
                    setChannelParamSlot (channelIndex, slot);
                    pushDefaultToHost (getChannelParamID (channelIndex));
                    ++channelIndex;
                }
            }

            if (channelB.isNotEmpty())
            {
                if (channelIndex >= numChannelParams)
                {
                    ++skippedFloat;
                }
                else
                {
                    ChannelParamSlot slot;
                    slot.channelName  = channelB;
                    slot.minValue     = (float) values[0];
                    slot.maxValue     = (float) values[1];
                    slot.defaultValue = (float) defaultB;
                    slot.skew         = (float) values[3];
                    slot.increment    = (float) values[4];
                    setChannelParamSlot (channelIndex, slot);
                    pushDefaultToHost (getChannelParamID (channelIndex));
                    ++channelIndex;
                }
            }
        }
        else if (identifier == "xypad")
        {
            // channel("xChan", "yChan") identico a vrange/hrange (vedi
            // cabbageMatchChannelPair sopra) - 2 slot Float, uno per asse.
            // A differenza di vrange/hrange pero' ogni asse ha il proprio
            // range INDIPENDENTE (rangeX/rangeY separati, non un'unica
            // range() con la sintassi "defaultA:defaultB"), e skew/increment
            // non sono affatto esposti da questo widget in Cabbage - fissi
            // a 1.0/0.001 per entrambi gli assi, richiesto esplicitamente.
            juce::String channelA, channelB, rangeXContents, rangeYContents;

            if (! cabbageMatchChannelPair (line, channelA, channelB))
                continue;

            constexpr float xyPadSkew = 1.0f;
            constexpr float xyPadIncrement = 0.001f;

            double valuesX[3] = { 0.0, 1.0, 0.0 };
            double valuesY[3] = { 0.0, 1.0, 0.0 };

            if (cabbageMatchRangeXContents (line, rangeXContents))
                cabbageParseThreeFieldRange (rangeXContents, valuesX);

            if (cabbageMatchRangeYContents (line, rangeYContents))
                cabbageParseThreeFieldRange (rangeYContents, valuesY);

            if (channelA.isNotEmpty())
            {
                if (channelIndex >= numChannelParams)
                {
                    ++skippedFloat;
                }
                else
                {
                    ChannelParamSlot slot;
                    slot.channelName  = channelA;
                    slot.minValue     = (float) valuesX[0];
                    slot.maxValue     = (float) valuesX[1];
                    slot.defaultValue = (float) valuesX[2];
                    slot.skew         = xyPadSkew;
                    slot.increment    = xyPadIncrement;
                    setChannelParamSlot (channelIndex, slot);
                    pushDefaultToHost (getChannelParamID (channelIndex));
                    ++channelIndex;
                }
            }

            if (channelB.isNotEmpty())
            {
                if (channelIndex >= numChannelParams)
                {
                    ++skippedFloat;
                }
                else
                {
                    ChannelParamSlot slot;
                    slot.channelName  = channelB;
                    slot.minValue     = (float) valuesY[0];
                    slot.maxValue     = (float) valuesY[1];
                    slot.defaultValue = (float) valuesY[2];
                    slot.skew         = xyPadSkew;
                    slot.increment    = xyPadIncrement;
                    setChannelParamSlot (channelIndex, slot);
                    pushDefaultToHost (getChannelParamID (channelIndex));
                    ++channelIndex;
                }
            }
        }
        else if (identifier == "encoder")
        {
            // Endless encoder: 1 slot Float, channel("nome") identico a
            // hslider/ecc. (cabbageMatchSingleChannel), ma min/max NON
            // vengono da range(...) - questo widget li espone come due
            // proprieta' separate, min(...) e max(...) - e il default non
            // e' affatto esposto in Cabbage per questo widget: assunto
            // uguale a min (nessuna indicazione migliore disponibile).
            // Skew/increment fissi a 1.0/0.001, come xypad sopra.
            juce::String channelName;

            if (! cabbageMatchSingleChannel (line, channelName) || channelName.isEmpty())
                continue;

            if (channelIndex >= numChannelParams)
            {
                ++skippedFloat;
                continue;
            }

            double minValue = 0.0, maxValue = 1.0;
            cabbageMatchMinNumber (line, minValue);
            cabbageMatchMaxNumber (line, maxValue);

            ChannelParamSlot slot;
            slot.channelName  = channelName;
            slot.minValue     = (float) minValue;
            slot.maxValue     = (float) maxValue;
            slot.defaultValue = (float) minValue;
            slot.skew         = 1.0f;
            slot.increment    = 0.001f;
            setChannelParamSlot (channelIndex, slot);
            pushDefaultToHost (getChannelParamID (channelIndex));
            ++channelIndex;
        }
        else if (identifier == "checkbox")
        {
            juce::String channelName;

            if (! cabbageMatchSingleChannel (line, channelName) || channelName.isEmpty())
                continue;

            if (boolIndex >= numBoolParams)
            {
                ++skippedBool;
                continue;
            }

            double value = 0.0;
            cabbageMatchValueNumber (line, value);

            BoolParamSlot slot;
            slot.channelName  = channelName;
            slot.defaultValue = ! juce::approximatelyEqual (value, 0.0);
            setBoolParamSlot (boolIndex, slot);
            pushDefaultToHost (getBoolParamID (boolIndex));
            ++boolIndex;
        }
        else if (identifier == "combobox")
        {
            juce::String channelName;

            if (! cabbageMatchSingleChannel (line, channelName) || channelName.isEmpty())
                continue;

            if (choiceIndex >= numChoiceParams)
            {
                ++skippedChoice;
                continue;
            }

            juce::StringArray items;
            cabbageMatchItems (line, items);

            // value(N) viene letto letteralmente, nessuna conversione
            // 1-based/0-based: il campo Default del pannello Choice deve
            // mostrare lo stesso numero scritto nel .csd.
            double value = 0.0;
            cabbageMatchValueNumber (line, value);

            ChoiceParamSlot slot;
            slot.channelName = channelName;

            for (int i = 0; i < juce::jmin (items.size(), maxChoiceOptions); ++i)
                slot.optionLabels.add (items[i]);

            slot.defaultIndex = juce::jlimit (0, juce::jmax (0, slot.optionLabels.size() - 1), (int) value);
            setChoiceParamSlot (choiceIndex, slot);
            pushDefaultToHost (getChoiceParamID (choiceIndex));
            ++choiceIndex;
        }
        else if (identifier == "listbox")
        {
            // Nessun parametro DAW per questo widget - vedi la tabella nel
            // commento di importCabbageParameters in PluginProcessor.h.
            ++skippedListbox;
        }
    }

    juce::String summary;
    summary << "--- Cabbage import: " << channelIndex << " float, " << boolIndex << " bool, "
            << choiceIndex << " choice parameters mapped";

    // Messaggio per tipo (non un conteggio unico "oltre il limite di 16"):
    // il limite per tipo NON e' piu' lo stesso per tutti da quando Float/
    // Int/Bool sono stati estesi a 64/32/32 (Choice resta a 16).
    if (skippedFloat > 0)
        summary << " (" << skippedFloat << " float widget(s) skipped: oltre il limite di " << numChannelParams << ")";
    if (skippedBool > 0)
        summary << " (" << skippedBool << " bool widget(s) skipped: oltre il limite di " << numBoolParams << ")";
    if (skippedChoice > 0)
        summary << " (" << skippedChoice << " choice widget(s) skipped: oltre il limite di " << numChoiceParams << ")";

    if (skippedListbox > 0)
        summary << " - " << skippedListbox << " listbox ignorato/i (nessun parametro DAW)";

    summary << " ---";
    handleMessage (summary);

    return true;
}

void CsoundAudioProcessor::restoreStateFromTree (const juce::ValueTree& state, const juce::String& csdTextToRestore)
{
    setCsdCodeFromFullText (csdTextToRestore);
    applyParamsStructureTree (state);
}

void CsoundAudioProcessor::applyParamsStructureTree (const juce::ValueTree& state)
{
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
                        // default 1.0/0.001 anche per sessioni salvate dalla
                        // vecchia versione a tendina curve (nessuna property
                        // skew/increment presente in quel caso).
                        slot.skew        = (float) (double) slotTree.getProperty ("skew", 1.0);
                        slot.increment   = (float) (double) slotTree.getProperty ("step", 0.001);
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

        // NIENTE ricompilazione qui (a differenza della versione precedente):
        // questa funzione serve sia a setStateInformation (che DEVE
        // ricompilare, vedi scheduleRecompileAfterRestore chiamata li') sia
        // a loadSessionFromText per Load CSD/"Usa versione su disco", dove e'
        // l'editor a chiamare Apply subito dopo - farlo anche qui
        // compilerebbe due volte lo stesso testo.
}

void CsoundAudioProcessor::scheduleRecompileAfterRestore (const juce::String& restoredCsd)
{
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
        juce::MessageManager::callAsync ([this, restoredCsd, alive = aliveFlag]
        {
            if (! alive->load())
                return;

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

    // Thread AUDIO (dentro csoundPerformKsmps): SOLO accodamento, niente
    // callAsync per messaggio (vedi il commento sul filtro nel .h).
    const juce::SpinLock::ScopedLockType sl (pendingMessagesLock);

    if (pendingMessages.size() < kMaxPendingMessages)
        pendingMessages.add (msg);
    else
        ++droppedMessages;
}

void CsoundAudioProcessor::timerCallback()
{
    flushPendingMessages();
}

void CsoundAudioProcessor::flushPendingMessages()
{
    juce::StringArray incoming;
    int dropped = 0;
    {
        const juce::SpinLock::ScopedLockType sl (pendingMessagesLock);
        incoming.swapWith (pendingMessages);
        dropped = droppedMessages;
        droppedMessages = 0;
    }

    const auto now = juce::Time::getMillisecondCounter();
    juce::StringArray out;

    auto shortened = [] (const juce::String& m)
    {
        return m.length() > 80 ? m.substring (0, 77) + "..." : m;
    };

    // Finestre scadute: riassunto delle ripetizioni soppresse, poi la
    // finestra si chiude (la prossima occorrenza ne apre una nuova e torna
    // a essere mostrata).
    for (auto it = repeatWindows.begin(); it != repeatWindows.end();)
    {
        if (now - it->second.startMs >= (juce::uint32) kRepeatWindowMs)
        {
            if (it->second.suppressed > 0)
                out.add ("--- previous message repeated " + juce::String (it->second.suppressed)
                         + " more time(s): " + shortened (it->first) + " ---");
            it = repeatWindows.erase (it);
        }
        else
        {
            ++it;
        }
    }

    int floodSuppressed = 0;

    for (auto& m : incoming)
    {
        auto& w = repeatWindows[m];

        if (w.shown == 0 && w.suppressed == 0)
            w.startMs = now;

        if (w.shown >= kMaxIdenticalMessagesPerWindow)
        {
            ++w.suppressed;
            continue;
        }

        if (out.size() >= kMaxLinesPerFlush)
        {
            ++floodSuppressed;   // flood di messaggi DIVERSI: non mostrati
            continue;
        }

        ++w.shown;
        out.add (m);
    }

    if (floodSuppressed > 0)
        out.add ("--- " + juce::String (floodSuppressed) + " message(s) not shown (console flood) ---");

    if (dropped > 0)
        out.add ("--- " + juce::String (dropped) + " message(s) lost (output too fast) ---");

    if (out.isEmpty())
        return;

    {
        const juce::ScopedLock sl (messageHistoryLock);
        messageHistory.addArray (out);

        if (messageHistory.size() > messageHistoryCapacity)
            messageHistory.removeRange (0, messageHistory.size() - messageHistoryCapacity);
    }

    listeners.call ([&out] (Listener& l) { l.csoundMessagesReceived (out); });
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

juce::String CsoundAudioProcessor::emptyCsdText()
{
    return R"CSD(<CsoundSynthesizer>
<CsOptions>
-m0 -Ma
</CsOptions>
<CsInstruments>

sr       = 44100
ksmps    = 32
nchnls   = 2
nchnls_i = 2
0dbfs    = 1

</CsInstruments>
<CsScore>
e
</CsScore>
</CsoundSynthesizer>
)CSD";
}

juce::String CsoundAudioProcessor::defaultCsdText()
{
    // nchnls_i dichiara i canali di INGRESSO (bus Input del plugin, es.
    // microfono con la Standalone, o sidechain in una DAW): letti nel
    // .csd con l'opcode "inch". nchnls resta per i canali di USCITA.
    return R"CSD(<CsoundSynthesizer>
<CsOptions>
-m0 -Ma
</CsOptions>
<CsInstruments>

; =========================================================
; HEADER - how apeCsound treats these values
; =========================================================
; sr      : overridden by the DAW sample rate (whatever is written here).
; ksmps   : respected as declared. It is also the plugin latency
;           (reported to the DAW, which compensates it): keep it small
;           (16-64) when processing live input.
; nchnls  : the value declared HERE is what Csound runs with (default,
;           Config > "Follow CSD nchnls" enabled): the code behaves the
;           same on any track. "outch n" goes to channel n of the track;
;           if the CSD has more channels than the track the extra ones are
;           silent, if the track has more they stay silent. Set the track
;           channel count in the DAW to hear them all (REAPER: track
;           routing > channels). A mismatch is reported in the console.
;           If nchnls is NOT declared, the track channel count is used
;           (minimum 2, because "outs" needs two).
;           Disabling "Follow CSD nchnls" makes Csound always follow the
;           track instead, whatever is written here.
; nchnls_i: same rule for the inputs read with "inch 1", "inch 2", ...
;           A channel beyond the track inputs reads silence.
; outs/outch/out: write to the plugin outputs 1, 2, ... up to nchnls.

sr       = 44100
ksmps    = 10
nchnls   = 2
nchnls_i = 2
0dbfs    = 1

; Assign MIDI all channels to the synth instrument
massign 0, 1

; =========================================================
; INSTRUMENT 1: MIDI SYNTH
; Generates a band limited oscillator, controlled by MIDI notes
; =========================================================

instr 1

    ; Get the MIDI note number
    inote notnum

    ; Convert MIDI note number to frequency in Hz
    icps = cpsmidinn(inote)

    ; Convert MIDI velocity to amplitude
    iamp = ampmidi(0.5)
    
    ;MENU: Options=Sawtooth,Square/PWM,Pulse; Default=Sawtooth
    iWave chnget "Wave"
    
    ; Attack time.
    iattack = 0.05
    ; Decay time.
    idecay = 0
    ; Sustain level.
    isustain = 1
    ; Release time.
    irelease = 0.5
    aenv madsr iattack, idecay, isustain, irelease
    asig vco aenv*iamp, icps, iWave, 0.5
    
    ; Send the synth audio to the plugin outputs 1 and 2
    ; (outch n works up to nchnls: on a 4-channel track you can also
    ; write outch 3, aSig / outch 4, aSig)
    outch 1, asig
    outch 2, asig

endin


; =========================================================
; INSTRUMENT 2: AUDIO INPUT EFFECT
; Reads stereo audio from the DAW track input (inch 1 / inch 2)
; and applies a feedback delay
; =========================================================

instr 2

    ; Read audio from the plugin inputs (nchnls_i channels available)
    aInL inch 1
    aInR inch 2

    ; Delay parameters
    idelay = 1
    
    ;SLIDER FLOAT: Min=0; Max=1; Skew=1; Step=0.001
    kdelay chnget "Delay"
    
    ;SLIDER FLOAT: Min=0; Max=0.98; Skew=1; Step=0.001
    kfb chnget "Feedback"

    ; Left channel delay line
    aDL delayr idelay
    aL  deltap3 kdelay
    delayw aInL + (aL * kfb)

    ; Right channel delay line
    aDR delayr idelay
    aR  deltap3 kdelay
    delayw aInR + (aR * kfb)

    ; Mix dry input with the delayed signal
    aOutL = aInL + (aL * 0.5)
    aOutR = aInR + (aR * 0.5)
    
    ; Send the processed audio to the plugin outputs 1 and 2
    ; (outch n works up to nchnls: on a 4-channel track you can also
    ; write outch 3, aSig / outch 4, aSig)
    outch 1, aOutL
    outch 2, aOutR

endin

</CsInstruments>
<CsScore>
; Keep the audio effect running continuously
i 2 0 z
e
</CsScore>
</CsoundSynthesizer>

<apeCsoundParams>
<?xml version="1.0" encoding="UTF-8"?>

<APE_CSOUND_STATE>
  <CHANNEL_PARAM_SLOTS>
    <SLOT index="0" channel="Delay" min="0.0" max="1.0" default="0.300000011920929"
          skew="1.0" step="0.001000000047497451"/>
    <SLOT index="1" channel="Feedback" min="0.0" max="0.9800000190734863"
          default="0.4000000059604645" skew="1.0" step="0.001000000047497451"/>
  </CHANNEL_PARAM_SLOTS>
  <INT_PARAM_SLOTS/>
  <BOOL_PARAM_SLOTS/>
  <CHOICE_PARAM_SLOTS>
    <SLOT index="0" channel="Wave" defaultIndex="0" options="Sawtooth&#10;Square/PWM&#10;Pulse"/>
  </CHOICE_PARAM_SLOTS>
</APE_CSOUND_STATE>

</apeCsoundParams>

)CSD";
}

//==============================================================================
// Punto d'ingresso richiesto dal plugin client di JUCE (Standalone/VST3/AU
// chiamano tutti questa funzione per ottenere l'istanza del processor).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CsoundAudioProcessor();
}
