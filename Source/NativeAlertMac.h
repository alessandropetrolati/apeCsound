#pragma once

#include <juce_core/juce_core.h>

// Dialogo di conferma DAVVERO nativo macOS (NSAlert, implementato in
// NativeAlertMac.mm) con TRE pulsanti dal TESTO PERSONALIZZATO - richiesto
// esplicitamente al posto sia di juce::AlertWindow::showYesNoCancelBox
// (testo libero ma disegnato da JUCE, non nativo) sia di juce::
// NativeMessageBox::showYesNoCancelBox (nativo ma pulsanti fissi
// Yes/No/Cancel, non rietichettabili).
//
// Il confine tra i due "mondi" e' voluto: showNativeThreeButtonAlertRaw
// (extern "C", solo const char*) e' l'unica cosa che il file .mm conosce -
// quel file NON include alcun header JUCE, perche' <Cocoa/Cocoa.h> insieme
// a <JuceHeader.h> (che fa "using namespace juce;" e porta dentro anche
// juce_gui_basics) genera centinaia di conflitti di nomi fra i tipi JUCE e
// Cocoa/Carbon (es. Point, Rectangle, Component - tentativo gia' fatto e
// scartato: produceva ~200 errori di compilazione). showNativeThreeButtonAlert
// qui sotto e' il wrapper inline che converte da/verso juce::String,
// compilato nel contesto di CHI LO CHIAMA (un normale .cpp che ha gia'
// incluso <JuceHeader.h> prima, es. PluginEditor.cpp), non nel .mm.
extern "C" int showNativeThreeButtonAlertRaw (const char* title,
                                               const char* message,
                                               const char* button1Text,
                                               const char* button2Text,
                                               const char* button3Text);

// Ritorna 1 se l'utente preme button1Text (il primo aggiunto, quello messo
// da NSAlert piu' a destra/predefinito), 2 per button2Text, 0 per
// button3Text oppure se la finestra viene chiusa senza scegliere.
inline int showNativeThreeButtonAlert (const juce::String& title,
                                        const juce::String& message,
                                        const juce::String& button1Text,
                                        const juce::String& button2Text,
                                        const juce::String& button3Text)
{
    return showNativeThreeButtonAlertRaw (title.toRawUTF8(),
                                           message.toRawUTF8(),
                                           button1Text.toRawUTF8(),
                                           button2Text.toRawUTF8(),
                                           button3Text.toRawUTF8());
}

// Stessa idea di showNativeThreeButtonAlertRaw sopra ma con SOLO due
// pulsanti - per le normali conferme si'/no (es. "Remove Parameters?")
// dove un terzo pulsante (anche vuoto) non avrebbe senso. button1Text e'
// quello di default/predefinito (piu' a destra, scattabile con Invio):
// per un'azione distruttiva conviene che sia quello "sicuro" (es.
// "Cancel"), NON quello distruttivo. Ritorna 1 per button1Text, 2 per
// button2Text o se la finestra viene chiusa senza scegliere.
extern "C" int showNativeTwoButtonAlertRaw (const char* title,
                                             const char* message,
                                             const char* button1Text,
                                             const char* button2Text);

inline int showNativeTwoButtonAlert (const juce::String& title,
                                      const juce::String& message,
                                      const juce::String& button1Text,
                                      const juce::String& button2Text)
{
    return showNativeTwoButtonAlertRaw (title.toRawUTF8(),
                                         message.toRawUTF8(),
                                         button1Text.toRawUTF8(),
                                         button2Text.toRawUTF8());
}
