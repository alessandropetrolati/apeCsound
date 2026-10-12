// Implementazioni per le piattaforme NON Apple (Linux, Windows) delle
// funzioni che su macOS/iOS stanno nei file Objective-C++ (.mm), che i
// Makefile di Linux e i progetti Visual Studio non compilano:
//   - NativeAlertMac.mm      -> showNativeAlertRaw (dialoghi a 2/3 bottoni)
//   - SecurityScopedFile.mm  -> bookmark/accesso security-scoped (solo iOS:
//                               altrove i percorsi sono accessibili direttamente)
// IOSKeyboard.h ha gia' le sue versioni inline per le piattaforme non Apple.

#include <JuceHeader.h>

#if ! JUCE_MAC && ! JUCE_IOS

#include "NativeAlertMac.h"
#include "SecurityScopedFile.h"

//==============================================================================
// Dialogo con l'AlertWindow di JUCE. Risultato come la versione Apple:
// 1 = primo bottone, 2 = secondo, 3 = terzo.
extern "C" void showNativeAlertRaw (const char* title, const char* message,
                                    const char* button1Text, const char* button2Text, const char* button3Text,
                                    void (*callback) (int result, void* context), void* context,
                                    void* /*nativeView*/)
{
    const bool threeButtons = (button3Text != nullptr);

    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle (juce::String::fromUTF8 (title))
                       .withMessage (juce::String::fromUTF8 (message))
                       .withButton (juce::String::fromUTF8 (button1Text))
                       .withButton (juce::String::fromUTF8 (button2Text));

    if (threeButtons)
        options = options.withButton (juce::String::fromUTF8 (button3Text));

    // AlertWindow: 1 = primo bottone, 2 = secondo (con tre), 0 = ULTIMO.
    juce::AlertWindow::showAsync (options, [callback, context, threeButtons] (int r)
    {
        int result = 0;

        if (r == 1)               result = 1;
        else if (threeButtons)    result = (r == 2 ? 2 : 3);
        else                      result = 2;

        if (callback != nullptr)
            callback (result, context);
    });
}

//==============================================================================
namespace SecurityScopedFile
{
    juce::MemoryBlock bookmarkFromChooserURL (juce::URL&)                     { return {}; }
    juce::URL makeURLWithBookmark (const juce::File& file, const juce::MemoryBlock&) { return juce::URL (file); }
    juce::MemoryBlock makeBookmark (const juce::File&)                        { return {}; }

    ScopedAccess::ScopedAccess (const juce::MemoryBlock&, const juce::File& fallback)
        : resolvedFile (fallback)
    {
    }

    ScopedAccess::~ScopedAccess() = default;
}

#endif
