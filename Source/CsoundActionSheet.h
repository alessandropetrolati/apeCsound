#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

/**
    Icona opzionale disegnata a SINISTRA di una riga normale (richiesta
    esplicita: "inserisci le icone sulla sx di ciascuna voce dei due menu")
    - vedi CsoundActionSheetItem::icon sotto e makeSheetIconPath() nel .cpp
    per i Path disegnati a mano, stessa tecnica (stroke-to-fill via
    juce::PathStrokeType::createStrokedPath) usata per checkmark/chevron.
    none = nessuna icona, riga indentata comunque quanto le altre della
    stessa pagina (spazio riservato sempre, vedi RowsContent::paint).
*/
enum class CsoundActionSheetIcon
{
    none,
    undo, redo,
    save, load,
    sidebarPanel, console,
    sliderFloat, sliderInt, toggleSwitch, comboMenu,
    eyeOpen, eyeClosed,
    trash, resetDefault,
    newDocument,
    info,
    book,        // manuale online
    codeBraces,  // sintassi moderna "f(x)"
    cut, copy, paste, selectAll, indent, comment,
    gear,        // sottomenu Config
    textLarger, textSmaller, // "A" grande/piccola con +/- (font dell'editor)
    sun,         // "Light Editor": sole (palette chiara)
    folder       // "Reveal in Finder": cartella
};

/**
    Una voce del foglio (vedi CsoundActionSheet sotto). Costruita a mano
    invece di un juce::PopupMenu::Item: niente dipendenza da PopupMenu, che
    questo foglio sostituisce interamente.

    - id == 0: riga NON selezionabile (separatore o intestazione di
      sezione), vedi i due helper statici sotto.
    - subItems non vuoto: la riga apre una SECONDA pagina del foglio
      (drill-down con un bottone "Back" in testa), invece di chiudere il
      foglio con un risultato - usata da "Add parameter".
*/
struct CsoundActionSheetItem
{
    int id = 0;
    juce::String text;
    bool enabled = true;
    bool ticked = false;
    bool isSeparator = false;
    juce::String sectionHeader; // non vuoto = riga di intestazione, solo etichetta
    CsoundActionSheetIcon icon = CsoundActionSheetIcon::none;
    std::vector<CsoundActionSheetItem> subItems;

    // keepOpen: la voce esegue la sua azione (callback onSelected con il
    // suo id) ma il foglio RESTA aperto, per azioni ripetute o multiple
    // (es. Config: spunte e Larger/Smaller Text). Con tickedFn impostata la
    // spunta viene riletta a ogni ridisegno, cosi' riflette subito il nuovo
    // stato senza chiudere e riaprire il menu.
    bool keepOpen = false;
    std::function<bool()> tickedFn;
    std::function<bool()> enabledFn; // come tickedFn, per lo stato abilitato (es. Undo/Redo)

    bool isEnabledNow() const  { return enabledFn != nullptr ? enabledFn() : enabled; }
    bool isTickedNow() const   { return ticked || (tickedFn != nullptr && tickedFn()); }

    static CsoundActionSheetItem separator()
    {
        CsoundActionSheetItem item;
        item.isSeparator = true;
        return item;
    }

    static CsoundActionSheetItem section (juce::String header)
    {
        CsoundActionSheetItem item;
        item.sectionHeader = std::move (header);
        return item;
    }
};

