#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

/**
    Pannello "Audio/MIDI Settings" della versione standalone, uguale su
    macOS, iOS, Windows e Linux. Sostituisce l'AudioDeviceSelectorComponent
    di JUCE (piccolo, pensato per il mouse) con un elenco a righe grandi e
    pagine "drill-down", sul modello di SLSettings di Numa Player: niente UI
    di JUCE, ma la stessa logica letta e scritta direttamente
    sull'AudioDeviceManager dello StandalonePluginHolder.

      pagina principale         sotto-pagine (riga "<" per tornare indietro)
      ------------------         -------------------------------------------
      Audio Driver      >        elenco dei tipi (CoreAudio, ASIO, ...)
      Output Device     >        elenco dei dispositivi di uscita
      Input Device      >        elenco dei dispositivi di ingresso + None
      Output Channels   >        coppie stereo, selezione multipla
      Input Channels    >        coppie stereo, selezione multipla
      Sample Rate       >        elenco
      Buffer Size       >        elenco, con la latenza in ms
      Mute audio input  [o]      (solo se c'e' rischio di larsen)
      Control Panel...  >        (driver che ne hanno uno, es. ASIO)
      MIDI Inputs       >        dispositivi, selezione multipla
      Bluetooth MIDI... >        pannello di accoppiamento del sistema
      Reset Audio/MIDI           dispositivi predefiniti

    Le righe con una sola scelta possibile (es. "iOS Audio" come unico
    dispositivo) mostrano il valore senza freccia. Lo stato si rilegge da
    solo (ChangeListener + controllo ogni secondo, come SLSettings::
    updateState): dispositivi collegati/scollegati, MIDI Bluetooth appena
    accoppiato, sample rate cambiato dal sistema.
*/
class CsoundAudioSettings final : public juce::Component,
                                  private juce::ChangeListener,
                                  private juce::Timer
{
public:
    CsoundAudioSettings (juce::AudioDeviceManager& deviceManager,
                         int maxInputChannels, int maxOutputChannels,
                         juce::Value* muteInputValue);
    ~CsoundAudioSettings() override;

    /** Altezza necessaria per la larghezza data (la pagina corrente). */
    int getIdealHeight (int width) const;

    /** L'altezza del contenuto e' cambiata (pagina o elenco diversi). */
    std::function<void()> onLayoutChanged;

    /** Una impostazione e' stata cambiata dall'utente: chi possiede il
        device manager la salva (StandalonePluginHolder::saveAudioDeviceState). */
    std::function<void()> onSetupChanged;

    void resized() override;
    void paint (juce::Graphics& g) override;

    /** Riga stile "Impostazioni": titolo a sinistra, valore a destra e un
        accessorio (freccia, spunta, interruttore). Tutta la riga si tocca. */
    struct Row final : public juce::Button
    {
        enum class Accessory { none, chevron, check, toggleSwitch, back };

        Row (const juce::String& titleText, const juce::String& valueText, Accessory accessoryType);
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

        juce::String title, value;
        Accessory accessory;
        bool ticked = false;
    };

private:
    enum class Page { main, driver, outputDevice, inputDevice, outputChannels, inputChannels,
                      sampleRate, bufferSize, midiInputs };

    struct Entry
    {
        bool isHeader = false;
        juce::String title, value;
        Row::Accessory accessory = Row::Accessory::none;
        bool ticked = false;
        bool enabled = true;
        std::function<void()> action;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void showPage (Page newPage);
    void rebuild();
    std::vector<Entry> buildEntries() const;
    std::vector<Entry> buildMainPage() const;
    juce::String stateSignature() const;

    // Azioni (come i callback di SLSettings)
    void selectDriver (const juce::String& typeName);
    void selectOutputDevice (const juce::String& name);
    void selectInputDevice (const juce::String& name);
    void toggleChannelPair (bool isInput, int pairIndex);
    void selectSampleRate (double rate);
    void selectBufferSize (int samples);
    void toggleMidiInput (const juce::String& identifier);
    void openBluetoothPairing();
    void resetAudioMidi();
    void setupChanged();

    juce::String channelSummary (bool isInput) const;
    juce::StringArray channelPairNames (bool isInput) const;
    static juce::String formatRate (double rate);

    int rowHeight() const noexcept;

    juce::AudioDeviceManager& deviceManager;
    const int maxInputs, maxOutputs;
    juce::Value* muteInput = nullptr;

    Page page = Page::main;
    juce::String lastSignature;

    std::vector<Entry> entries;
    std::vector<std::unique_ptr<Row>> rows;   // una per Entry non-header (nullptr per gli header)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundAudioSettings)
};
