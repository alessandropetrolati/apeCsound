#pragma once

#include <JuceHeader.h>
#include <array>
#include <utility>
#include <vector>
#include "PluginProcessor.h"

/**
    LookAndFeel dedicata al pannello parametri (CsoundParameterMappingPanel):
    tema scuro petrolio/teal con angoli a 90 gradi ovunque (nessun bordo
    arrotondato), "glow" accentato sui campi in focus/hover e un menu a
    tendina coerente - sostituisce i soli
    setColour() per-componente della versione precedente con un vero
    ridisegno. Applicata con setLookAndFeel() SOLO sul pannello (si propaga
    ai figli finche' non trovano un'altra LookAndFeel esplicita): il resto
    del plugin resta sul tema chiaro di CsoundLookAndFeel, invariato.
*/
class CsoundParameterPanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CsoundParameterPanelLookAndFeel();

    // Angoli a 90 gradi anche sui bottoni Edit/+ (altrimenti LookAndFeel_V4
    // arrotonderebbe automaticamente i bordi) - coerente con lo stile
    // squadrato richiesto per tutti i widget del plugin. Il bottone "+" fa
    // eccezione (proprieta' dinamica "circular", vedi il .cpp) ed e'
    // disegnato circolare con un'icona "+" dedicata (vedi drawButtonText).
    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;
    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                        int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    juce::Font getComboBoxFont (juce::ComboBox& box) override;

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                     int& idealWidth, int& idealHeight) override;
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                             bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                             const juce::String& text, const juce::String& shortcutKeyText,
                             const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                         bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                         bool isMouseOver, bool isMouseDown) override;

    // Slider orizzontale della card UI (GenericParamRow, Kind::slider):
    // track SOTTILE a tutta larghezza + maniglia (thumb) MOLTO grande,
    // quasi quanto l'intera altezza del componente - richiesta esplicita
    // ("la maniglia dev'essere molto piu' grande, non l'altezza totale dello
    // slider"): invece di ingrandire la riga (che userebbe piu' spazio
    // verticale), si ingrandisce SOLO la maniglia rispetto a un track
    // sottile, nello stesso ingombro compatto di sempre.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float minSliderPos, float maxSliderPos,
                            const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // Disegna un "pill" squadrato (sfondo come i campi, pallino pieno +
    // testo ON/OFF) SOLO per i ToggleButton riconosciuti dal nome
    // ("pillToggle", vedi GenericParamRow) - il bottone On/Off della card
    // Edit resta sulla checkbox standard di LookAndFeel_V4.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Disegna icona + testo SOLO per il bottone Edit (riconosciuto da
    // Component::setName("editToggle")); disegna un'icona "+" dedicata
    // (nessun testo) per il bottone circolare Add (riconosciuto dalla
    // proprieta' dinamica "circular") - per tutti gli altri bottoni delega
    // al comportamento standard di LookAndFeel_V4.
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

