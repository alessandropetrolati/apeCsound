#pragma once

// JuceHeader.h (serve per le impostazioni globali dei moduli). Attenzione
// in SecurityScopedFile.mm: Foundation va importato PRIMA di questo header,
// altrimenti il "using namespace juce" di JuceHeader rende ambiguo "Point"
// in MacTypes.h.
#include <JuceHeader.h>

/**
    Accesso ai file FUORI dalla sandbox su iOS (iCloud Drive, "Sul mio
    iPhone", provider di terze parti...). Il document picker di sistema
    restituisce URL "security-scoped": si possono leggere/scrivere solo tra
    startAccessingSecurityScopedResource e stopAccessing..., e per
    ritrovarli in un'altra sessione (riapertura del progetto DAW) serve un
    BOOKMARK (NSData) salvato insieme al path.

    JUCE aggancia gia' un bookmark alla juce::URL restituita dal FileChooser
    (vedi juce_FileChooser_ios.mm / setURLBookmark); qui lo si estrae come
    MemoryBlock (per salvarlo nello stato del progetto) e lo si usa con un
    RAII che apre e chiude l'accesso intorno alle operazioni su juce::File.

    Su macOS (e ovunque non sia iOS) tutto e' no-op: i path sono accessibili
    direttamente.
*/
namespace SecurityScopedFile
{
    /** Bookmark (NSData) agganciato da JUCE alla URL del FileChooser; vuoto
        se non c'e' (macOS, o file dentro la sandbox). */
    juce::MemoryBlock bookmarkFromChooserURL (juce::URL& url);

    /** juce::URL "file://" per il file, con il bookmark agganciato (come fa
        il FileChooser): juce::URL::createInputStream/createOutputStream
        aprono e chiudono da soli l'accesso security-scoped intorno alla
        lettura/scrittura (iOSFileStreamWrapper in juce_URL.cpp). E' il
        modo con cui NP2 scrive i file scelti dal picker. Con bookmark vuoto
        o non iOS: semplice URL del file. */
    juce::URL makeURLWithBookmark (const juce::File& file, const juce::MemoryBlock& bookmark);

    /** Crea un bookmark per un file gia' accessibile ADESSO (es. dentro un
        ScopedAccess, o dentro la sandbox). Vuoto se fallisce o non iOS. */
    juce::MemoryBlock makeBookmark (const juce::File& file);

    /** Apre l'accesso al file descritto dal bookmark per la durata
        dell'oggetto. resolvedFile e' il path attuale del file (puo' essere
        diverso da quello salvato se l'utente lo ha spostato); active dice se
        l'accesso e' stato concesso. Con bookmark vuoto: no-op, active=false,
        resolvedFile = fallback. */
    struct ScopedAccess
    {
        ScopedAccess (const juce::MemoryBlock& bookmark, const juce::File& fallback);
        ~ScopedAccess();

        bool active = false;
        bool stale = false;          // il bookmark andrebbe rinnovato (makeBookmark)
        juce::File resolvedFile;

    private:
        void* nsurl = nullptr; // NSURL* su iOS
        JUCE_DECLARE_NON_COPYABLE (ScopedAccess)
    };
}
