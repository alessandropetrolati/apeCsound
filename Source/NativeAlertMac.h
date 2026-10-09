#pragma once

// Dialoghi di conferma DAVVERO nativi (NSAlert su macOS, UIAlertController
// su iOS) con pulsanti dal testo personalizzato - vedi NativeAlertMac.mm.
// ASINCRONI (callback con la scelta): su iOS un dialogo modale sincrono
// non esiste, e un'API asincrona unica vale per entrambe le piattaforme.
// Convenzione: il PRIMO bottone e' l'azione di default (Invio);
// risultato 1 = primo, 2 = secondo, 3 = terzo bottone (0 = chiuso senza
// scegliere, solo macOS).
//
// NESSUN header JUCE in NativeAlertMac.mm (conflitti di nomi fra
// juce_gui_basics e Cocoa/UIKit): il confine e' const char* + un puntatore
// a funzione C con contesto opaco; qui sopra ci mettiamo una std::function.

#include <juce_core/juce_core.h>
#include <functional>
#include <memory>

extern "C" void showNativeAlertRaw (const char* title, const char* message,
                                    const char* button1Text, const char* button2Text, const char* button3Text, // button3Text nullptr = due bottoni
                                    void (*callback) (int result, void* context), void* context);

namespace NativeAlertDetail
{
    struct Context { std::function<void (int)> onResult; };

    inline void trampoline (int result, void* context)
    {
        std::unique_ptr<Context> ctx (static_cast<Context*> (context));

        if (ctx != nullptr && ctx->onResult)
            ctx->onResult (result);
    }
}

inline void showNativeThreeButtonAlertAsync (const juce::String& title, const juce::String& message,
                                             const juce::String& button1Text, const juce::String& button2Text,
                                             const juce::String& button3Text, std::function<void (int)> onResult)
{
    auto* ctx = new NativeAlertDetail::Context { std::move (onResult) };
    showNativeAlertRaw (title.toRawUTF8(), message.toRawUTF8(),
                        button1Text.toRawUTF8(), button2Text.toRawUTF8(), button3Text.toRawUTF8(),
                        NativeAlertDetail::trampoline, ctx);
}

inline void showNativeTwoButtonAlertAsync (const juce::String& title, const juce::String& message,
                                           const juce::String& button1Text, const juce::String& button2Text,
                                           std::function<void (int)> onResult)
{
    auto* ctx = new NativeAlertDetail::Context { std::move (onResult) };
    showNativeAlertRaw (title.toRawUTF8(), message.toRawUTF8(),
                        button1Text.toRawUTF8(), button2Text.toRawUTF8(), nullptr,
                        NativeAlertDetail::trampoline, ctx);
}
