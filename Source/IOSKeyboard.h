#pragma once

#include <JuceHeader.h>

/**
    Stato della tastiera a schermo di iOS (visibile / nascosta), osservato
    tramite le notifiche UIKeyboardDidShow/DidHide (arrivano anche dentro
    l'estensione AUv3). Serve a CodeView per capire se, al tap su un editor
    che ha gia' il focus, la tastiera va richiesta di nuovo (l'utente l'ha
    chiusa col tasto "nascondi") o se e' gia' aperta e non va toccata.

    Su tutte le altre piattaforme isVisible() ritorna sempre true (nessuna
    tastiera a schermo da gestire): definito in IOSKeyboard.mm per Apple
    (macOS incluso), inline qui per Windows/Linux dove i .mm non vengono
    compilati.
*/
namespace IOSKeyboard
{
   #if defined (__APPLE__)
    /** Installa gli osservatori delle notifiche. Va chiamata PRIMA che la
        tastiera possa comparire (es. nel costruttore dell'editor): gli
        osservatori venivano creati pigramente alla prima isVisible(), e se
        la tastiera era gia' aperta la notifica DidShow era gia' passata ->
        risultava "chiusa" fino alla successiva apertura. */
    void initialise();
    bool isVisible();

    /** Frame attuale della tastiera in coordinate dello schermo (punti,
        le stesse di Component::getScreenBounds su iOS); vuoto se nascosta
        o flottante (iPad: non copre nulla in modo prevedibile). */
    juce::Rectangle<int> getFrameOnScreen();

    /** Emette un ChangeMessage (asincrono, sul message thread) a ogni
        cambio di frame/visibilita' della tastiera: PluginEditor lo usa per
        ridurre il layout allo spazio NON coperto dalla tastiera. */
    juce::ChangeBroadcaster& getBroadcaster();

    /** True se negli appunti c'e' del testo, SENZA leggerli: su iOS 16+
        leggere gli appunti (SystemClipboard::getTextFromClipboard) fa
        comparire l'avviso di sistema "apeCsound vorrebbe incollare da...",
        mentre UIPasteboard.hasStrings no. Da usare per abilitare/disabilitare
        la voce "Paste" dei menu; la lettura vera avviene solo quando
        l'utente sceglie Paste. */
    bool clipboardHasText();
   #else
    inline void initialise() {}
    inline bool isVisible() { return true; }
    inline juce::Rectangle<int> getFrameOnScreen() { return {}; }
    inline juce::ChangeBroadcaster& getBroadcaster() { static juce::ChangeBroadcaster b; return b; }
    inline bool clipboardHasText() { return juce::SystemClipboard::getTextFromClipboard().isNotEmpty(); }
   #endif
}