/**
    Sidebar ancorata a destra (non una finestra flottante/overlay) per
    definire, per ciascuno dei 64 parametri host float (CsoundAudioProcessor::
    ChannelParamSlot), dei 32 interi (IntParamSlot), dei 32 booleani
    (BoolParamSlot) e dei 16 a scelta multipla (ChoiceParamSlot), il canale
    Csound a cui sono assegnati ("rename" - il parametro apvts resta sempre
    "Float N"/"Int N"/"Bool N"/"Choice N" per l'host) e i relativi metadata.

    Una SOLA lista (niente tab Float/Int/Bool/Choice/UI), che mostra SOLO gli
    slot gia' ALLOCATI (channelName non vuoto) - i 64+32+32+16 slot sono un
    pool da cui si "consuma" tramite il bottone "+"/Add (vedi showAddMenu()/
    addNewParameter()/createPendingRow()).

    Il flusso di Add (richiesto esplicitamente): "+" apre un menu Slider/Knob/
    Toggle/Menu (= Float/Int/Bool/Choice internamente); scelto il tipo, il
    primo slot libero di quel tipo NON viene allocato subito - compare una
    card "in sospeso" (pendingRow) con il campo nome VUOTO (nessun nome
    placeholder) e il focus gia' li', pronta a scrivere il chnget. Da quel
    momento il campo si comporta come una riga normale (commit ad ogni
    carattere, vedi ParamRow::commitFromFields):
      - Invio (o click altrove, che perde il focus) con il campo ANCORA
        VUOTO -> la card sospesa scompare, lo slot resta libero, come se
        "+" non fosse mai stato premuto (vedi ParamRow::onRemoveRequested,
        qui agganciato a uno scarto invece che a una rimozione normale);
      - Invio (o perdita del focus) con un nome scritto -> lo slot e'
        gia' stato scritto nel processor (succede ad ogni carattere, non
        solo alla fine), la card sospesa viene sostituita dalla riga VERA
        della lista unificata (vedi ParamRow::onCommittedNonEmpty) - stesso
        slot, ma ora persistente/ricostruibile come ogni altra riga.

    Un bottone "Edit" (sempre visibile, vedi editToggleButton) commuta OGNI
    riga della lista tra due visualizzazioni, SENZA cambiare quali righe sono
    presenti:
      - modalita' UI (default): una card (vedi uiCardHeight) con etichetta
        tipo + nome canale in testa e il controllo VERO (slider/toggle/combo,
        agganciato al parametro apvts reale tramite GenericParamRow) a tutta
        larghezza sotto - stessa maniglia colorata e sfondo delle card Edit,
        ma senza campi editabili;
      - modalita' Edit: una "card" piu' alta (vedi cardHeightSingleRow/
        cardHeightDoubleRow) con, dall'alto in basso: etichetta tipo colorata
        + bottone di rimozione ("SLIDER · CANALE" ecc. - vedi
        ParamRow/IntParamRow/BoolParamRow/ChoiceParamRow), il campo nome
        canale a tutta larghezza in grande, poi una riga di campi con
        etichetta sopra ciascuno (MIN/MAX/INIT/EXP/STEP per Float, MIN/MAX/
        INIT per Int, INIT+checkbox per Bool, DEFAULT per Choice) e, solo
        per Choice, una seconda riga larga quanto la card per le opzioni
        (OPZIONI, separate da virgola). La maniglia di trascinamento e' una
        barra verticale colorata (diversa per tipo) sul lato sinistro della
        card - vedi layoutCardSkeleton()/paintCardChrome().

    Ogni riga (in ENTRAMBE le modalita' - anche quella UI, vedi
    GenericParamRow::mouseDrag, ha la sua maniglia) e' anche una sorgente di
    drag and drop (vedi ParamRow::mouseDrag/ecc.): trascinando l'area della
    maniglia sull'editor di codice (CsoundCodeEditor, che implementa
    DragAndDropTarget) e rilasciando su una riga, viene inserito
    automaticamente un chnget per quel canale, seguito da un commento con i
    parametri di configurazione dello slot (Min/Max/Skew/Step per Float,
    Min/Max per Int, Init per Bool, Options/Default per Choice - vedi
    makeFloatConfigComment() ecc. nel .cpp) - vedi il commento in testa a
    CsoundCodeEditor.h per il formato esatto.
*/
class CsoundParameterMappingPanel final : public juce::Component
{
public:
    explicit CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit);
    ~CsoundParameterMappingPanel() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    // Ricostruisce l'intera lista unificata dallo stato attuale del
    // processor - chiamata da PluginEditor dopo un Load Session da file
    // (vedi CsoundAudioProcessor::loadSessionFromFile), cosi' il pannello
    // mostra subito il contenuto appena caricato invece dei vecchi valori
    // della sessione precedente.
    void refreshAllFromProcessor();

    // Chiamata quando il tasto destro su una maniglia copia un chnget negli
    // appunti, quando un parametro viene creato/rimosso dal flusso "+", o
    // quando il pool di uno dei quattro tipi e' esaurito (nessuno slot
    // libero) - PluginEditor la usa per scrivere una riga in consolle (vedi
    // appendToLog), cosi' ogni azione altrimenti invisibile lascia una
    // traccia verificabile.
    std::function<void (const juce::String&)> onParameterCopiedToClipboard;

    // Geometria della "card" Edit - vedi layoutCardSkeleton()/
    // paintCardChrome() in CsoundParameterEditor.cpp. handleStripWidth e'
    // sia la barra colorata (maniglia) sia lo spazio riservato ad essa a
    // sinistra del contenuto.
    static constexpr int handleStripWidth = 34;
    static constexpr int cardPaddingH = 14;
    static constexpr int cardPaddingV = 12;
    static constexpr int cardHeaderHeight = 20;
    static constexpr int cardHeaderGap = 6;
    static constexpr int cardNameHeight = 30;
    static constexpr int cardNameGap = 10;
    static constexpr int cardFieldCaptionHeight = 13;
    static constexpr int cardFieldCaptionGap = 3;
    static constexpr int cardFieldBoxHeight = 30;
    static constexpr int cardFieldRowGap = 10;
    static constexpr int cardGap = 8; // spazio verticale TRA le card, nella lista

    // Larghezze dei singoli campi numerici nella riga sotto il nome -
    // diverse per tipo, come nel mockup (lo "STEP" del Float e' piu' largo
    // degli altri, deve contenere valori come "0.001").
    static constexpr int cardFieldNarrowWidth = 68;
    static constexpr int cardFieldStepWidth = 92;
    static constexpr int cardFieldGap = 14;
    static constexpr int cardBoolFieldWidth = 120;
    static constexpr int cardChoiceDefaultWidth = 160; // ComboBox: deve mostrare l'etichetta scelta, non solo un indice

    // Altezza di una card Edit con "numFieldRows" righe di campi (etichetta
    // sopra + campo sotto, ripetute verticalmente con cardFieldRowGap tra
    // una riga e l'altra) - usata sia per i tipi a righe FISSE (Bool: 1,
    // Choice: 2) sia, dinamicamente, per Slider/Knob quando i campi non
    // entrano tutti su una riga sola e vanno a capo (vedi
    // computeWrappedFieldRows()/ParamRow::resized()).
    static constexpr int cardHeightForFieldRows (int numFieldRows)
    {
        return cardPaddingV * 2 + cardHeaderHeight + cardHeaderGap + cardNameHeight + cardNameGap
               + numFieldRows * (cardFieldCaptionHeight + cardFieldCaptionGap + cardFieldBoxHeight)
               + (numFieldRows - 1) * cardFieldRowGap;
    }

    // NON scritte come cardHeightForFieldRows(1)/(2): l'inizializzatore di
    // un dato membro static NON e' un "complete-class context" (a
    // differenza del CORPO di cardHeightForFieldRows() sopra, che infatti
    // puo' essere chiamato liberamente da dentro altri corpi di funzione,
    // es. FloatUnifiedRow::getPreferredHeight() piu' sotto) - su alcuni
    // compiler chiamarla qui fallisce. Stessa formula, scritta per esteso.
    static constexpr int cardHeightSingleRow = cardPaddingV * 2 + cardHeaderHeight + cardHeaderGap
                                                + cardNameHeight + cardNameGap
                                                + 1 * (cardFieldCaptionHeight + cardFieldCaptionGap + cardFieldBoxHeight)
                                                + 0 * cardFieldRowGap;
    static constexpr int cardHeightDoubleRow = cardPaddingV * 2 + cardHeaderHeight + cardHeaderGap
                                                + cardNameHeight + cardNameGap
                                                + 2 * (cardFieldCaptionHeight + cardFieldCaptionGap + cardFieldBoxHeight)
                                                + 1 * cardFieldRowGap;

    // Geometria della card "UI" (modalita' NON Edit, vedi mockup): maniglia
    // colorata + sfondo delle card Edit, MA una propria skeleton piu'
    // compatta (layoutUiCardSkeleton(), non layoutCardSkeleton()) - niente
    // bottone di rimozione, etichetta tipo e nome canale (sola lettura) piu'
    // bassi di quelli Edit, nessuna didascalia sopra il controllo. Economia
    // verticale massima (richiesta esplicita, l'altezza Edit-like era
    // "troppo alta, spreca spazio"): uiCardControlHeight e' l'UNICA riga
    // sotto il nome, usata per intero da slider/toggle/combo/box VALUE -
    // niente spazi di allineamento aggiuntivi. La maniglia (thumb) dello
    // slider resta pero' grande relativamente a questa riga sottile (vedi
    // CsoundParameterPanelLookAndFeel::drawLinearSlider nel .cpp) - e'
    // quello, non l'altezza della riga, a dover risultare grande.
    static constexpr int uiCardPaddingV = 6;
    static constexpr int uiCardHeaderHeight = 14;   // solo etichetta tipo, niente bottone rimozione
    static constexpr int uiCardHeaderGap = 3;
    static constexpr int uiCardNameHeight = 18;     // nome canale, sola lettura - piu' basso del campo Edit
    static constexpr int uiCardNameGap = 3;
    static constexpr int uiCardControlHeight = 28;  // slider/toggle/combo/box VALUE - un'unica riga, niente didascalia sopra

    static constexpr int uiCardHeight = uiCardPaddingV * 2 + uiCardHeaderHeight + uiCardHeaderGap
                                         + uiCardNameHeight + uiCardNameGap + uiCardControlHeight;

    static constexpr int uiValueGap = 10;        // spazio orizzontale tra lo slider e il box VALUE
    static constexpr int uiValueBoxWidth = 80;   // editabile - "0.00001" ci sta comodo
    static constexpr int uiRangeLabelHeight = 10; // "0"/"127" sotto lo slider - piccola, non e' una didascalia di campo
    static constexpr int uiSliderTrackGap = 2;    // tra lo slider e le etichette min/max sotto
    static constexpr int uiTogglePillWidth = 100;

    // Larghezza "di comodo" iniziale della sidebar (vedi PluginEditor.h) -
    // la riga di campi piu' larga (Slider, con 5 campi fissi) SU UNA SOLA
    // riga. Non e' piu' un minimo forzato per il contenuto: se il pannello
    // e' piu' stretto di cosi', le righe di campi vanno semplicemente a
    // capo (vedi computeWrappedFieldRows()) invece di mostrare una
    // scrollbar orizzontale.
    static constexpr int minContentWidth = handleStripWidth + cardPaddingH * 2
                                            + cardFieldNarrowWidth * 4 + cardFieldStepWidth + cardFieldGap * 4;

