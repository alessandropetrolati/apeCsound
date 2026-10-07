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

    // Angoli a 90 gradi anche sulla tab bar segmentata (altrimenti
    // LookAndFeel_V4 arrotonderebbe automaticamente i bordi "non connessi"
    // del primo/ultimo bottone del gruppo - vedi setConnectedEdges in
    // CsoundParameterMappingPanel) - coerente con lo stile squadrato
    // richiesto per tutti i widget del plugin.
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

    // Disegna icona + testo SOLO per il bottone della tab "UI" (riconosciuto
    // da Component::setName("genericEditorTab") - vedi CsoundParameterMappingPanel);
    // per tutti gli altri bottoni (Float/Int/Bool/Choice, senza icona) delega
    // al comportamento standard di LookAndFeel_V4. L'icona e' la STESSA
    // "tune" del bottone Parameters nella toolbar (vedi makeTuneIconPath nel
    // .cpp) - stesso path SVG, nessuna icona nuova da inventare.
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

/**
    Sidebar ancorata a destra (non una finestra flottante/overlay - vedi il
    commento su parameterPanel in PluginEditor.h sul perche') per definire,
    per ciascuno dei 64 parametri host float (CsoundAudioProcessor::
    ChannelParamSlot), dei 32 interi (IntParamSlot), dei 32 booleani
    (BoolParamSlot) e dei 16 a scelta multipla (ChoiceParamSlot), il canale
    Csound a cui sono assegnati ("rename" - il parametro apvts resta sempre
    "Float N"/"Int N"/"Bool N"/"Choice N" per l'host) e i relativi metadata
    (range/default/skew/increment per i float, range/default per gli interi, default
    on/off per i bool, etichette/indice di default per i choice).

    I quattro tipi sono isolati in QUATTRO TAB separate, nell'ordine Float,
    Int, Bool, Choice (FloatParamsPage/IntParamsPage/BoolParamsPage/
    ChoiceParamsPage, vedi sotto), non impilati in un'unica lista
    scorrevole: con tutte le righe insieme non si distingueva piu' un tipo
    dall'altro.

    Niente barra del titolo (ne' etichetta ne' pulsante di chiusura, rimossa
    per recuperare spazio verticale per il contenuto): si mostra/nasconde
    SOLO dal bottone "Parameters" nella toolbar di PluginEditor (vedi
    toggleParameterPanel()), niente modo di chiuderla dal pannello stesso.
    La barra tab (Float/Int/Bool/Choice/UI) parte quindi direttamente dal
    bordo superiore; un bordo/sfondo disegnati in paint() restano a
    distinguerla dal resto dell'editor.

    Non mostra/non crea slider per i VALORI correnti: quelli restano
    affidati al meccanismo automatico di JUCE (juce::GenericAudioProcessor
    Editor, agganciato a CsoundAudioProcessor::apvts) o a una skin dedicata
    in futuro - questo pannello riguarda solo i metadata per slot.

    Ogni riga (in tutte e quattro le tab) e' anche una sorgente di drag and
    drop (vedi ParamRow::mouseDrag/IntParamRow::mouseDrag/BoolParamRow::
    mouseDrag/ChoiceParamRow::mouseDrag): trascinando l'area "#N" sull'editor
    di codice (CsoundCodeEditor,
    che implementa DragAndDropTarget) e rilasciando su una riga, viene
    inserito automaticamente un chnget per quel canale - vedi il commento in
    testa a CsoundCodeEditor.h per il formato esatto.
*/
class CsoundParameterMappingPanel final : public juce::Component
{
public:
    explicit CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit);
    ~CsoundParameterMappingPanel() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    // Rilegge TUTTE le righe (tutte e 4 le tab di metadata, piu' la tab
    // Generic Editor se e' quella corrente) dallo stato attuale del
    // processor - chiamata da PluginEditor dopo un Load Session da file
    // (vedi CsoundAudioProcessor::loadSessionFromFile), cosi' il pannello
    // mostra subito il contenuto appena caricato invece dei vecchi valori
    // della sessione precedente.
    void refreshAllFromProcessor();

    // Chiamata quando il tasto destro su una maniglia copia un chnget negli
    // appunti (vedi ParamRow/IntParamRow/BoolParamRow/ChoiceParamRow::
    // onCopiedToClipboard, agganciato riga per riga alla creazione - vedi
    // il costruttore) - PluginEditor la usa per scrivere una riga in
    // consolle (vedi appendToLog), cosi' l'azione (altrimenti invisibile:
    // nessun popup, nessun cambio grafico) lascia una traccia verificabile.
    std::function<void (const juce::String&)> onParameterCopiedToClipboard;

    // Larghezze fisse delle colonne handle/min/max/default/skew/increment (vedi
    // layoutColumns()); nameWidth e' solo la larghezza MINIMA del campo
    // nome, usata per calcolare preferredWidth - il campo stesso si allarga
    // con la larghezza assegnata. preferredWidth e' la larghezza "comoda" di
    // partenza che CsoundAudioProcessorEditor usa come default per la
    // sidebar ancorata a destra (vedi sidebarWidth in PluginEditor.h) - non
    // piu' un vincolo di una finestra flottante, solo il valore iniziale
    // prima che l'utente la ridimensioni trascinando il divisore.
    static constexpr int handleWidth = 30;
    static constexpr int nameWidth = 108;
    static constexpr int minMaxWidth = 80;
    static constexpr int defaultWidth = 80;
    // Colonna condivisa da skew e increment (vedi layoutColumns()): divisa a
    // meta' da ParamRow/FloatParamsPage in due campi affiancati.
    static constexpr int curveWidth = 160;
    static constexpr int preferredWidth = handleWidth + nameWidth + minMaxWidth * 2 + defaultWidth + curveWidth + 10 + 16 + 16 + 10;

    static constexpr int boolDefaultWidth = 70;
    static constexpr int choiceOptionsWidth = 220;
    static constexpr int choiceDefaultIndexWidth = 50;