/**
    Popup "custom" centrato - sostituisce OVUNQUE in questo plugin i popup
    menu nativi di JUCE (richiesta esplicita: "i popup menu vanno
    specializzati per funzionare con iOS, serve un approccio custom").

    juce::PopupMenu su iOS non si comporta come un vero menu desktop (niente
    hover, niente "tasto destro", target di tocco spesso troppo piccoli per
    un dito) - questo componente e' disegnato a mano pensando al tocco: un
    overlay scuro a schermo intero con un pannello CENTRATO di dimensione
    ragionevole (NON a schermo intero: una prima versione era un bottom
    sheet a tutta larghezza/altezza, giudicato "troppo grande" - richiesta
    esplicita), righe alte e spaziose, tap fuori dal pannello per annullare.
    Un sottomenu (CsoundActionSheetItem::subItems) apre una SECONDA pagina
    dentro lo stesso popup (con un bottone "Back"), non un popup annidato:
    su un touch screen i popup annidati sono scomodi da raggiungere/
    chiudere per errore.

    Gestisce la propria vita da solo: show() lo crea e lo mostra, l'istanza
    si rimuove e si autodistrugge alla chiusura (tap su una riga finale,
    sullo scrim, o "Indietro" dall'ultima pagina) - niente da tenere a mano
    nel chiamante, stesso spirito di juce::PopupMenu::showMenuAsync.
*/
class CsoundActionSheet final : public juce::Component,
                                 private juce::Timer
{
public:
    // Mostra il foglio SOPRA "host" (passare getTopLevelComponent() dal
    // chiamante, per coprire TUTTA la finestra del plugin e non solo il
    // componente che ha richiesto il foglio - un vero bottom sheet occupa
    // sempre l'intera larghezza/altezza della finestra). onSelected riceve
    // l'id della riga scelta, oppure 0 se il foglio e' stato chiuso senza
    // scegliere nulla (tap sullo scrim).
    /** Path (24x24 circa, da scalare con scaleToFit) dell'icona: le stesse
        icone dei menu, riusate da CsoundEditCallout. Vuoto per none. */
    static juce::Path getIconPath (CsoundActionSheetIcon icon);

    static void show (juce::Component& host, const juce::String& title,
                       std::vector<CsoundActionSheetItem> items,
                       std::function<void (int)> onSelected);

private:
    struct Page
    {
        juce::String title;
        std::vector<CsoundActionSheetItem> items;
    };

    struct LaidOutRow
    {
        enum class Kind { normal, separator, sectionHeader, back };

        juce::Rectangle<int> bounds; // relative a rowsContent (NON all'intero overlay)
        int itemIndex = -1;          // indice in pageStack.back().items - valido SOLO se kind == normal
        Kind kind = Kind::normal;
    };

    // Componente INTERNO che disegna e gestisce il tocco SOLO per la lista
    // di righe della pagina corrente, dentro un juce::Viewport (vedi
    // rowsViewport sotto) per poter scorrere se il contenuto supera lo
    // spazio disponibile (schermi piccoli, pagine con molte voci) - separato
    // dal foglio "esterno" (scrim/sfondo/maniglia/titolo) solo per questo.
    // Classe nidificata: l'accesso ai membri PRIVATI di CsoundActionSheet
    // tramite "owner" e' legittimo in C++ (una classe nidificata e' un
    // membro della classe che la contiene).
    class RowsContent final : public juce::Component
    {
    public:
        explicit RowsContent (CsoundActionSheet& ownerIn) : owner (ownerIn) {}

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseUp (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        CsoundActionSheet& owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RowsContent)
    };

    CsoundActionSheet (const juce::String& title, std::vector<CsoundActionSheetItem> items,
                        std::function<void (int)> onSelected);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

    // juce::Component chiama questo automaticamente sui FIGLI (questo
    // foglio e' un figlio di "host", vedi show()) ogni volta che il
    // genitore cambia dimensione - BUG corretto (richiesta esplicita: "non
    // si ridimensiona con il component padre"): prima le bounds erano
    // fissate una volta sola in show() e non piu' aggiornate, quindi
    // ridimensionando la finestra del plugin il foglio restava della
    // vecchia dimensione (centrato nel punto sbagliato, eventualmente
    // tagliato fuori). Richiama semplicemente setBounds() con le bounds
    // CORRENTI del genitore, il che fa scattare da solo resized() ->
    // recomputeLayout() con la nuova geometria.
    void parentSizeChanged() override;

    void recomputeLayout();
    void timerCallback() override;
    int findRowAt (juce::Point<int> positionInRowsContent) const;
    void setPressedRow (int rowIndex);
    void activateRow (int rowIndex);
    void goBack();
    void dismiss (int resultId);
    void animateIn();

    std::vector<Page> pageStack;
    std::vector<LaidOutRow> laidOutRows;
    std::function<void (int)> onSelected;

    juce::Viewport rowsViewport;
    RowsContent rowsContent { *this };

    // Geometria del popup, SEMPRE centrata nell'overlay (calcolata da
    // recomputeLayout in base al contenuto della pagina corrente) - usata
    // da paint() e dal tap-fuori-per-chiudere.
    juce::Rectangle<int> sheetArea;

    // Animazione d'ingresso: solo un fade (setAlpha SULL'INTERO componente,
    // scrim compreso - niente piu' uno slide dal basso, non ha senso per un
    // popup centrato) da 0 a 1, vedi animateIn()/timerCallback().
    float sheetAlpha = 0.0f;
    int pressedRowIndex = -1;
    bool pressStartedOnScrim = false;

    // Tutte ridotte ulteriormente (richiesta esplicita: "e' troppo
    // invasivo... rendilo piu' modesto") rispetto alla versione precedente
    // (gia' un popup centrato, non piu' un bottom sheet a tutta larghezza).
    static constexpr int rowHeight             = 44;
    static constexpr int sectionHeaderHeight   = 24;
    static constexpr int separatorHeight       = 11;
    static constexpr int topPadding            = 10; // spazio sopra il titolo (o sopra le righe se la pagina non ha titolo)
    static constexpr int titleAreaHeight       = 32;
    static constexpr float sheetCornerRadius   = 14.0f;
    static constexpr int rowHorizontalPadding  = 16;
    static constexpr int iconAreaWidth         = 26; // spazio riservato all'icona (vedi CsoundActionSheetItem::icon), SEMPRE, anche se none (allineamento uniforme tra righe)
    static constexpr int maxSheetHeightPercent = 92; // rispetto all'altezza dell'overlay (= host): il menu principale (11 voci + 4 separatori = 538 px) deve starci senza scrollbar in una finestra normale

    // Dimensioni del popup CENTRATO (richiesta esplicita: "ragionevolmente
    // ampio senza esagerare", non piu' un bottom sheet a tutta larghezza) -
    // preferredWidth clampato tra minWidth e (larghezza host - 2*outerMargin),
    // vedi recomputeLayout().
    static constexpr int popupPreferredWidth = 280;
    static constexpr int popupMinWidth       = 200;
    static constexpr int popupOuterMargin    = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundActionSheet)
};