private:
    // Larghezza disponibile per i campi di una card (dopo maniglia e
    // padding) dato lo spazio totale della card - stessa identica
    // sottrazione che fa layoutCardSkeleton() per fieldsArea, cosi' il
    // conteggio righe qui e il posizionamento reale in resized() restano
    // sempre d'accordo.
    static int fieldsAvailableWidth (int fullCardWidth);

    // Conta quante righe servono per sistemare "fieldWidths" (larghezze, in
    // ordine) dentro "availableWidth", andando a capo (stesso algoritmo
    // "greedy" left-to-right di un flex-wrap CSS) invece di schiacciare le
    // colonne o mostrare una scrollbar orizzontale - usata sia da
    // FloatUnifiedRow/IntUnifiedRow::getPreferredHeight() sia da
    // ParamRow::resized()/IntParamRow::resized(), che DEVONO restare
    // d'accordo su quante righe risultano per la stessa larghezza.
    static int computeWrappedFieldRows (int availableWidth, std::initializer_list<int> fieldWidths, int gap);

    // Posiziona "fieldWidths" (in ordine, stessa lista data a
    // computeWrappedFieldRows()) dentro "fieldsArea", andando a capo con lo
    // STESSO algoritmo - chiama onPlaceField(index, cellBounds) per ciascun
    // campo, cosi' il chiamante puo' smistare la cella alla coppia
    // etichetta+editor giusta (vedi ParamRow::resized()).
    static void layoutWrappedFields (juce::Rectangle<int> fieldsArea, std::initializer_list<int> fieldWidths, int gap,
                                      const std::function<void (int index, juce::Rectangle<int> cell)>& onPlaceField);

    // Scheletro comune a tutte le card Edit: ritaglia da "full" la striscia
    // della maniglia (handleStrip, piena altezza), poi - dal resto, dopo il
    // padding - l'area di intestazione (typeLabel + bottone rimozione), il
    // campo nome (tutta la larghezza), e cio' che resta per i campi
    // specifici del tipo (fieldsArea, da suddividere nel chiamante).
    static void layoutCardSkeleton (juce::Rectangle<int> full,
                                     juce::Rectangle<int>& handleStrip,
                                     juce::Rectangle<int>& typeLabelArea,
                                     juce::Rectangle<int>& removeArea,
                                     juce::Rectangle<int>& nameArea,
                                     juce::Rectangle<int>& fieldsArea);

    // Stesso principio di layoutCardSkeleton() sopra ma con la geometria
    // COMPATTA delle card UI (uiCardPaddingV/uiCardHeaderHeight/ecc., vedi
    // sopra) - niente bottone di rimozione (GenericParamRow non ne ha uno),
    // usata SOLO da GenericParamRow::resized().
    static void layoutUiCardSkeleton (juce::Rectangle<int> full,
                                       juce::Rectangle<int>& handleStrip,
                                       juce::Rectangle<int>& typeLabelArea,
                                       juce::Rectangle<int>& nameArea,
                                       juce::Rectangle<int>& fieldsArea);

    // Posiziona una coppia (etichetta sopra, campo sotto) dentro "cell" -
    // usata per ogni campo di ogni tipo (Min/Max/Init/Exp/Step/Default...).
    static void layoutCaptionedField (juce::Rectangle<int> cell, juce::Label& caption, juce::Component& field);

    // Sfondo della card (leggermente piu' chiaro del pannello) + barra
    // colorata della maniglia (con i puntini, se la riga ha gia' un nome) -
    // condiviso dai paint() di tutti e 4 i tipi.
    static void paintCardChrome (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour accent,
                                  juce::Rectangle<int> handleStrip, bool showHandleDots, bool handleHovered);

    // Una riga EDIT per slot float ("SLIDER" nel mockup): barra colorata
    // (blu) a sinistra come maniglia di trascinamento, etichetta tipo +
    // bottone di rimozione in alto, nome canale (TextEditor, grande) sotto,
    // poi min/max/init/exp/step su una riga di campi con etichetta sopra
    // ciascuno. Ogni modifica (Return o focus perso) rilegge subito lo slot
    // corrente dal processor, applica il singolo campo cambiato e lo
    // riscrive.
    struct ParamRow final : public juce::Component,
                             private juce::TextEditor::Listener
    {
        ParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex);

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent& event) override;

        // Pubblico: chiamato anche dopo un Load Session da file, per
        // rileggere lo slot appena ripristinato invece di restare con i
        // vecchi valori mostrati prima del caricamento.
        void refreshFromProcessor();

        // Chiamata dal tasto destro sulla maniglia dopo aver copiato negli
        // appunti - usata SOLO per dare un riscontro visibile in consolle.
        std::function<void (const juce::String&)> onCopiedToClipboard;

        // Chiamata quando lo slot diventa vuoto - sia per il bottone
        // removeButton sia perche' l'utente ha cancellato/non ha mai
        // scritto il nome e poi e' uscito dal campo (Return/focus perso,
        // non ad ogni carattere digitato: svuotare la riga a META' della
        // digitazione non deve farla sparire sotto le dita dell'utente).
        // Per una riga della lista unificata, il pannello la usa (con
        // juce::MessageManager::callAsync, MAI in modo sincrono dentro
        // questo stesso callback - altrimenti si distruggerebbe questa
        // riga mentre e' ancora nello stack di chiamata che l'ha generata)
        // per togliere la riga dalla lista; per una pendingRow (vedi
        // createPendingRow()) la usa per scartarla senza mai aver allocato
        // nulla.
        std::function<void()> onRemoveRequested;

        // Chiamata (stesso identico vincolo di asincronia di
        // onRemoveRequested sopra) quando il campo nome viene confermato
        // (Return/focus perso) con un testo NON vuoto - usata SOLO da
        // createPendingRow() per sostituire la riga sospesa con quella vera
        // della lista unificata non appena l'utente ha finito di scrivere
        // il nome. Per una riga GIA' nella lista unificata non e' agganciata
        // a nulla (nessun comportamento in piu').
        std::function<void()> onCommittedNonEmpty;

        // Porta subito il focus da tastiera sul campo nome canale e ne
        // seleziona il contenuto - usata da createPendingRow().
        void focusChannelNameField();

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);
        void notifyRemovedIfEmpty();
        void notifyCommittedIfNonEmpty();

        CsoundAudioProcessor& processor;
        int index;

        // Striscia della maniglia (barra colorata a tutta altezza, lato
        // sinistro) - mouseDown/mouseDrag intercettano il trascinamento
        // SOLO li'. handleHovered (mouseMove/mouseExit) e' solo feedback
        // visivo (puntini piu' chiari).
        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;

        // Bottone "x" di rimozione, in alto a destra della card - visibile
        // solo quando il canale ha gia' un nome, stessa condizione della
        // maniglia stessa (vedi paint()/resized()).
        juce::TextButton removeButton { "x" };

        juce::Label typeLabel;
        juce::TextEditor channelNameEditor;

        juce::Label minCaption, maxCaption, initCaption, expCaption, stepCaption;
        juce::TextEditor minEditor;
        juce::TextEditor maxEditor;
        juce::TextEditor defaultEditor;   // "Init" nel mockup
        juce::TextEditor skewEditor;      // "Exp" nel mockup
        juce::TextEditor incrementEditor; // "Step" nel mockup

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamRow)
    };

    // Una riga EDIT per slot intero ("KNOB" nel mockup) - stesso schema di
    // ParamRow ma senza exp/step (gli interi usano sempre una mappatura
    // lineare), barra maniglia viola.
    struct IntParamRow final : public juce::Component,
                                private juce::TextEditor::Listener
    {
        IntParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex);

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent& event) override;

        void refreshFromProcessor();

        std::function<void (const juce::String&)> onCopiedToClipboard;
        std::function<void()> onRemoveRequested;
        std::function<void()> onCommittedNonEmpty;

        void focusChannelNameField();

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);
        void notifyRemovedIfEmpty();
        void notifyCommittedIfNonEmpty();

        CsoundAudioProcessor& processor;
        int index;

        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;
        juce::TextButton removeButton { "x" };

        juce::Label typeLabel;
        juce::TextEditor channelNameEditor;

        juce::Label minCaption, maxCaption, initCaption;
        juce::TextEditor minEditor;
        juce::TextEditor maxEditor;
        juce::TextEditor defaultEditor;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntParamRow)
    };

    // Una riga EDIT per slot booleano ("TOGGLE" nel mockup) - barra
    // maniglia verde/teal, un solo campo (checkbox "On" con etichetta
    // "INIT" sopra).
    struct BoolParamRow final : public juce::Component,
                                 private juce::TextEditor::Listener
    {
        BoolParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex);

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent& event) override;

        void refreshFromProcessor();

        std::function<void (const juce::String&)> onCopiedToClipboard;
        std::function<void()> onRemoveRequested;
        std::function<void()> onCommittedNonEmpty;

        void focusChannelNameField();

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);
        void notifyRemovedIfEmpty();
        void notifyCommittedIfNonEmpty();

        CsoundAudioProcessor& processor;
        int index;

        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;
        juce::TextButton removeButton { "x" };

        juce::Label typeLabel;
        juce::TextEditor channelNameEditor;

        juce::Label initCaption;
        juce::ToggleButton defaultToggle { "On" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoolParamRow)
    };

    // Una riga EDIT per slot a scelta multipla ("MENU" nel mockup) - barra
    // maniglia arancione, una riga intera (OPTIONS) per il testo libero
    // separato da virgole (fino a maxChoiceOptions), poi in fondo un
    // ComboBox (DEFAULT) che mostra le etichette vere inserite in OPTIONS -
    // ripopolato ad ogni modifica del testo (vedi refreshDefaultOptions()),
    // cosi' si sceglie il default per NOME invece che per indice a mente.
    struct ChoiceParamRow final : public juce::Component,
                                   private juce::TextEditor::Listener
    {
        ChoiceParamRow (CsoundAudioProcessor& processorToEdit, int slotIndex);

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent& event) override;

        void refreshFromProcessor();

        std::function<void (const juce::String&)> onCopiedToClipboard;
        std::function<void()> onRemoveRequested;
        std::function<void()> onCommittedNonEmpty;

        void focusChannelNameField();

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);
        void notifyRemovedIfEmpty();
        void notifyCommittedIfNonEmpty();

        // Ricostruisce le voci di defaultIndexCombo dal testo CORRENTE di
        // optionsEditor (chiamata ad ogni carattere digitato, da
        // commitFromFields()) - preserva la selezione corrente se l'indice
        // e' ancora valido nella nuova lista, altrimenti la riporta alla
        // prima voce disponibile.
        void refreshDefaultOptions();

        CsoundAudioProcessor& processor;
        int index;

        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;
        juce::TextButton removeButton { "x" };

        juce::Label typeLabel;
        juce::TextEditor channelNameEditor;

        juce::Label optionsCaption;
        juce::TextEditor optionsEditor;      // etichette separate da virgole

        juce::Label defaultCaption;
        juce::ComboBox defaultIndexCombo;    // popolato dalle etichette di optionsEditor

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceParamRow)
    };

    // Riga UI (vista a controllo reale, vedi mockup): una card con la stessa
    // maniglia colorata/sfondo delle card Edit, etichetta tipo + nome canale
    // in testa, poi il controllo VERO a tutta larghezza - uno slider
    // orizzontale (con box VALUE a destra e min/max sotto) per Float/Int, un
    // pill toggle per Bool, un combo a tutta larghezza per Choice - agganciato
    // al parametro apvts VERO tramite le classi di attachment "ufficiali" di
    // JUCE.
    struct GenericParamRow final : public juce::Component
    {
        enum class Kind { slider, toggle, choice };

        // accent/typeLabelText: stessi colori/testo della card Edit
        // corrispondente (kSliderAccent/"SLIDER FLOAT" ecc.), passati dal
        // chiamante invece di essere ridotti qui - cosi' le due modalita'
        // restano visivamente coerenti senza duplicare la tabella type->colore.
        // treatAsInteger sceglie la formattazione del box VALUE (0 decimali
        // per Int, 3 per Float) - l'unica differenza reale tra i due, dato che
        // entrambi usano Kind::slider. getChannelNameFn rilegge il nome
        // canale ATTUALE dal processor (invece di usare il solo "channelName"
        // catturato alla costruzione, che puo' diventare obsoleto se l'utente
        // rinomina il canale in modalita' Edit senza che questa riga venga
        // ricreata) - serve alla maniglia di trascinamento e al menu "Copy"
        // del tasto destro, esattamente come in ParamRow/IntParamRow/ecc.
        // getConfigCommentFn rilegge il contenuto del commento di
        // configurazione ATTUALE (es. "SLIDER FLOAT: Min=0; Max=10; ...",
        // vedi makeFloatConfigComment() ecc. nel .cpp) - stesso principio di
        // getChannelNameFn, usato dalla maniglia/dal menu "Copy" per
        // anteporre un commento Csound classico (";...") al chnget,
        // esattamente come le righe Edit.
        GenericParamRow (const juce::String& channelName, juce::RangedAudioParameter& parameter,
                          Kind kind, const juce::StringArray& choiceLabels,
                          juce::Colour accent, const juce::String& typeLabelText,
                          std::function<juce::String()> getChannelNameFn,
                          std::function<juce::String()> getConfigCommentFn,
                          bool treatAsInteger = false);

        void resized() override;
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseDrag (const juce::MouseEvent& event) override;
        void mouseMove (const juce::MouseEvent& event) override;
        void mouseExit (const juce::MouseEvent& event) override;

        // Stesso identico scopo di ParamRow::onCopiedToClipboard - riscontro
        // visibile in consolle dopo una copia da tasto destro sulla maniglia.
        std::function<void (const juce::String&)> onCopiedToClipboard;

    private:
        // Rilegge il valore ATTUALE dallo slider e aggiorna il testo del box
        // VALUE - MAI mentre l'utente ci sta scrivendo dentro (vedi
        // valueReadout.hasKeyboardFocus() nel .cpp), altrimenti gli
        // sovrascriverebbe il testo a meta' digitazione.
        void updateValueReadout();

        // Chiamata da valueReadout.onReturnKey/onFocusLost (commit SOLO li',
        // non ad ogni carattere - digitare un numero carattere per carattere
        // produce valori intermedi senza senso, es. "0." o "-") - analizza
        // il testo, aggiorna lo slider (quindi il parametro apvts reale
        // tramite sliderAttachment) e rilegge il valore clampato/formattato.
        void commitValueFromField();

        void updateHandleHover (juce::Point<int> position);

        const Kind kind;
        const juce::Colour accentColour;
        const bool isIntegerLike;
        std::function<juce::String()> getChannelName;
        std::function<juce::String()> getConfigComment;

        juce::Label typeLabel;
        juce::Label channelNameLabel;

        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
        juce::Label minLabel, maxLabel;
        juce::TextEditor valueReadout;   // editabile da tastiera, non un semplice readout - niente didascalia sopra (riga compatta)

        juce::ToggleButton toggle;
        juce::ComboBox comboBox;

        // Dichiarati DOPO i widget che referenziano: distrutti PRIMA di essi.
        std::unique_ptr<juce::SliderParameterAttachment> sliderAttachment;
        std::unique_ptr<juce::ButtonParameterAttachment> buttonAttachment;
        std::unique_ptr<juce::ComboBoxParameterAttachment> comboAttachment;

        // Striscia della maniglia (barra colorata a tutta altezza, lato
        // sinistro) - stesso identico meccanismo di ParamRow: mouseDown/
        // mouseDrag intercettano il trascinamento SOLO li', handleHovered e'
        // solo feedback visivo (puntini piu' chiari in paintCardChrome()).
        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenericParamRow)
    };

    // Interfaccia minima per poter commutare UI/Edit e conoscere l'altezza
    // preferita di una riga qualunque (Float/Int/Bool/Choice) dal codice
    // della lista unificata, senza dover conoscere il tipo concreto.
    struct UnifiedRowInterface
    {
        virtual ~UnifiedRowInterface() = default;
        virtual void setEditMode (bool edit) = 0;
        virtual void focusNameField() = 0;

        // Altezza richiesta da questa riga per il dato editMode e la data
        // larghezza DISPONIBILE per l'intera card (serve il secondo
        // parametro perche' Slider/Knob possono aver bisogno di UNA riga di
        // campi in piu' quando il pannello e' stretto - vedi
        // computeWrappedFieldRows() - invece della scrollbar orizzontale).
        // In UI e' sempre rowHeight (riga sottile, mai a capo).
        virtual int getPreferredHeight (bool editMode, int availableWidth) const = 0;
    };

    struct FloatUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        FloatUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                          std::function<void (const juce::String&)> onCopied,
                          std::function<void()> onRemoved);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (bool edit, int availableWidth) const override
        {
            if (! edit)
                return uiCardHeight;

            const auto rows = computeWrappedFieldRows (fieldsAvailableWidth (availableWidth),
                { cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldStepWidth },
                cardFieldGap);
            return cardHeightForFieldRows (rows);
        }

        std::unique_ptr<ParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatUnifiedRow)
    };

    struct IntUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        IntUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                        std::function<void (const juce::String&)> onCopied,
                        std::function<void()> onRemoved);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (bool edit, int availableWidth) const override
        {
            if (! edit)
                return uiCardHeight;

            const auto rows = computeWrappedFieldRows (fieldsAvailableWidth (availableWidth),
                { cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth }, cardFieldGap);
            return cardHeightForFieldRows (rows);
        }

        std::unique_ptr<IntParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntUnifiedRow)
    };

    struct BoolUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        BoolUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                         std::function<void (const juce::String&)> onCopied,
                         std::function<void()> onRemoved);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (bool edit, int) const override { return edit ? cardHeightSingleRow : uiCardHeight; }

        std::unique_ptr<BoolParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoolUnifiedRow)
    };

    struct ChoiceUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        ChoiceUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                           std::function<void (const juce::String&)> onCopied,
                           std::function<void()> onRemoved);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (bool edit, int) const override { return edit ? cardHeightDoubleRow : uiCardHeight; }

        std::unique_ptr<ChoiceParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceUnifiedRow)
    };

    // Riferimento al processor - tenuto per rebuildUnifiedRows()/
    // addNewParameter(), chiamate ben oltre il costruttore.
    CsoundAudioProcessor& processor;

    // Dichiarata PER PRIMA tra i membri sotto: i membri si distruggono
    // nell'ordine INVERSO a quello di dichiarazione, quindi essendo la prima
    // e' anche l'ULTIMA a essere distrutta - lookAndFeel resta valida per
    // tutta la vita delle righe/bottoni, mai un puntatore a LookAndFeel
    // pendente durante lo smontaggio dell'albero di componenti.
    CsoundParameterPanelLookAndFeel lookAndFeel;

    // Bottone "Edit": SEMPRE visibile, commuta editMode per OGNI riga della
    // lista (vedi toggleEditMode()) - nessuna tab da selezionare, un solo
    // interruttore globale. Nome "editToggle" riconosciuto da
    // CsoundParameterPanelLookAndFeel::drawButtonText per disegnarci sopra
    // l'icona "tune".
    juce::TextButton editToggleButton { "Edit" };
    bool editMode = false;
    void toggleEditMode();

    // Bottone "+"/Add: SEMPRE visibile, apre un menu Slider/Knob/Toggle/Menu
    // (vedi showAddMenu()) che individua il primo slot libero del tipo
    // corrispondente (0=Float/Slider, 1=Int/Knob, 2=Bool/Toggle,
    // 3=Choice/Menu) e fa apparire una card "in sospeso" per quello slot -
    // vedi createPendingRow() e il commento in testa alla classe sul flusso
    // completo.
    juce::TextButton addButton { "+" };
    void showAddMenu();

    // kind: 0=Float, 1=Int, 2=Bool, 3=Choice - indice semplice invece di un
    // enum dedicato solo per questo, usato una volta sola qui.
    void addNewParameter (int kind);

    // Crea la riga "in sospeso" (pendingRow sotto) per lo slot (kind,
    // slotIndex) - vedi il commento in testa alla classe sul flusso
    // completo (scarto se Invio con nome vuoto, promozione a riga vera
    // altrimenti).
    void createPendingRow (int kind, int slotIndex);

    // Ricostruisce DA ZERO la lista unificata leggendo lo stato ATTUALE di
    // tutti e 4 i pool di slot (solo quelli con channelName non vuoto -
    // vedi il commento in testa alla classe) - chiamata dal costruttore,
    // da refreshAllFromProcessor() (Load Session), dalla promozione di una
    // pendingRow e, in modo ASINCRONO (juce::MessageManager::callAsync, mai
    // sincrono - vedi il commento su ParamRow::onRemoveRequested) da ogni
    // onRemoveRequested di ogni riga della lista.
    void rebuildUnifiedRows();

    juce::Viewport viewport;
    juce::Component rowsContainer;

    std::vector<std::unique_ptr<juce::Component>> unifiedRows;

    // La riga "in sospeso" creata da createPendingRow() (vedi sopra) -
    // SEPARATA da unifiedRows apposta: rappresenta uno slot che il
    // processor NON ha ancora (o non ha piu', se scartata) come allocato
    // in modo definitivo, quindi non deve mai passare per
    // rebuildUnifiedRows() (che la ignorerebbe, essendo il suo slot vuoto
    // finche' l'utente non scrive un nome). Sempre al massimo UNA alla
    // volta (createPendingRow() scarta quella precedente se ce n'e' gia'
    // una), mostrata in coda a unifiedRows nel viewport - vedi resized().
    std::unique_ptr<juce::Component> pendingRow;

    // Mostrata SOLO quando non c'e' NULLA da mostrare (unifiedRows vuoto e
    // nessuna pendingRow) - invita a usare il bottone "+" invece di un
    // pannello vuoto senza spiegazione.
    juce::Label emptyStateLabel;

    static constexpr int rowHeight = 30;    // solo fallback per righe senza UnifiedRowInterface (non dovrebbe mai accadere)
    static constexpr int toolbarHeight = 38; // = diametro di "+"/"Edit" (bottoni circolari piu' grandi, richiesta esplicita)
    static constexpr int toolbarButtonGap = 4; // tra "+" ed "Edit", invece del margine di 10px di prima (richiesta esplicita: "piu' ravvicinati")

    // Spazio riservato SEMPRE sul lato destro del contenuto per la scrollbar
    // verticale del viewport (vedi il commento storico sul perche' non si usa
    // viewport.getMaximumVisibleWidth() direttamente).
    static constexpr int scrollbarGutter = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundParameterMappingPanel)
};
