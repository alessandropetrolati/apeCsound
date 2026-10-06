#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
    LookAndFeel dedicata al pannello parametri (CsoundParameterMappingPanel):
    tema scuro petrolio/teal con bordi arrotondati, "glow" accentato sui
    campi in focus/hover e un menu a tendina coerente - sostituisce i soli
    setColour() per-componente della versione precedente con un vero
    ridisegno. Applicata con setLookAndFeel() SOLO sul pannello (si propaga
    ai figli finche' non trovano un'altra LookAndFeel esplicita): il resto
    del plugin resta sul tema chiaro di CsoundLookAndFeel, invariato.
*/
class CsoundParameterPanelLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CsoundParameterPanelLookAndFeel();

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
};

/**
    Pannello per definire, per ciascuno dei 32 "macro" parametri host (vedi
    CsoundAudioProcessor::ChannelParamSlot), il canale Csound a cui e'
    assegnato ("rename" - il parametro apvts resta sempre "Param N" per
    l'host, qui si da' il nome VERO, es. "freq"), il range reale (min/max),
    il valore di default (in unita' reali, come min/max - usato da
    ChannelHostParameter::getDefaultValue per il "reset to default"
    dell'host) e la curva (lineare/esponenziale/logaritmica - vedi
    CsoundAudioProcessor::ChannelParamCurve) con cui il valore normalizzato
    0..1 dell'host viene convertito prima di scriverlo nel canale (vedi
    CsoundAudioProcessor::denormalizeChannelParam).

    Non mostra/non crea slider per i VALORI correnti: quelli restano
    affidati al meccanismo automatico di JUCE (juce::GenericAudioProcessor
    Editor, agganciato a CsoundAudioProcessor::apvts) o a una skin dedicata
    in futuro - questo pannello riguarda solo i metadata per slot.

    Ogni riga e' anche una sorgente di drag and drop (vedi ParamRow::
    mouseDrag): trascinando l'area "#N" sull'editor di codice (CsoundCodeEditor,
    che implementa DragAndDropTarget) e rilasciando su una riga, viene
    inserito automaticamente un chnget per quel canale - vedi il commento in
    testa a CsoundCodeEditor.h per il formato esatto.

    Occupa l'intera larghezza del plugin (sostituisce la consolle, vedi
    PluginEditor): handle/min/max/curva hanno larghezza fissa, il campo nome
    canale si allarga/restringe per occupare tutto lo spazio restante -
    scala quindi automaticamente con la larghezza della finestra, vedi
    layoutColumns().
*/
class CsoundParameterMappingPanel final : public juce::Component
{
public:
    explicit CsoundParameterMappingPanel (CsoundAudioProcessor& processorToEdit);
    ~CsoundParameterMappingPanel() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    // Larghezze fisse delle colonne handle/min/max/default/curva (vedi
    // layoutColumns()); nameWidth e' solo la larghezza MINIMA del campo
    // nome, usata per calcolare preferredWidth - il campo stesso si
    // allarga con la finestra (vedi layoutColumns()).
    static constexpr int handleWidth = 30;
    static constexpr int nameWidth = 108;
    static constexpr int minMaxWidth = 80;
    static constexpr int defaultWidth = 80;
    static constexpr int preferredWidth = handleWidth + nameWidth + minMaxWidth * 2 + defaultWidth + 10 + 16 + 16 + 10;

private:
    // Divide "area" nelle stesse 5 colonne (handle/nome/min/max/default) sia
    // per l'intestazione sia per ogni ParamRow, cosi' le etichette
    // nell'intestazione restano SEMPRE allineate ai campi sotto - un'unica
    // fonte di verita' per il layout, invece di ricalcolarlo due volte con
    // margini separati che potrebbero disallinearsi. handle/min/max/default
    // hanno larghezza FISSA; il nome prende tutto lo spazio che resta (fino
    // al campo min), quindi l'intera riga scala con la larghezza di "area".
    // Niente piu' colonna/combo "Curve": rimossa dal pannello su richiesta -
    // la curva resta internamente sempre lineare (vedi ParamRow::
    // commitFromFields), il campo/enum lato processor non e' stato toccato
    // nel caso serva reintrodurla in futuro.
    static void layoutColumns (juce::Rectangle<int> area,
                                juce::Rectangle<int>& handle,
                                juce::Rectangle<int>& name,
                                juce::Rectangle<int>& min,
                                juce::Rectangle<int>& max,
                                juce::Rectangle<int>& defaultVal);
    // Una riga per slot: maniglia di trascinamento "#N", nome canale
    // (TextEditor, larghezza elastica - vedi layoutColumns()), min/max/
    // default (TextEditor). Ogni modifica (Return o focus perso) rilegge
    // subito lo slot corrente dal processor, applica il singolo campo
    // cambiato e lo riscrive - cosi' una modifica a un campo non perde
    // quelle fatte agli altri nel frattempo.
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

    private:
        void textEditorReturnKeyPressed (juce::TextEditor&) override;
        void textEditorFocusLost (juce::TextEditor&) override;
        void textEditorTextChanged (juce::TextEditor&) override;

        void commitFromFields();
        void refreshFromProcessor();
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

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamRow)
    };

    // Dichiarata PER PRIMA tra i membri sotto: i membri si distruggono
    // nell'ordine INVERSO a quello di dichiarazione, quindi essendo la
    // prima e' anche l'ULTIMA a essere distrutta - lookAndFeel resta valida
    // per tutta la vita di viewport/rowsContainer/header/righe, mai un
    // puntatore a LookAndFeel pendente durante lo smontaggio dell'albero di
    // componenti (setLookAndFeel(nullptr) nel distruttore stacca comunque
    // subito il collegamento, per sicurezza).
    CsoundParameterPanelLookAndFeel lookAndFeel;

    juce::Viewport viewport;
    juce::Component rowsContainer;

    // Un juce::Label per colonna (non una sola stringa con spazi a mano:
    // non restava allineata ai campi sotto) - vedi layoutColumns().
    juce::Label headerNameLabel, headerMinLabel, headerMaxLabel, headerDefaultLabel;

    std::array<std::unique_ptr<ParamRow>, (size_t) CsoundAudioProcessor::numChannelParams> rows;

    // Y della linea accentata sotto l'header (vedi paint()), aggiornata da
    // resized(); 0 finche' resized() non e' mai stato chiamato.
    int headerBottomY = 0;

    static constexpr int rowHeight = 30;
    static constexpr int headerHeight = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsoundParameterMappingPanel)
};