private:
    // Divide "area" nelle stesse 6 colonne (handle/nome/min/max/default/
    // skew+increment) sia per l'intestazione sia per ogni ParamRow, cosi'
    // le etichette nell'intestazione restano SEMPRE allineate ai campi
    // sotto - un'unica fonte di verita' per il layout, invece di
    // ricalcolarlo due volte con margini separati che potrebbero
    // disallinearsi. handle/min/max/default/skewIncrement hanno larghezza
    // FISSA (l'ultima e' condivisa da due campi affiancati - vedi
    // ParamRow::resized()); il nome prende tutto lo spazio che resta (fino
    // al campo min), quindi l'intera riga scala con la larghezza di "area".
    static void layoutColumns (juce::Rectangle<int> area,
                                juce::Rectangle<int>& handle,
                                juce::Rectangle<int>& name,
                                juce::Rectangle<int>& min,
                                juce::Rectangle<int>& max,
                                juce::Rectangle<int>& defaultVal,
                                juce::Rectangle<int>& skewIncrement);

    // Stesso schema di layoutColumns() ma per le righe Int: handle + nome
    // (elastico) + min/max/default (fissi), senza colonna skew/increment
    // (sempre lineare, passo 1, per gli interi). Usata sia per IntParamRow sia per
    // l'intestazione della tab Int.
    static void layoutIntColumns (juce::Rectangle<int> area,
                                   juce::Rectangle<int>& handle,
                                   juce::Rectangle<int>& name,
                                   juce::Rectangle<int>& min,
                                   juce::Rectangle<int>& max,
                                   juce::Rectangle<int>& defaultVal);

    // Stesso schema di layoutColumns() ma per le righe Bool: handle + nome
    // (elastico) + default (fisso). Usata sia per BoolParamRow sia per
    // l'intestazione della tab Bool.
    static void layoutBoolColumns (juce::Rectangle<int> area,
                                    juce::Rectangle<int>& handle,
                                    juce::Rectangle<int>& name,
                                    juce::Rectangle<int>& defaultVal);

    // Stesso schema ma per le righe Choice: handle + nome (elastico) +
    // opzioni (fisso, testo libero separato da virgole) + indice di default
    // (fisso). Usata sia per ChoiceParamRow sia per l'intestazione della
    // tab Choice.
    static void layoutChoiceColumns (juce::Rectangle<int> area,
                                      juce::Rectangle<int>& handle,
                                      juce::Rectangle<int>& name,
                                      juce::Rectangle<int>& options,
                                      juce::Rectangle<int>& defaultIndex);

    // Una riga per slot: maniglia di trascinamento "#N", nome canale
    // (TextEditor, larghezza elastica - vedi layoutColumns()), min/max/
    // default/skew/increment (tutti TextEditor). Ogni modifica (Return o
    // focus perso) rilegge subito lo slot corrente dal processor, applica
    // il singolo campo cambiato e lo riscrive - cosi' una modifica a un
    // campo non perde quelle fatte agli altri nel frattempo.
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

        // Pubblico (a differenza di commitFromFields/updateHandleHover):
        // chiamato anche da FloatParamsPage::refreshAllFromProcessor() dopo
        // un Load Session da file, per rileggere lo slot appena ripristinato
        // invece di restare con i vecchi valori mostrati prima del caricamento.
        void refreshFromProcessor();

        // Chiamata dal tasto destro sulla maniglia dopo aver copiato negli
        // appunti (vedi mouseDown) - usata SOLO per dare un riscontro
        // visibile in consolle (CsoundParameterMappingPanel::
        // onParameterCopiedToClipboard -> PluginEditor::appendToLog):
        // senza questo riscontro l'azione e' invisibile (nessun popup,
        // nessun cambio grafico), rendendo impossibile per l'utente
        // distinguere "ha copiato ma non si vede" da "non ha fatto nulla".
        std::function<void (const juce::String&)> onCopiedToClipboard;

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);

        CsoundAudioProcessor& processor;
        int index;

        // Area "#N" a sinistra: non e' un juce::Label separato apposta,
        // cosi' mouseDown/mouseDrag su ParamRow intercettano il click SOLO
        // li' (gli altri controlli della riga, essendo figli, catturano
        // gia' loro i propri eventi mouse). handleHovered (mouseMove/
        // mouseExit) e' solo feedback visivo (evidenziazione + cursore),
        // vedi paint().
        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;

        juce::TextEditor channelNameEditor;
        juce::TextEditor minEditor;
        juce::TextEditor maxEditor;
        juce::TextEditor defaultEditor;
        juce::TextEditor skewEditor;
        juce::TextEditor incrementEditor;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamRow)
    };

    // Una riga per slot intero: maniglia di trascinamento "#N", nome canale
    // (elastico), min/max/default (TextEditor, interi) - stesso schema di
    // ParamRow ma senza skew/increment (gli interi usano sempre una mappatura
    // lineare, vedi CsoundAudioProcessor::denormalizeIntParam).
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

        // Vedi il commento identico su ParamRow::onCopiedToClipboard sopra.
        std::function<void (const juce::String&)> onCopiedToClipboard;

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);

        CsoundAudioProcessor& processor;
        int index;

        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;

        juce::TextEditor channelNameEditor;
        juce::TextEditor minEditor;
        juce::TextEditor maxEditor;
        juce::TextEditor defaultEditor;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntParamRow)
    };

    // Una riga per slot booleano: maniglia di trascinamento "#N", nome
    // canale (elastico) e un toggle per il default on/off - stesso schema
    // di ParamRow ma senza min/max/skew/increment (non ha senso per un on/off).
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

        // Vedi il commento identico su ParamRow::onCopiedToClipboard sopra.
        std::function<void (const juce::String&)> onCopiedToClipboard;

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);

        CsoundAudioProcessor& processor;
        int index;

        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;

        juce::TextEditor channelNameEditor;
        juce::ToggleButton defaultToggle { "On/Off" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoolParamRow)
    };

    // Una riga per slot a scelta multipla: maniglia di trascinamento "#N",
    // nome canale (elastico), opzioni (testo libero separato da virgole -
    // fino a CsoundAudioProcessor::maxChoiceOptions, le eccedenti vengono
    // ignorate, le mancanti ricadono su "Option N") e indice di default.
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

        // Vedi il commento identico su ParamRow::onCopiedToClipboard sopra.
        std::function<void (const juce::String&)> onCopiedToClipboard;

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void updateHandleHover (juce::Point<int> position);

        CsoundAudioProcessor& processor;
        int index;

        juce::Rectangle<int> handleBounds;
        bool draggingFromHandle = false;
        bool handleHovered = false;

        juce::TextEditor channelNameEditor;
        juce::TextEditor optionsEditor;      // etichette separate da virgole
        juce::TextEditor defaultIndexEditor; // indice 0-based, testo libero numerico

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceParamRow)
    };

    // Viewport di una tab di metadata: oltre alla normale vista scrollabile,
    // notifica ogni scorrimento ORIZZONTALE (onHorizontalScrollChanged).
    // Serve perche' l'intestazione delle colonne (headerNameLabel/
    // headerMinLabel/ecc.) e' disegnata FUORI dal viewport apposta, per
    // restare fissa durante lo scroll VERTICALE delle righe - ma da quando
    // il contenuto ha una larghezza MINIMA (vedi minContentWidth sotto,
    // introdotto su richiesta esplicita: il pannello non deve piu'
    // schiacciare/nascondere le colonne quando ridimensionato piu' stretto
    // del necessario, deve comparire una scrollbar orizzontale invece)
    // puo' essere piu' largo del viewport visibile, quindi serve anche
    // scorrere ORIZZONTALMENTE - e in quel caso l'intestazione deve
    // spostarsi in sincrono, altrimenti smette di allinearsi alle colonne
    // sotto. Vedi *ParamsPage::layoutHeaderForScroll().
    struct ScrollSyncedViewport final : public juce::Viewport
    {
        std::function<void (int)> onHorizontalScrollChanged;

        void visibleAreaChanged (const juce::Rectangle<int>& newVisibleArea) override
        {
            juce::Viewport::visibleAreaChanged (newVisibleArea);

            if (onHorizontalScrollChanged)
                onHorizontalScrollChanged (newVisibleArea.getX());
        }
    };

    // Una "pagina" di tab: intestazione colonne (fissa in alto) + viewport
    // scrollabile con SOLO le righe di un tipo di parametro. Le quattro
    // istanze sotto (una per tab: Float, Int, Bool, Choice) isolano
    // completamente i quattro elenchi, invece di impilarli in un'unica
    // lista lunghissima dove non si distingueva piu' un tipo dall'altro.
    struct FloatParamsPage final : public juce::Component
    {
        explicit FloatParamsPage (CsoundAudioProcessor& processorToEdit);
        void resized() override;

        // Richiamata da CsoundParameterMappingPanel::refreshAllFromProcessor()
        // dopo un Load Session da file: rilegge ogni riga dallo stato appena
        // ripristinato nel processor, invece di lasciare visibili i vecchi
        // valori della sessione precedente.
        void refreshAllFromProcessor();

        // Agganciata riga per riga ad ogni ParamRow::onCopiedToClipboard nel
        // costruttore (vedi .cpp) - CsoundParameterMappingPanel imposta
        // questo per inoltrare al proprio onParameterCopiedToClipboard.
        std::function<void (const juce::String&)> onParameterCopiedToClipboard;

        // Larghezza minima del contenuto (handle + nome minimo + min/max/
        // default/skew/increment + margini, stessa formula di
        // CsoundParameterMappingPanel::preferredWidth) - sotto questa
        // larghezza il viewport mostra una scrollbar orizzontale invece di
        // schiacciare le colonne (vedi resized()).
        static constexpr int minContentWidth = handleWidth + nameWidth + minMaxWidth * 2 + defaultWidth + curveWidth + 4 + 6 + 6 + 6 + 6 + 8;

    private:
        // Riposiziona le etichette di intestazione in base allo scroll
        // orizzontale corrente del viewport (vedi ScrollSyncedViewport sopra)
        // - fattorizzato qui per essere chiamato sia da resized() sia dal
        // callback onHorizontalScrollChanged, invece di duplicare la stessa
        // logica in due posti.
        void layoutHeaderForScroll (int scrollX);

        ScrollSyncedViewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerMinLabel, headerMaxLabel, headerDefaultLabel, headerSkewLabel, headerIncrementLabel;
        std::array<std::unique_ptr<ParamRow>, (size_t) CsoundAudioProcessor::numChannelParams> rows;

        // Base (a scrollX=0) dell'area di intestazione, calcolata in
        // resized() e riusata da layoutHeaderForScroll() ad ogni scroll.
        juce::Rectangle<int> headerAreaBase;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatParamsPage)
    };

    struct IntParamsPage final : public juce::Component
    {
        explicit IntParamsPage (CsoundAudioProcessor& processorToEdit);
        void resized() override;
        void refreshAllFromProcessor();

        std::function<void (const juce::String&)> onParameterCopiedToClipboard;

        // Vedi il commento identico su FloatParamsPage::minContentWidth sopra.
        static constexpr int minContentWidth = handleWidth + nameWidth + minMaxWidth * 2 + defaultWidth + 4 + 6 + 6 + 6 + 8;

    private:
        void layoutHeaderForScroll (int scrollX);

        ScrollSyncedViewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerMinLabel, headerMaxLabel, headerDefaultLabel;
        std::array<std::unique_ptr<IntParamRow>, (size_t) CsoundAudioProcessor::numIntParams> rows;
        juce::Rectangle<int> headerAreaBase;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntParamsPage)
    };

    struct BoolParamsPage final : public juce::Component
    {
        explicit BoolParamsPage (CsoundAudioProcessor& processorToEdit);
        void resized() override;
        void refreshAllFromProcessor();

        std::function<void (const juce::String&)> onParameterCopiedToClipboard;

        // Vedi il commento identico su FloatParamsPage::minContentWidth sopra.
        static constexpr int minContentWidth = handleWidth + nameWidth + boolDefaultWidth + 4 + 6 + 8;

    private:
        void layoutHeaderForScroll (int scrollX);

        ScrollSyncedViewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerDefaultLabel;
        std::array<std::unique_ptr<BoolParamRow>, (size_t) CsoundAudioProcessor::numBoolParams> rows;
        juce::Rectangle<int> headerAreaBase;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoolParamsPage)
    };

    struct ChoiceParamsPage final : public juce::Component
    {
        explicit ChoiceParamsPage (CsoundAudioProcessor& processorToEdit);
        void resized() override;
        void refreshAllFromProcessor();

        std::function<void (const juce::String&)> onParameterCopiedToClipboard;

        // Vedi il commento identico su FloatParamsPage::minContentWidth sopra.
        static constexpr int minContentWidth = handleWidth + nameWidth + choiceOptionsWidth + choiceDefaultIndexWidth + 4 + 6 + 6 + 8;

    private:
        void layoutHeaderForScroll (int scrollX);

        ScrollSyncedViewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerOptionsLabel, headerDefaultLabel;
        std::array<std::unique_ptr<ChoiceParamRow>, (size_t) CsoundAudioProcessor::numChoiceParams> rows;
        juce::Rectangle<int> headerAreaBase;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceParamsPage)
    };

    // Quinta tab, "UI"/Generic Editor: a differenza delle quattro sopra (che
    // editano i METADATA per slot) mostra/automatizza i VALORI correnti dei
    // parametri host - uno slider orizzontale per ogni Float/Int, un toggle
    // per ogni Bool, un combo per ogni Choice, "esattamente come l'editor
    // generico di JUCE" (juce::GenericAudioProcessorEditor), ma con due
    // differenze volute rispetto a usare quella classe direttamente:
    //   1) mostra SOLO gli slot con un canale Csound assegnato (channelName
    //      non vuoto) - gli altri non sono "parametri validi" da esporre
    //      qui, mostrarli tutti e 64 confondeva solamente;
    //   2) ogni controllo e' legato al parametro apvts reale tramite le
    //      classi di attachment "ufficiali" di JUCE (vedi GenericParamRow
    //      sotto), non la logica privata/interna di GenericAudioProcessorEditor.
    // Era in precedenza una finestra flottante separata (GenericEditorWindow
    // in PluginEditor.h/.cpp): spostata qui come QUINTA tab del pannello
    // Parameters (dopo Choice) su richiesta esplicita - una view a se stante
    // separata dalle altre quattro non aveva piu' senso, essendo comunque
    // un'altra vista sugli stessi 64 slot.
    struct GenericParamRow final : public juce::Component
    {
        enum class Kind { slider, toggle, choice };

        GenericParamRow (const juce::String& channelName, juce::RangedAudioParameter& parameter,
                          Kind kind, const juce::StringArray& choiceLabels);

        void resized() override;

    private:
        // Larghezza FISSA (uguale per ogni riga, cosi' i controlli sotto
        // restano tutti allineati alla stessa X - vedi resized()) per il
        // nome canale: ridotta da 130 (lasciava troppo spazio vuoto prima
        // dello slider/combo quando il nome e' corto, richiesto
        // esplicitamente) - tutto il resto della riga va al controllo.
        static constexpr int nameLabelWidth = 90;

        juce::Label nameLabel;

        // TextBoxRight: casella di testo INTERNA di Slider, EDITABILE (di
        // serie - Slider::isTextBoxEditable() e' true di default), cosi'
        // il valore si puo' anche scrivere a mano, non solo trascinare -
        // richiesto esplicitamente. I colori (testo bianco, sfondo scuro)
        // sono impostati DIRETTAMENTE sull'istanza nel costruttore, che
        // vincono comunque su qualunque LookAndFeel.
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::ToggleButton toggle;
        juce::ComboBox comboBox;
        const Kind kind;

        // Dichiarati DOPO i widget che referenziano: distrutti PRIMA di
        // essi (ordine inverso di dichiarazione) - un attachment vivo che
        // referenzia un widget gia' distrutto sarebbe un puntatore pendente.
        std::unique_ptr<juce::SliderParameterAttachment> sliderAttachment;
        std::unique_ptr<juce::ButtonParameterAttachment> buttonAttachment;
        std::unique_ptr<juce::ComboBoxParameterAttachment> comboAttachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenericParamRow)
    };

    struct GenericEditorPage final : public juce::Component
    {
        GenericEditorPage();

        void resized() override;

        // Ricostruisce da zero l'elenco delle righe leggendo lo stato
        // ATTUALE degli slot (getChannelParamSlot/getIntParamSlot/
        // getBoolParamSlot/getChoiceParamSlot) - chiamata da showPage() ogni
        // volta che questa tab diventa quella corrente, cosi' un canale
        // rinominato/assegnato nelle altre tab si riflette qui subito,
        // invece di restare congelato alla prima apertura. Se nessuno slot
        // ha un canale assegnato, nasconde il viewport e mostra invece
        // emptyStateLabel al centro - vedi il commento in testa alla classe.
        void refreshRows (CsoundAudioProcessor& processor);

    private:
        static constexpr int genericRowHeight = 34;

        // Vedi il commento su FloatParamsPage::minContentWidth - stessa idea,
        // ma qui non c'e' un'intestazione separata da tenere allineata (ogni
        // riga e' gia' autonoma: nome + un solo controllo), quindi basta
        // impedire che il contenuto scenda sotto una larghezza leggibile.
        // 90 = GenericParamRow::nameLabelWidth (privato li', duplicato qui
        // come letterale per evitare di doverlo esporre solo per questo).
        static constexpr int minContentWidth = 90 + 8 + 180;

        juce::Viewport viewport;
        juce::Component rowsContainer;
        std::vector<std::unique_ptr<GenericParamRow>> rows;

        // Mostrata SOLO quando rows e' vuoto (nessun parametro ancora
        // configurato nelle tab Float/Int/Bool/Choice) - testo centrato che
        // invita a configurare prima i parametri, invece di un pannello
        // vuoto senza spiegazione.
        juce::Label emptyStateLabel;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenericEditorPage)
    };

    // Riferimento al processor, tenuto per poter richiamare
    // GenericEditorPage::refreshRows() ogni volta che la tab "UI" diventa
    // quella corrente (vedi showPage()) - una semplice reference, nessun
    // problema di ordine di costruzione/distruzione.
    CsoundAudioProcessor& processor;

    // Dichiarata PER PRIMA tra i membri sotto: i membri si distruggono
    // nell'ordine INVERSO a quello di dichiarazione, quindi essendo la
    // prima e' anche l'ULTIMA a essere distrutta - lookAndFeel resta valida
    // per tutta la vita di tabs/pagine/righe, mai un puntatore a LookAndFeel
    // pendente durante lo smontaggio dell'albero di componenti
    // (setLookAndFeel(nullptr) nel distruttore stacca comunque subito il
    // collegamento, per sicurezza).
    CsoundParameterPanelLookAndFeel lookAndFeel;

    // Barra tab fatta a mano (4 TextButton + switch diretto di visibilita'
    // tra le 4 pagine), al posto di juce::TabbedComponent/TabbedButtonBar:
    // quest'ultimo ha un bug di layout (TabbedButtonBar::updateTabPositions,
    // vedi juce_TabbedButtonBar.cpp) che ricalcola quali tab mostrare ogni
    // volta che cambia quella corrente e, in certe condizioni di scala,
    // nasconde (setVisible(false)) tutte le tab tranne quella appena
    // selezionata - riproducibile anche dopo aver alzato setMinimumTabScale
    // Factor quasi a zero. Niente di tutto questo con 4 bottoni gestiti
    // interamente da noi: showPage()/updateTabButtonStyles() sono le uniche
    // funzioni che decidono cosa e' visibile, nessuna euristica nascosta.
    // Quinta tab "UI" (Generic Editor, vedi GenericEditorPage sopra): nome
    // impostato a "genericEditorTab" nel .cpp, cosi' CsoundParameterPanelLook
    // AndFeel::drawButtonText sa di doverci disegnare sopra anche l'icona -
    // la STESSA "tune" del bottone Parameters nella toolbar.
    juce::TextButton floatTabButton  { "Float" };
    juce::TextButton intTabButton    { "Int" };
    juce::TextButton boolTabButton   { "Bool" };
    juce::TextButton choiceTabButton { "Choice" };
    juce::TextButton genericEditorTabButton { "UI" };

    int currentPageIndex = 0;
    void showPage (int pageIndex);
    void updateTabButtonStyles();

    // {bottone, indice di pagina} nell'ordine VISIVO sinistra->destra -
    // unica fonte di verita' per quell'ordine, usata dal costruttore,
    // updateTabButtonStyles() e resized() invece di tre elenchi scritti a
    // mano da tenere sincronizzati.
    std::array<std::pair<juce::TextButton*, int>, 5> getOrderedTabs();

    std::unique_ptr<FloatParamsPage> floatPage;
    std::unique_ptr<IntParamsPage> intPage;
    std::unique_ptr<BoolParamsPage> boolPage;
    std::unique_ptr<ChoiceParamsPage> choicePage;
    std::unique_ptr<GenericEditorPage> genericEditorPage;

    static constexpr int rowHeight = 30;
    static constexpr int headerHeight = 24;
    static constexpr int tabBarHeight = 28;

    // Spazio riservato SEMPRE (indipendentemente dal fatto che la scrollbar
    // verticale del viewport sia davvero visibile in quel momento) sul lato
    // destro di ogni pagina, per il contenuto (rowsContainer/intestazione
    // colonne): viewport.getMaximumVisibleWidth() cambia dinamicamente a
    // seconda che la scrollbar sia mostrata o no, e in certe sequenze di
    // layout il contenuto veniva misurato PRIMA che la scrollbar comparisse,
    // finendo per essere troppo largo e "infilarsi sotto" la scrollbar
    // quando poi compariva (il bug "margine a destra/overlap con la
    // scrollbar" segnalato). Riservare uno spazio fisso elimina il
    // problema a prescindere dal timing.
    static constexpr int scrollbarGutter = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundParameterMappingPanel)
};
