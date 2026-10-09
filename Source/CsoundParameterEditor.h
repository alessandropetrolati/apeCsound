#pragma once

#include <JuceHeader.h>
#include <array>
#include <set>
#include <utility>
#include <vector>
#include "PluginProcessor.h"
#include "CsoundActionSheet.h"

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

    Ogni riga della lista unificata ha una sua PROPRIA icona "edit" (angolo
    in alto a destra della card, vedi editIconButton in ciascuno dei 4
    *UnifiedRow sotto) che commuta SOLO quella riga tra due visualizzazioni -
    non esiste piu' un interruttore globale che le mostri/nasconda tutte
    insieme (richiesta esplicita: ogni parametro apre il SUO contenuto senza
    toccare gli altri). Il bottone "+" in cima al pannello resta l'unico
    controllo della toolbar, centrato da solo sulla larghezza del pannello:
      - modalita' UI (default per ogni riga): una card (vedi uiCardHeight) con
        etichetta tipo + nome canale in testa e il controllo VERO (slider/
        toggle/combo, agganciato al parametro apvts reale tramite
        GenericParamRow) a tutta larghezza sotto - stessa maniglia colorata e
        sfondo delle card Edit, ma senza campi editabili;
      - modalita' Edit (solo per la riga la cui icona e' stata premuta): una
        "card" piu' alta (vedi cardHeightSingleRow/cardHeightDoubleRow) con,
        dall'alto in basso: etichetta tipo colorata + bottone di rimozione
        ("SLIDER · CANALE" ecc. - vedi ParamRow/IntParamRow/BoolParamRow/
        ChoiceParamRow), il campo nome canale a tutta larghezza in grande,
        poi una riga di campi con etichetta sopra ciascuno (MIN/MAX/INIT/EXP/
        STEP per Float, MIN/MAX/INIT per Int, INIT+checkbox per Bool,
        DEFAULT per Choice) e, solo per Choice, una seconda riga larga quanto
        la card per le opzioni (OPZIONI, separate da virgola). La maniglia di
        trascinamento e' una barra verticale colorata (diversa per tipo) sul
        lato sinistro della card - vedi layoutCardSkeleton()/
        paintCardChrome(). L'icona edit resta visibile in ENTRAMBE le
        modalita' e SEMPRE nell'angolo in alto a DESTRA (richiesta
        esplicita) cosi' si puo' tornare alla vista UI dalla stessa icona
        che ha aperto quella Edit; il cerchio rosso "-" di rimozione, visibile
        solo in modalita' Edit, e' invece nell'angolo in alto a SINISTRA
        (vedi layoutCardSkeleton()), per non essere mai adiacente all'icona
        edit.

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
class CsoundParameterMappingPanel final : public juce::Component,
                                          private CsoundAudioProcessor::Listener
{
public:
    explicit CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit,
                                           juce::UndoManager& sharedUndoManager);
    ~CsoundParameterMappingPanel() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    // CsoundAudioProcessor::Listener: un'azione della cronologia (che vive
    // nel processor e NON cattura piu' questo pannello - sopravvive alla
    // chiusura dell'editor) ha cambiato la struttura dei parametri:
    // ricostruisce la lista. structureReplaced (Load/Initialize/Relocate)
    // scarta anche un'eventuale riga "in sospeso".
    void sessionEditedByUndoRedo (bool structureReplaced) override;

    // Ricostruisce l'intera lista unificata dallo stato attuale del
    // processor - chiamata da PluginEditor dopo un Load Session da file
    // (vedi CsoundAudioProcessor::loadSessionFromFile), cosi' il pannello
    // mostra subito il contenuto appena caricato invece dei vecchi valori
    // della sessione precedente.
    void refreshAllFromProcessor();

    // Undo/Redo GENERALI per i METADATA (nome canale, min/max/default/skew/
    // step, default bool, opzioni/default choice) e per aggiunta/rimozione
    // di un parametro - NON per i VALORI dei parametri (quelli sono gestiti
    // dalla DAW/host, vedi il commento su undoManager piu' sotto). Agiscono
    // su undoManager, che e' un RIFERIMENTO alla STESSA sharedUndoManager di
    // PluginEditor usata anche per l'editor di codice (vedi
    // PluginEditor::performUndo()/performRedo()) - quindi chiamare undo()/
    // redo() qui o chiamare sharedUndoManager.undo()/redo() direttamente da
    // PluginEditor sono equivalenti: una SINGOLA cronologia, in ordine
    // cronologico reale, condivisa tra editor e pannello Parametri
    // ("linearita' avanti e indietro", richiesta esplicita).
    //   - le modifiche ai METADATA in modalita' Edit e l'aggiunta/rimozione
    //     di un parametro NON passano per apvts (sono struct custom nel
    //     processor, non parametri) - per queste usiamo undoManager con una
    //     nostra UndoableAction minimale (vedi LambdaUndoableAction nel
    //     .cpp) costruita a mano ad ogni commit finale (Return/Esc/focus
    //     perso/click), tramite il campo pushUndo di ciascuna riga Edit
    //     (vedi piu' sotto) - mai catturando `this` della riga nelle due
    //     lambda (puo' essere distrutta e ricostruita da
    //     rebuildUnifiedRows() nel frattempo), solo processor/index/i
    //     valori dello slot, con un refresh della lista fatto dal pannello
    //     stesso dopo ogni perform()/undo().
    //   - i controlli VERI in modalita' UI (slider/toggle/combo di
    //     GenericParamRow) NON usano piu' juce::UndoManager (vedi il
    //     commento sul costruttore di GenericParamRow): i loro 3 attachment
    //     JUCE ricevono nullptr, perche' i VALORI sono gestiti dalla DAW.
    bool undo();
    bool redo();

    // menuButton (vedi il membro privato piu' sotto) vive VISIVAMENTE nella
    // toolbar PRINCIPALE di PluginEditor, ancorato a destra (richiesta
    // esplicita) - PluginEditor lo riparenta con addAndMakeVisible() (JUCE
    // sposta automaticamente un Component dal suo vecchio genitore al
    // nuovo) e lo posiziona nel proprio resized(). L'OGGETTO (stile, nome,
    // onClick -> showPanelMenu()) resta pero' di proprieta' di questo
    // pannello, impostato come sempre nel costruttore - da cui questo
    // accessor. Il "+" (addButton) invece NON viene piu' riparentato
    // (richiesta esplicita: e' tornato dentro la title bar di QUESTO
    // pannello, vedi il commento sulla sua dichiarazione piu' sotto) -
    // nessun accessor pubblico per lui.
    juce::Button& getMenuButton() { return menuButton; }

    // Permette a PluginEditor di applicare la STESSA LookAndFeel di questo
    // pannello al bottone sopra una volta riparentato - altrimenti
    // erediterebbe quella di PluginEditor (CsoundLookAndFeel), che non sa
    // disegnare la sua icona hamburger (nome "burgerMenu", riconosciuto
    // SOLO da drawButtonText/drawButtonBackground di QUESTA LookAndFeel,
    // vedi lookAndFeel piu' sotto).
    juce::LookAndFeel& getButtonLookAndFeel() { return lookAndFeel; }

    // Impostate da PluginEditor (uniche funzioni che sanno davvero
    // scrivere/leggere un .csd su disco, vedi promptSaveSession()/
    // promptLoadSession() in PluginEditor.h) - richiamate dalle voci "Save"
    // /"Load..." del menu hamburger (vedi showPanelMenu()). I vecchi
    // bottoni "Save as..."/"Load" nella toolbar principale sono stati
    // rimossi: questa e' ora l'UNICA via per queste due azioni.
    std::function<void()> onSaveSessionRequested;
    std::function<void()> onLoadSessionRequested;

    // "Save" (richiesta esplicita, accanto a Save As/Load): sovrascrive il
    // file .csd COLLEGATO alla sessione senza chiedere il path - vedi
    // CsoundAudioProcessorEditor::performSaveLinked() (che ricade su Save
    // As se la sessione non e' collegata a nessun file).
    std::function<void()> onSaveLinkedRequested;

    // "About apeCsound" del menu hamburger: impostata da PluginEditor, che
    // mostra la vista informazioni (vedi AboutView in PluginEditor.h).
    std::function<void()> onAboutRequested;

    // "Modern syntax in help" (spunta) e "Csound Manual (online)": entrambe
    // impostate da PluginEditor. isModernSyntaxEnabled serve solo a
    // disegnare la spunta nel menu.
    std::function<void()> onToggleModernSyntaxRequested;
    std::function<bool()> isModernSyntaxEnabled;
    std::function<void()> onOpenManualRequested;

    // "Follow CSD nchnls" (spunta): Csound usa nchnls/nchnls_i del .csd
    // invece dei canali della traccia - vedi
    // CsoundAudioProcessor::setFollowCsdChannels.
    std::function<void()> onToggleFollowCsdChannelsRequested;
    std::function<bool()> isFollowingCsdChannels;

    // Chiamata alla fine di OGNI rebuildUnifiedRows() (aggiunta/rimozione
    // di un parametro, commit di un metadata, undo/redo, refresh dopo Load/
    // ripristino): PluginEditor la usa per ricontrollare se la sessione
    // (codice nell'editor + struttura dei parametri) differisce ancora dal
    // file .csd collegato su disco e aggiornare la barra di avviso
    // (richiesta esplicita: "quando aggiungo o rimuovo un parametro o
    // modifico qualcosa sul codice, voglio vedere la segnalazione
    // dell'incongruenza col file su disco").
    std::function<void()> onMappingChanged;

    // Impostata da PluginEditor (vedi promptInitializeSession()/
    // performInitializeSession() in PluginEditor.h) - richiamata dalla
    // voce "Initialize Session" del menu hamburger (richiesta esplicita:
    // "pulisce tutto e carica il CSD hard coded") - vedi showPanelMenu().
    // Stesso meccanismo di onSaveSessionRequested/onLoadSessionRequested
    // sopra: questo pannello non sa nulla del documento dell'editor di
    // codice, quindi non puo' implementarla da solo.
    std::function<void()> onInitializeSessionRequested;

    // Impostate da PluginEditor con performUndo()/performRedo() (vedi
    // PluginEditor.h) - richiamate dalle voci "Undo"/"Redo" del menu
    // hamburger (vedi showPanelMenu()) invece di undo()/redo() diretti,
    // cosi' il percorso menu e quello Cmd+Z/Cmd+Shift+Z passano sempre dallo
    // stesso punto. Dato che undoManager e' ora un riferimento alla STESSA
    // sharedUndoManager usata da performUndo()/performRedo(), il risultato
    // e' identico a chiamare undo()/redo() qui - ma mantenendo
    // l'indirezione, PluginEditor resta l'unico posto che decide come
    // instradare l'azione. Se non impostate (non dovrebbe succedere in
    // pratica), la voce di menu ricade sul solo undo()/redo() di questo
    // pannello.
    std::function<void()> onUndoRequested;
    std::function<void()> onRedoRequested;

    // Impostate da PluginEditor - mostrano/nascondono rispettivamente la
    // sidebar Parametri e la consolle (vedi toggleParameterPanel()/
    // toggleConsole() in PluginEditor.h/.cpp). I vecchi bottoni dedicati
    // "Parameters"/"Hide Console" nella toolbar principale sono stati
    // rimossi (richiesta esplicita): queste azioni sono ora SOLO voci
    // spuntabili nella sezione "View" del menu hamburger (vedi
    // showPanelMenu()), spuntate quando il rispettivo elemento e' visibile
    // (isParametersPanelVisible/isConsoleVisible).
    std::function<void()> onToggleParametersRequested;
    std::function<void()> onToggleConsoleRequested;
    std::function<bool()> isParametersPanelVisible;
    std::function<bool()> isConsoleVisible;

    // Impostata da PluginEditor - apre la sidebar Parametri SE E SOLO SE e'
    // attualmente chiusa (non la richiude se e' gia' aperta, a differenza
    // di onToggleParametersRequested sopra) - richiamata da
    // addNewParameter() PRIMA di creare la riga "in sospeso" del nuovo
    // parametro, cosi' sia il bottone "+" sia la voce "Add parameter" del
    // menu hamburger mostrano sempre la sidebar invece di aggiungere un
    // parametro "a vuoto" in un pannello invisibile (richiesta esplicita).
    std::function<void()> onEnsurePanelVisible;

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
    static constexpr int cardHeaderHeight = 38; // ingrandito ANCORA (richiesta esplicita: Remove/Copy/Edit "ancora piu' grandi... un poco piu' spaziati") - deve restare >= rowEditIconSize sotto, altrimenti jmin() in layoutRowIconButtons/layoutCardSkeleton tornerebbe a schiacciare le icone
    static constexpr int cardHeaderGap = 6;
    static constexpr int cardNameHeight = 30;
    static constexpr int cardNameGap = 10;
    static constexpr int cardFieldCaptionHeight = 13;
    static constexpr int cardFieldCaptionGap = 3;
    static constexpr int cardFieldBoxHeight = 30;
    static constexpr int cardFieldRowGap = 10;
    static constexpr int cardGap = 8; // spazio verticale TRA le card, nella lista

    // Title bar del pannello (richiesta esplicita: "Riabilita la Title bar
    // in Parameters, con la label centrale") - vedi resized()/paint() nel
    // .cpp. panelTitleBarButtonDiameter e' il diametro del "+" circolare a
    // sinistra, leggermente piu' piccolo dell'altezza della barra per
    // lasciare un margine sopra/sotto.
    static constexpr int panelTitleBarHeight = 40;
    static constexpr int panelTitleBarButtonDiameter = 30;
    static constexpr int panelTitleBarPaddingH = 8;

    // Larghezze dei singoli campi numerici nella riga sotto il nome - STEP
    // (Float) usa la stessa larghezza "narrow" di Min/Max/Init/Exp
    // (richiesta esplicita: prima era piu' largo per contenere valori come
    // "0.001", ma doveva restare uniforme con gli altri campi).
    static constexpr int cardFieldNarrowWidth = 68;
    static constexpr int cardFieldStepWidth = cardFieldNarrowWidth; // stessa larghezza di Min/Max/ecc. (richiesta esplicita, prima era piu' largo)
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
    static constexpr int uiCardHeaderHeight = 38;   // solo etichetta tipo (niente bottone rimozione) - stessa altezza di cardHeaderHeight sopra, per fare spazio a editIconButton/copyButton ingranditi
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

    // Icone "edit"/"copy" proprie di OGNI riga (angolo in alto a destra
    // della card, sia in modalita' UI sia Edit - vedi editIconButton/
    // copyButton in ciascuno dei 4 *UnifiedRow piu' sotto) - senza sfondo
    // pieno a riposo, visto che si ripetono su ogni card, a differenza del
    // vecchio bottone "Edit" globale nella toolbar (rimosso). Ingrandite
    // ulteriormente (richiesta esplicita, pensando a un uso touch/iOS: "i
    // bottoni devono essere piu' grandi" - non il target 44pt delle linee
    // guida Apple, che qui non ci sta per riga, ma un'area di tocco
    // comunque nettamente piu' comoda di prima).
    static constexpr int rowEditIconSize = 34;  // ingrandito ANCORA (richiesta esplicita) - Remove/Copy/Edit usano TUTTI questa stessa costante, quindi restano sempre della stessa dimensione fra loro
    static constexpr int rowEditIconMargin = 4; // spazio tra l'icona e il bordo della card / il bottone di rimozione
    static constexpr int rowIconButtonGap = 10; // ingrandito ANCORA (richiesta esplicita: "un poco piu' spaziati") - spazio TRA copyButton ed editIconButton (e, in modalita' Edit, tra removeButton e copyButton)

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
                                  juce::Rectangle<int> handleStrip, bool showHandleDots, bool handleHovered,
                                  bool isEditingCard);

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
        // Assegnata dal pannello (mai dal costruttore - stesso stile di
        // onCopiedToClipboard) per collegare questa riga all'undoManager
        // condiviso (vedi CsoundParameterMappingPanel::undo()/redo() per il
        // quadro completo): primo argomento = lambda da eseguire ORA/al
        // "redo", secondo = lambda da eseguire all'"undo" - entrambe le
        // lambda, costruite dal chiamante (commitFromFields() e dintorni),
        // NON devono mai catturare `this` di QUESTA riga (puo' essere
        // distrutta da un rebuildUnifiedRows() nel frattempo), solo
        // processor/index/valori per copia o riferimento stabile.
        std::function<void (std::function<void()>, std::function<void()>)> pushUndo;

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

        // Esc deve comportarsi ESATTAMENTE come Return (richiesto
        // esplicitamente): commit del campo (o scarto se il nome e' ancora
        // vuoto - vedi onRemoveRequested) + perdita del focus, invece del
        // comportamento di default di juce::TextEditor (che consuma Esc per
        // conto suo senza notificare nessuno dei listener sopra - ecco
        // perche' prima Esc non faceva nulla di visibile qui dentro).
        void textEditorEscapeKeyPressed (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);
        void notifyRemovedIfEmpty();
        void notifyCommittedIfNonEmpty();

        // Spinge su pushUndo (se agganciata) l'azione accumulata da quando
        // hasBeforeEditSlot e' diventato true (vedi textEditorTextChanged) -
        // chiamata da TUTTI i commit finali (Return/Esc/focus perso), MAI
        // da textEditorTextChanged stesso: un'azione di Undo per intera
        // sessione di digitazione, non una per carattere.
        void pushPendingUndoIfAny();

        // Istantanea dello slot presa al PRIMO carattere digitato dopo
        // l'ultimo commit finale (hasBeforeEditSlot passa a true solo
        // allora, vedi textEditorTextChanged) - "prima" per Undo quando
        // arriva il prossimo commit finale.
        CsoundAudioProcessor::ChannelParamSlot beforeEditSlot;
        bool hasBeforeEditSlot = false;

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
        // Assegnata dal pannello (mai dal costruttore - stesso stile di
        // onCopiedToClipboard) per collegare questa riga all'undoManager
        // condiviso (vedi CsoundParameterMappingPanel::undo()/redo() per il
        // quadro completo): primo argomento = lambda da eseguire ORA/al
        // "redo", secondo = lambda da eseguire all'"undo" - entrambe le
        // lambda, costruite dal chiamante (commitFromFields() e dintorni),
        // NON devono mai catturare `this` di QUESTA riga (puo' essere
        // distrutta da un rebuildUnifiedRows() nel frattempo), solo
        // processor/index/valori per copia o riferimento stabile.
        std::function<void (std::function<void()>, std::function<void()>)> pushUndo;

        std::function<void()> onRemoveRequested;
        std::function<void()> onCommittedNonEmpty;

        void focusChannelNameField();

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        // Esc deve comportarsi ESATTAMENTE come Return (richiesto
        // esplicitamente): commit del campo (o scarto se il nome e' ancora
        // vuoto - vedi onRemoveRequested) + perdita del focus, invece del
        // comportamento di default di juce::TextEditor (che consuma Esc per
        // conto suo senza notificare nessuno dei listener sopra - ecco
        // perche' prima Esc non faceva nulla di visibile qui dentro).
        void textEditorEscapeKeyPressed (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);
        void notifyRemovedIfEmpty();
        void notifyCommittedIfNonEmpty();
        void pushPendingUndoIfAny();

        CsoundAudioProcessor::IntParamSlot beforeEditSlot;
        bool hasBeforeEditSlot = false;

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
        // Assegnata dal pannello (mai dal costruttore - stesso stile di
        // onCopiedToClipboard) per collegare questa riga all'undoManager
        // condiviso (vedi CsoundParameterMappingPanel::undo()/redo() per il
        // quadro completo): primo argomento = lambda da eseguire ORA/al
        // "redo", secondo = lambda da eseguire all'"undo" - entrambe le
        // lambda, costruite dal chiamante (commitFromFields() e dintorni),
        // NON devono mai catturare `this` di QUESTA riga (puo' essere
        // distrutta da un rebuildUnifiedRows() nel frattempo), solo
        // processor/index/valori per copia o riferimento stabile.
        std::function<void (std::function<void()>, std::function<void()>)> pushUndo;

        std::function<void()> onRemoveRequested;
        std::function<void()> onCommittedNonEmpty;

        void focusChannelNameField();

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        // Esc deve comportarsi ESATTAMENTE come Return (richiesto
        // esplicitamente): commit del campo (o scarto se il nome e' ancora
        // vuoto - vedi onRemoveRequested) + perdita del focus, invece del
        // comportamento di default di juce::TextEditor (che consuma Esc per
        // conto suo senza notificare nessuno dei listener sopra - ecco
        // perche' prima Esc non faceva nulla di visibile qui dentro).
        void textEditorEscapeKeyPressed (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);
        void notifyRemovedIfEmpty();
        void notifyCommittedIfNonEmpty();
        void pushPendingUndoIfAny();

        CsoundAudioProcessor::BoolParamSlot beforeEditSlot;
        bool hasBeforeEditSlot = false;

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
        // Assegnata dal pannello (mai dal costruttore - stesso stile di
        // onCopiedToClipboard) per collegare questa riga all'undoManager
        // condiviso (vedi CsoundParameterMappingPanel::undo()/redo() per il
        // quadro completo): primo argomento = lambda da eseguire ORA/al
        // "redo", secondo = lambda da eseguire all'"undo" - entrambe le
        // lambda, costruite dal chiamante (commitFromFields() e dintorni),
        // NON devono mai catturare `this` di QUESTA riga (puo' essere
        // distrutta da un rebuildUnifiedRows() nel frattempo), solo
        // processor/index/valori per copia o riferimento stabile.
        std::function<void (std::function<void()>, std::function<void()>)> pushUndo;

        std::function<void()> onRemoveRequested;
        std::function<void()> onCommittedNonEmpty;

        void focusChannelNameField();

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        // Esc deve comportarsi ESATTAMENTE come Return (richiesto
        // esplicitamente): commit del campo (o scarto se il nome e' ancora
        // vuoto - vedi onRemoveRequested) + perdita del focus, invece del
        // comportamento di default di juce::TextEditor (che consuma Esc per
        // conto suo senza notificare nessuno dei listener sopra - ecco
        // perche' prima Esc non faceva nulla di visibile qui dentro).
        void textEditorEscapeKeyPressed (juce::TextEditor&) override;

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

        void pushPendingUndoIfAny();

        CsoundAudioProcessor::ChoiceParamSlot beforeEditSlot;
        bool hasBeforeEditSlot = false;

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
        // NIENTE juce::UndoManager qui: i VALORI dei parametri sono
        // gestiti dalla DAW/host (automazione, stato di sessione) e NON
        // devono comparire nella cronologia Undo/Redo del plugin - per
        // questo i 3 attachment sotto (SliderParameterAttachment/
        // ButtonParameterAttachment/ComboBoxParameterAttachment) ricevono
        // nullptr invece di un UndoManager (vedi il costruttore nel
        // .cpp). L'unica cronologia Undo/Redo del plugin e' sharedUndoManager
        // in PluginEditor, che copre editor di codice + metadata/mapping dei
        // parametri in modalita' Edit (vedi pushUndo su ParamRow/ecc.), MAI
        // i valori stessi.
        // sliderToRealFn/realToSliderFn (SOLO per Kind::slider, ignorate
        // altrimenti - passare {} per toggle/choice): il parametro apvts
        // sottostante ha SEMPRE un range nativo FISSO (0..1 per i Float,
        // 0..intHostRangeMax per gli Int, vedi ChannelHostParameter/
        // IntHostParameter in PluginProcessor.h/.cpp) - min/max REALI
        // configurati dall'utente vivono SOLO nello slot (ChannelParamSlot/
        // IntParamSlot) e servono finora solo per il canale Csound
        // (denormalizeChannelParam/denormalizeIntParam). BUG corretto qui:
        // lo slider/i box min/max/valore di QUESTA riga mostravano il
        // valore nativo grezzo dello slider (0..1 o 0..intHostRangeMax),
        // MAI il range reale configurato - sliderToRealFn/realToSliderFn
        // fanno da ponte, rilegendo lo slot AL VOLO ad ogni chiamata (cosi'
        // restano sempre aggiornate se l'utente cambia Min/Max in modalita'
        // Edit, vedi refreshRangeDisplay() chiamata da *UnifiedRow::
        // setEditMode() quando si torna alla vista UI).
        GenericParamRow (const juce::String& channelName, juce::RangedAudioParameter& parameter,
                          Kind kind, const juce::StringArray& choiceLabels,
                          juce::Colour accent, const juce::String& typeLabelText,
                          std::function<juce::String()> getChannelNameFn,
                          std::function<juce::String()> getConfigCommentFn,
                          bool treatAsInteger = false,
                          std::function<double (double)> sliderToRealFn = {},
                          std::function<double (double)> realToSliderFn = {});

        // Rilegge lo slot (tramite sliderToReal/realToSlider) e riaggiorna
        // minLabel/maxLabel/valueReadout - chiamata alla costruzione e da
        // *UnifiedRow::setEditMode() quando si torna da Edit alla vista UI,
        // cosi' un Min/Max appena modificato si vede SUBITO (vedi il
        // commento sul costruttore sopra). No-op per toggle/choice.
        void refreshRangeDisplay();

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
        std::function<double (double)> sliderToReal; // vedi il commento sul costruttore
        std::function<double (double)> realToSlider;  // vedi il commento sul costruttore
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

        // Altezza richiesta da questa riga ORA, per la data larghezza
        // DISPONIBILE per l'intera card (serve il parametro perche'
        // Slider/Knob possono aver bisogno di UNA riga di campi in piu'
        // quando il pannello e' stretto - vedi computeWrappedFieldRows() -
        // invece della scrollbar orizzontale). Non prende piu' un editMode
        // esterno: ogni riga tiene il proprio (vedi rowEditMode in ciascuna
        // delle 4 implementazioni sotto), commutato dalla SUA editIconButton
        // invece che da un interruttore globale di pannello.
        virtual int getPreferredHeight (int availableWidth) const = 0;
    };

    // Costruisce editIconButton/copyButton, comuni a tutte e 4 le
    // *UnifiedRow sotto - nomi componente fissi ("editToggle"/"copyChnget")
    // riconosciuti da CsoundParameterPanelLookAndFeel::drawButtonText per
    // disegnarci sopra le rispettive icone, stile "piatto" (nessun riquadro
    // pieno a riposo) visto che si ripetono su ogni card invece di essere
    // un unico bottone di toolbar.
    static void setupRowEditIconButton (juce::TextButton& button);
    static void setupRowCopyButton (juce::TextButton& button);

    // Posiziona copyButton ED editIconButton insieme, nell'angolo in alto a
    // destra della card - copyButton SEMPRE immediatamente a sinistra di
    // editIconButton (richiesta esplicita: "un bottone sulla sx di edit per
    // il copy"), in ENTRAMBE le modalita' (sostituisce il vecchio
    // layoutRowEditIconButton, che posizionava solo editIconButton).
    static void layoutRowIconButtons (juce::Rectangle<int> fullBounds, bool rowEditMode,
                                       juce::Component& copyButton, juce::Component& editButton);

    struct FloatUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        FloatUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                          std::function<void (const juce::String&)> onCopied,
                          std::function<void()> onRemoved,
                          std::function<void()> onEditModeChanged);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (int availableWidth) const override
        {
            if (! rowEditMode)
                return uiCardHeight;

            const auto rows = computeWrappedFieldRows (fieldsAvailableWidth (availableWidth),
                { cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldStepWidth },
                cardFieldGap);
            return cardHeightForFieldRows (rows);
        }

        std::unique_ptr<ParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;
        juce::TextButton editIconButton;
        juce::TextButton copyButton;
        bool rowEditMode = false;
        std::function<void()> onEditModeChanged;

        // Avvisa il pannello (vedi rowsInEditMode) ad OGNI chiamata di
        // setEditMode(), sia dall'utente (click su editIconButton) sia dal
        // pannello stesso (ripristino dopo un rebuildUnifiedRows()).
        std::function<void (bool)> onEditModeToggled;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatUnifiedRow)
    };

    struct IntUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        IntUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                        std::function<void (const juce::String&)> onCopied,
                        std::function<void()> onRemoved,
                        std::function<void()> onEditModeChanged);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (int availableWidth) const override
        {
            if (! rowEditMode)
                return uiCardHeight;

            const auto rows = computeWrappedFieldRows (fieldsAvailableWidth (availableWidth),
                { cardFieldNarrowWidth, cardFieldNarrowWidth, cardFieldNarrowWidth }, cardFieldGap);
            return cardHeightForFieldRows (rows);
        }

        std::unique_ptr<IntParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;
        juce::TextButton editIconButton;
        juce::TextButton copyButton;
        bool rowEditMode = false;
        std::function<void()> onEditModeChanged;
        std::function<void (bool)> onEditModeToggled;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntUnifiedRow)
    };

    struct BoolUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        BoolUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                         std::function<void (const juce::String&)> onCopied,
                         std::function<void()> onRemoved,
                         std::function<void()> onEditModeChanged);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (int) const override { return rowEditMode ? cardHeightSingleRow : uiCardHeight; }

        std::unique_ptr<BoolParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;
        juce::TextButton editIconButton;
        juce::TextButton copyButton;
        bool rowEditMode = false;
        std::function<void()> onEditModeChanged;
        std::function<void (bool)> onEditModeToggled;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoolUnifiedRow)
    };

    struct ChoiceUnifiedRow final : public juce::Component, public UnifiedRowInterface
    {
        ChoiceUnifiedRow (CsoundAudioProcessor& processorToEdit, int slotIndex,
                           std::function<void (const juce::String&)> onCopied,
                           std::function<void()> onRemoved,
                           std::function<void()> onEditModeChanged);
        void resized() override;
        void setEditMode (bool edit) override;
        void focusNameField() override { editRow->focusChannelNameField(); }
        int getPreferredHeight (int) const override { return rowEditMode ? cardHeightDoubleRow : uiCardHeight; }

        std::unique_ptr<ChoiceParamRow> editRow;
        std::unique_ptr<GenericParamRow> uiRow;
        juce::TextButton editIconButton;
        juce::TextButton copyButton;
        bool rowEditMode = false;
        std::function<void()> onEditModeChanged;
        std::function<void (bool)> onEditModeToggled;

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

    // Bottone multifunzione (richiesta esplicita: "il bottone + ... ormai e'
    // multifunzionale") - vive ora DENTRO la title bar di QUESTO pannello,
    // a SINISTRA (vedi titleLabel/resized() per il resto della barra), non
    // piu' riparentato nella toolbar principale di PluginEditor. Nome
    // "paramsMenu" (non piu' generico "circular" con icona "+" - vedi
    // CsoundParameterPanelLookAndFeel::drawButtonText) perche' apre un
    // intero menu (showAddMenu(), vedi sotto) con TUTTO cio' che prima era
    // nel submenu "Parameters" del burger (Add Slider Float/Int/Toggle/
    // Menu, Open/Close Config, Remove Parameters, Reset to INIT Values) -
    // quel submenu e' stato rimosso dal burger (vedi showPanelMenu()) dato
    // che ora si raggiunge direttamente da qui, con un tap in meno.
    juce::TextButton addButton { "+" };
    void showAddMenu();

    // Etichetta centrale della title bar (richiesta esplicita: "con la
    // label centrale") - vedi resized(): occupa l'INTERA larghezza della
    // barra (non solo lo spazio tra addButton e il bordo destro), cosi' il
    // testo resta visivamente centrato nel pannello a prescindere dalla
    // presenza di addButton a sinistra, come un titolo di finestra/sheet
    // standard.
    juce::Label titleLabel;

    // Calcolato in resized(), riusato da paint() per disegnare lo sfondo
    // della title bar senza ripetere lo stesso calcolo di layout due volte.
    juce::Rectangle<int> titleBarBounds;

    // Costruisce le voci Slider Float/Slider Int/Toggle/Menu condivise sia
    // da showAddMenu() (il "+") sia dalla voce "Add parameter" di
    // showPanelMenu() sotto (richiesta esplicita: "identico allo shortcut
    // '+'") - un'unica lista di item invece di duplicarla in due posti.
    // Vettore di CsoundActionSheetItem (non piu' un juce::PopupMenu): ogni
    // pagina del foglio ha un proprio spazio di ID indipendente (vedi
    // CsoundActionSheet.h), quindi qui si usano semplicemente gli ID 1..4 -
    // non serve piu' l'offset 10..13 di quando "Add parameter" era un
    // addSubMenu() annidato dentro lo STESSO juce::PopupMenu di showPanelMenu().
    std::vector<CsoundActionSheetItem> buildAddParameterItems();

    // Bottone "menu" (icona hamburger - tre barre orizzontali, riconosciuta
    // da drawButtonText via il nome "burgerMenu") a DESTRA della toolbar,
    // simmetrico al "+" - apre Undo/Redo/Remove Parameters (richiesta
    // esplicita, vedi showPanelMenu()).
    juce::TextButton menuButton;
    void showPanelMenu();

    // "Remove Parameters" del menu sopra: azzera TUTTI gli slot dei 4
    // tipi in un'unica transazione di Undo (come le altre operazioni del
    // pannello - vedi il commento su undo()/redo()).
    void removeAllParameters();

    // "Reset to INIT Values" del menu sopra (richiesta
    // esplicita): riporta il VALORE CORRENTE di ogni parametro assegnato al
    // suo default/init configurato (slot.defaultValue/defaultIndex) tramite
    // juce::RangedAudioParameter::setValueNotifyingHost() - a differenza di
    // removeAllParameters() sopra, NON tocca le mappature/metadata (nome
    // canale, min/max, opzioni...), solo il valore corrente. NESSUNA
    // transazione di Undo: i VALORI sono gestiti dalla DAW/host (vedi il
    // commento sul costruttore di GenericParamRow), quindi questa azione
    // non deve comparire nella cronologia Undo/Redo del plugin, esattamente
    // come un host che riporta i parametri al default non e' "annullabile"
    // dal plugin stesso.
    void resetAllParametersToInit();

    // "Show/Hide Parameters Settings" del menu sopra (richiesta esplicita):
    // apre/chiude la vista Edit (bottone occhio) di TUTTE le righe in un
    // colpo, invece di doverlo fare riga per riga. Ogni UnifiedRowInterface::
    // setEditMode() gia' aggiorna da solo rowsInEditMode (tramite
    // onEditModeToggled, vedi wireEditModeTracking in rebuildUnifiedRows())
    // e fa rifluire la lista (onEditModeChanged) - qui si chiama solo in
    // sequenza su ogni riga, nessuna transazione di Undo (e' un cambio di
    // VISTA, non di dato).
    void setAllRowsEditMode (bool edit);

    // RIFERIMENTO alla sharedUndoManager di PluginEditor (richiesto
    // esplicitamente: "Undo unico" tra editor di codice e pannello
    // Parametri, "linearita' avanti e indietro") - NON un'istanza propria:
    // vedi il commento su undo()/redo() nella sezione public sopra per il
    // quadro completo di chi la usa e come. Usato direttamente qui per le
    // azioni costruite a mano sui metadata (vedi pushUndo); i controlli UI
    // reali (GenericParamRow) NON lo ricevono piu' (vedi il commento sul
    // costruttore di GenericParamRow - i valori sono gestiti dalla DAW).
    juce::UndoManager& undoManager;

    // Slot (kind, slotIndex) attualmente apert* in modalita' Edit - kind:
    // 0=Float, 1=Int, 2=Bool, 3=Choice. rebuildUnifiedRows() distrugge e
    // ricrea TUTTE le righe (serve per add/remove, che fanno
    // apparire/scomparire righe), quindi lo stato "sono in modalita' Edit"
    // di una riga, vivendo nell'oggetto riga stesso, andrebbe perso ad ogni
    // singolo commit di un campo durante l'editing (ogni pushUndo rifa'
    // scattare un rebuild) - l'utente tornerebbe alla vista UI da solo,
    // mentre l'UNICO modo per tornarci deve essere il bottone Edit
    // (richiesta esplicita). Questo set persiste quell'informazione FUORI
    // dalle righe cosi' rebuildUnifiedRows() puo' ripristinarla dopo ogni
    // ricostruzione; viene aggiornato dal callback onEditModeToggled di
    // ciascuna riga (settato qui sotto) e ripulito degli slot ormai vuoti
    // in testa a rebuildUnifiedRows().
    std::set<std::pair<int, int>> rowsInEditMode;

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

    // Spazio riservato SEMPRE sul lato destro del contenuto per la scrollbar
    // verticale del viewport (vedi il commento storico sul perche' non si usa
    // viewport.getMaximumVisibleWidth() direttamente).
    static constexpr int scrollbarGutter = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundParameterMappingPanel)
};
