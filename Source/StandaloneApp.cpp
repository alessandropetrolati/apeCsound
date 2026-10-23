// Applicazione standalone di apeCsound (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1,
// vedi i defines del .jucer). E' la StandaloneFilterApp di JUCE
// (juce_audio_plugin_client_Standalone.cpp) con in piu' l'apertura dei .csd
// dal sistema:
//   - macOS: doppio clic su un .csd nel Finder, "Apri con", trascinamento
//     sull'icona del Dock (CFBundleDocumentTypes generato da documentExtensions
//     nel .jucer). JUCE consegna i file a anotherInstanceStarted().
//   - Windows / Linux: "Apri con" / associazione del tipo di file: il percorso
//     arriva come argomento della riga di comando (initialise()).
// Il file viene caricato come Load / drag & drop (con la richiesta di salvare
// le modifiche non salvate), vedi CsoundAudioProcessorEditor::openFileFromSystem.

#include <JuceHeader.h>

// Non su iOS: li' resta l'app standalone standard di JUCE (il define non e'
// impostato nell'exporter iOS).
#if JucePlugin_Build_Standalone && JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP && ! JUCE_IOS

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "PluginEditor.h"

namespace juce
{

class ApeCsoundStandaloneApp final : public JUCEApplication
{
public:
    ApeCsoundStandaloneApp()
    {
        PropertiesFile::Options options;

        options.applicationName     = CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return false; } // i .csd aperti vanno all'istanza gia' aperta

    void anotherInstanceStarted (const String& commandLine) override
    {
        openFilesFromCommandLine (commandLine);
    }

    StandaloneFilterWindow* createWindow()
    {
        if (Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new StandaloneFilterWindow (getApplicationName(),
                                           LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                                           createPluginHolder());
    }

    std::unique_ptr<StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
       #if (JUCE_ANDROID || JUCE_IOS) && ! JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
                true;
       #else
                false;
       #endif

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const Array<StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<StandalonePluginHolder> (appProperties.getUserSettings(),
                                                         false,
                                                         String{},
                                                         nullptr,
                                                         channelConfig,
                                                         autoOpenMidiDevices);
    }

    void initialise (const String& commandLine) override
    {
        mainWindow = rawToUniquePtr (createWindow());

        if (mainWindow != nullptr)
        {
           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif

            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }

        // Windows/Linux: file passato come argomento ("Apri con").
        // Differito: l'editor e lo stato salvato vengono ripristinati prima.
        if (commandLine.isNotEmpty())
        {
            const auto args = commandLine;
            MessageManager::callAsync ([this, args] { openFilesFromCommandLine (args); });
        }
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, []()
            {
                if (auto app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    // Primo .csd trovato tra gli argomenti (percorsi eventualmente tra
    // virgolette, come li passa JUCE/il sistema).
    void openFilesFromCommandLine (const String& commandLine)
    {
        StringArray args;
        args.addTokens (commandLine, " ", "\"");
        args.trim();
        args.removeEmptyStrings();

        for (auto arg : args)
        {
            arg = arg.unquoted();

            if (arg.startsWith ("file://"))
                arg = URL (arg).getLocalFile().getFullPathName();

            if (! File::isAbsolutePath (arg))
                continue;

            const File file (arg);

            if (file.existsAsFile() && file.hasFileExtension ("csd"))
            {
                openFile (file);
                return;
            }
        }
    }

    void openFile (const File& file)
    {
        auto* holder = StandalonePluginHolder::getInstance();
        auto* processor = holder != nullptr ? holder->processor.get() : nullptr;

        if (processor == nullptr)
            return;

        if (mainWindow != nullptr)
            mainWindow->toFront (true);

        if (auto* editor = dynamic_cast<CsoundAudioProcessorEditor*> (processor->getActiveEditor()))
            editor->openFileFromSystem (file);
    }

    ApplicationProperties appProperties;
    std::unique_ptr<StandaloneFilterWindow> mainWindow;
    std::unique_ptr<StandalonePluginHolder> pluginHolder;
};

} // namespace juce

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication()   { return new juce::ApeCsoundStandaloneApp(); }


#endif
