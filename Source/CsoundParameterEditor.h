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
    Finestra flottante (overlay DENTRO la view del plugin, non una
    juce::DocumentWindow separata - vedi il commento in testa a
    CsoundAudioProcessorEditor in PluginEditor.h sul perche') per definire,
    per ciascuno dei 16 parametri host float (CsoundAudioProcessor::
    ChannelParamSlot), dei 16 interi (IntParamSlot), dei 16 booleani
    (BoolParamSlot) e dei 16 a scelta multipla (ChoiceParamSlot), il canale
    Csound a cui sono assegnati ("rename" - il parametro apvts resta sempre
    "Float N"/"Int N"/"Bool N"/"Choice N" per l'host) e i relativi metadata
    (range/default/curva per i float, range/default per gli interi, default
    on/off per i bool, etichette/indice di default per i choice).

    I quattro tipi sono isolati in QUATTRO TAB separate, nell'ordine Float,
    Int, Bool, Choice (FloatParamsPage/IntParamsPage/BoolParamsPage/
    ChoiceParamsPage, vedi sotto), non impilati in un'unica lista
    scorrevole: con tutte le righe insieme non si distingueva piu' un tipo
    dall'altro.

    E' una finestra "a se'" dentro i confini del plugin: barra del titolo con
    pulsante di chiusura (onCloseButtonClicked, agganciato da PluginEditor),
    bordo/ombra disegnati in paint(). PluginEditor la mostra come overlay
    centrato sopra il resto dell'editor (codice/consolle restano visibili
    sotto, con un velo semitrasparente dietro per il focus) invece di farla
    sostituire la consolle come in una versione precedente.

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

    // Finestra spostabile: trascinando la barra del titolo (non un overlay
    // fisso bloccato al centro) si sposta dentro i confini del genitore
    // (l'editor del plugin) - vedi titleBarHeight/titleLabel sotto.
    // titleLabel ha setInterceptsMouseClicks(false,...) apposta, cosi' il
    // click sopra di essa arriva comunque qui invece di fermarsi li'.
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

    // Chiamata quando l'utente preme la X nella barra del titolo - PluginEditor
    // la usa per richiudere l'overlay (vedi toggleParameterPanel() in
    // PluginEditor.cpp). Il pannello stesso non decide mai di nascondersi da
    // solo: lascia all'editor la responsabilita' di gestire overlay/velo.
    std::function<void()> onCloseButtonClicked;

    // Larghezze fisse delle colonne handle/min/max/default/curva (vedi
    // layoutColumns()); nameWidth e' solo la larghezza MINIMA del campo
    // nome, usata per calcolare preferredWidth - il campo stesso si allarga
    // con la finestra. preferredWidth/preferredHeight sono il riferimento
    // "naturale" che PluginEditor usa per dimensionare l'overlay (vedi
    // CsoundAudioProcessorEditor::resized()).
    static constexpr int handleWidth = 30;
    static constexpr int nameWidth = 108;
    static constexpr int minMaxWidth = 80;
    static constexpr int defaultWidth = 80;
    static constexpr int curveWidth = 130;
    static constexpr int preferredWidth = handleWidth + nameWidth + minMaxWidth * 2 + defaultWidth + curveWidth + 10 + 16 + 16 + 10;

    // Abbastanza alto da mostrare tutte le 16 righe di qualunque tab (Float/
    // Int/Bool/Choice) SENZA scroll verticale (titleBarHeight + tabBarHeight
    // + margini pagina/header + 16*rowHeight, con un margine extra) - vedi
    // il commento sul perche' in CsoundParameterMappingPanel::resized().
    static constexpr int preferredHeight = 620;

    static constexpr int boolDefaultWidth = 70;
    static constexpr int choiceOptionsWidth = 220;
    static constexpr int choiceDefaultIndexWidth = 50;

private:
    // Divide "area" nelle stesse 6 colonne (handle/nome/min/max/default/
    // curva) sia per l'intestazione sia per ogni ParamRow, cosi' le
    // etichette nell'intestazione restano SEMPRE allineate ai campi sotto -
    // un'unica fonte di verita' per il layout, invece di ricalcolarlo due
    // volte con margini separati che potrebbero disallinearsi. handle/min/
    // max/default/curva hanno larghezza FISSA; il nome prende tutto lo
    // spazio che resta (fino al campo min), quindi l'intera riga scala con
    // la larghezza di "area".
    static void layoutColumns (juce::Rectangle<int> area,
                                juce::Rectangle<int>& handle,
                                juce::Rectangle<int>& name,
                                juce::Rectangle<int>& min,
                                juce::Rectangle<int>& max,
                                juce::Rectangle<int>& defaultVal,
                                juce::Rectangle<int>& curve);

    // Stesso schema di layoutColumns() ma per le righe Int: handle + nome
    // (elastico) + min/max/default (fissi), senza colonna curva (sempre
    // lineare per gli interi). Usata sia per IntParamRow sia per
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
    // default (TextEditor) e curva (ComboBox). Ogni modifica (Return, focus
    // perso, o selezione nel combo) rilegge subito lo slot corrente dal
    // processor, applica il singolo campo cambiato e lo riscrive - cosi'
    // una modifica a un campo non perde quelle fatte agli altri nel
    // frattempo.
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
        juce::ComboBox curveCombo;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamRow)
    };

    // Una riga per slot intero: maniglia di trascinamento "#N", nome canale
    // (elastico), min/max/default (TextEditor, interi) - stesso schema di
    // ParamRow ma senza combo curva (gli interi usano sempre una mappatura
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
    // di ParamRow ma senza min/max/curva (non ha senso per un on/off).
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

    private:
        juce::Viewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerMinLabel, headerMaxLabel, headerDefaultLabel, headerCurveLabel;
        std::array<std::unique_ptr<ParamRow>, (size_t) CsoundAudioProcessor::numChannelParams> rows;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatParamsPage)
    };

    struct IntParamsPage final : public juce::Component
    {
        explicit IntParamsPage (CsoundAudioProcessor& processorToEdit);
        void resized() override;
        void refreshAllFromProcessor();

    private:
        juce::Viewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerMinLabel, headerMaxLabel, headerDefaultLabel;
        std::array<std::unique_ptr<IntParamRow>, (size_t) CsoundAudioProcessor::numIntParams> rows;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IntParamsPage)
    };

    struct BoolParamsPage final : public juce::Component
    {
        explicit BoolParamsPage (CsoundAudioProcessor& processorToEdit);
        void resized() override;
        void refreshAllFromProcessor();

    private:
        juce::Viewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerDefaultLabel;
        std::array<std::unique_ptr<BoolParamRow>, (size_t) CsoundAudioProcessor::numBoolParams> rows;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoolParamsPage)
    };

    struct ChoiceParamsPage final : public juce::Component
    {
        explicit ChoiceParamsPage (CsoundAudioProcessor& processorToEdit);
        void resized() override;
        void refreshAllFromProcessor();

    private:
        juce::Viewport viewport;
        juce::Component rowsContainer;
        juce::Label headerNameLabel, headerOptionsLabel, headerDefaultLabel;
        std::array<std::unique_ptr<ChoiceParamRow>, (size_t) CsoundAudioProcessor::numChoiceParams> rows;

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
        // Larghezza FISSA e minima per il nome canale (quasi sempre corto):
        // tutto il resto della riga va al controllo - vedi resized(), che
        // massimizza lo spazio del controllo (in particolare dello slider)
        // invece del nome.
        static constexpr int nameLabelWidth = 130;

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

    // Barra del titolo della "finestra" flottante: etichetta + pulsante di
    // chiusura (onCloseButtonClicked).
    juce::Label titleLabel;
    juce::TextButton closeButton { "Close" };

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

    // Trascinamento della barra del titolo (vedi mouseDown/mouseDrag sopra).
    juce::ComponentDragger titleBarDragger;
    bool draggingTitleBar = false;

    static constexpr int rowHeight = 30;
    static constexpr int headerHeight = 24;
    static constexpr int titleBarHeight = 34;
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
