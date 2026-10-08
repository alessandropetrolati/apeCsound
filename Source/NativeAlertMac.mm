// NESSUN header JUCE qui (ne' <JuceHeader.h> ne' un modulo "nudo" come
// <juce_core/juce_core.h>) - vedi il commento in testa a NativeAlertMac.h
// sul perche': combinare "using namespace juce;"/juce_gui_basics con
// <Cocoa/Cocoa.h> nello stesso file genera centinaia di conflitti di nomi.
// Questo file conosce solo tipi Objective-C/Cocoa e const char* in ingresso
// e int in uscita (extern "C", nessun juce::String che attraverso questo
// confine).

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>

extern "C" int showNativeThreeButtonAlertRaw (const char* title,
                                               const char* message,
                                               const char* button1Text,
                                               const char* button2Text,
                                               const char* button3Text)
{
    NSAlert* alert = [[NSAlert alloc] init];

    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = [NSString stringWithUTF8String: title];
    alert.informativeText = [NSString stringWithUTF8String: message];

    // Il PRIMO bottone aggiunto e' quello che NSAlert posiziona piu' a
    // destra (l'azione di default, scattabile anche con Invio) - quindi
    // button1Text deve essere l'azione "consigliata" (qui: "Save").
    [alert addButtonWithTitle: [NSString stringWithUTF8String: button1Text]];
    [alert addButtonWithTitle: [NSString stringWithUTF8String: button2Text]];
    [alert addButtonWithTitle: [NSString stringWithUTF8String: button3Text]];

    const NSModalResponse response = [alert runModal];

    if (response == NSAlertFirstButtonReturn)
        return 1;

    if (response == NSAlertSecondButtonReturn)
        return 2;

    return 0; // terzo bottone, oppure finestra chiusa senza scegliere
}

extern "C" int showNativeTwoButtonAlertRaw (const char* title,
                                             const char* message,
                                             const char* button1Text,
                                             const char* button2Text)
{
    NSAlert* alert = [[NSAlert alloc] init];

    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = [NSString stringWithUTF8String: title];
    alert.informativeText = [NSString stringWithUTF8String: message];

    // Stesso ordine/convenzione di showNativeThreeButtonAlertRaw sopra: il
    // PRIMO bottone aggiunto e' quello di default (piu' a destra,
    // scattabile con Invio).
    [alert addButtonWithTitle: [NSString stringWithUTF8String: button1Text]];
    [alert addButtonWithTitle: [NSString stringWithUTF8String: button2Text]];

    const NSModalResponse response = [alert runModal];

    return response == NSAlertFirstButtonReturn ? 1 : 2;
}

#endif // defined(__APPLE__)
