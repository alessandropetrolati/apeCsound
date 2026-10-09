#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

/**
    Editor di codice scritto da zero in JUCE puro: sostituisce
    juce::CodeEditorComponent, che e' pensato per il mouse (scroll a righe
    intere, nessun supporto touch) e si e' rivelato scadente anche su
    desktop. Un'unica implementazione per macOS, iOS, Windows e Linux, con
    l'aspetto e i menu dell'app ovunque.

    Modello: juce::CodeDocument (testo + cronologia di undo) e
    juce::CodeTokeniser (colorazione) restano quelli di JUCE, cosi' tutto
    cio' che ci lavora sopra (undo condiviso, Find/Replace, help degli
    opcode, UDO, drop dei parametri, Indent/Comment in CsoundCodeEditor)
    non cambia.
*/
class CodeView : public juce::Component,
                 public juce::TextInputTarget,
                 private juce::ScrollBar::Listener,
                 private juce::AnimatedPosition<juce::AnimatedPositionBehaviours::ContinuousWithMomentum>::Listener
{
public:
    using ColourScheme = juce::CodeEditorComponent::ColourScheme;

    enum ColourIds
    {
        backgroundColourId       = juce::CodeEditorComponent::backgroundColourId,
        highlightColourId        = juce::CodeEditorComponent::highlightColourId,
        defaultTextColourId      = juce::CodeEditorComponent::defaultTextColourId,
        lineNumberBackgroundId   = juce::CodeEditorComponent::lineNumberBackgroundId,
        lineNumberTextId         = juce::CodeEditorComponent::lineNumberTextId,
        caretColourId            = juce::CaretComponent::caretColourId
    };

    CodeView (juce::CodeDocument& document, juce::CodeTokeniser* tokeniser);
    ~CodeView() override;

    //==========================================================================
    juce::CodeDocument& getDocument() const noexcept        { return document; }

    void setFont (const juce::Font& newFont);
    const juce::Font& getFont() const noexcept              { return font; }

    void setColourScheme (const ColourScheme& scheme);
    const ColourScheme& getColourScheme() const noexcept    { return colourScheme; }

    /** Tab = spazi (sempre insertSpaces nel nostro caso). */
    void setTabSize (int numSpaces, bool insertSpaces);
    int getTabSize() const noexcept                         { return tabSize; }

    void setLineNumbersShown (bool shouldBeShown);
    void setReadOnly (bool shouldBeReadOnly)                { readOnly = shouldBeReadOnly; }
    bool isReadOnly() const noexcept                        { return readOnly; }

    //==========================================================================
    // Caret e selezione (indici in code point come CodeDocument)
    juce::CodeDocument::Position getCaretPos() const        { return caretPos; }
    juce::CodeDocument::Position getSelectionStart() const;
    juce::CodeDocument::Position getSelectionEnd() const;
    bool isHighlightActive() const;

    void moveCaretTo (const juce::CodeDocument::Position& newPos, bool selecting);
    void selectRegion (const juce::CodeDocument::Position& start, const juce::CodeDocument::Position& end);
    bool selectAll();  // bool: richiesto da juce::TextEditorKeyMapper
    void deselectAll();

    //==========================================================================
    // juce::TextInputTarget: e' cio' che fa comparire la tastiera a schermo
    // su iOS (il peer la apre solo se il componente focalizzato e' un
    // TextInputTarget attivo) e che collega IME/dettatura/accessibilita'.
    // getCaretRectangle() (non virtuale, della base) = rettangolo del caret.
    bool isTextInputActive() const override                         { return ! readOnly && ! textInputSuspended; }
    juce::Range<int> getHighlightedRegion() const override;
    void setHighlightedRegion (const juce::Range<int>& newRange) override;
    void setTemporaryUnderlining (const juce::Array<juce::Range<int>>& underlinedRegions) override;
    juce::String getTextInRange (const juce::Range<int>& range) const override;
    void insertTextAtCaret (const juce::String& text) override;
    int getCaretPosition() const override                           { return caretPos.getPosition(); }
    juce::Rectangle<int> getCaretRectangleForCharIndex (int characterIndex) const override;
    int getTotalNumChars() const override                           { return document.getNumCharacters(); }
    int getCharIndexForPoint (juce::Point<int> point) const override;
    juce::RectangleList<int> getTextBounds (juce::Range<int> textRange) const override;

    //==========================================================================
    // Editing
    void insertTabAtCaret();
    bool deleteSelection();           // true se c'era qualcosa da cancellare
    bool cutToClipboard();  // bool: richiesto da juce::TextEditorKeyMapper
    bool copyToClipboard();  // bool: richiesto da juce::TextEditorKeyMapper
    bool pasteFromClipboard();  // bool: richiesto da juce::TextEditorKeyMapper
    bool undo();  // bool: richiesto da juce::TextEditorKeyMapper
    bool redo();  // bool: richiesto da juce::TextEditorKeyMapper

    // Usate da juce::TextEditorKeyMapper (nomi obbligati)
    bool moveCaretLeft (bool moveInWholeWordSteps, bool selecting);
    bool moveCaretRight (bool moveInWholeWordSteps, bool selecting);
    bool moveCaretUp (bool selecting);
    bool moveCaretDown (bool selecting);
    bool moveCaretToStartOfLine (bool selecting);
    bool moveCaretToEndOfLine (bool selecting);
    bool moveCaretToTop (bool selecting);
    bool moveCaretToEnd (bool selecting);
    bool pageUp (bool selecting);
    bool pageDown (bool selecting);
    bool deleteBackwards (bool moveInWholeWordSteps);
    bool deleteForwards (bool moveInWholeWordSteps);
    bool scrollUp();
    bool scrollDown();

    /** Invio: default = a capo con la stessa indentazione della riga. */
    virtual void handleReturnKey();
    /** Tab: indent delle righe selezionate, o spazi fino alla tabulazione. */
    virtual void handleTabKey();
    /** Esc: default non fa nulla (false = non consumato). */
    virtual bool handleEscapeKey()                           { return false; }

    void indentSelection();
    void unindentSelection();

    //==========================================================================
    // Geometria e scroll (pixel, continuo)
    int getLineHeight() const noexcept                      { return lineHeight; }
    float getCharWidth() const noexcept                     { return charWidth; }
    int getGutterWidth() const noexcept                     { return gutterWidth; }

    /** Prima riga (anche parzialmente) visibile. */
    int getFirstLineOnScreen() const;
    /** Numero di righe che entrano nell'area di testo (arrotondato in su). */
    int getNumLinesOnScreen() const;

    juce::Rectangle<int> getCharacterBounds (const juce::CodeDocument::Position& pos) const;
    juce::CodeDocument::Position getPositionAt (int x, int y) const;

    juce::Point<double> getScrollPosition() const noexcept  { return { scrollX, scrollY }; }
    void setScrollPosition (double newX, double newY);
    void scrollToLine (int line);               // riga in cima
    void scrollBy (int deltaLines);
    void scrollToColumn (int column);
    void scrollToKeepCaretOnScreen();

    //==========================================================================
    // Hook per le sottoclassi
    /** Tasto destro, ctrl-click, rilascio dopo una selezione touch, tap su
        una selezione. localPos = punto in coordinate del componente. */
    virtual void showContextMenu (juce::Point<int> localPos)  { juce::ignoreUnused (localPos); }
    virtual void caretPositionMoved() {}
    virtual void editorViewportPositionChanged() {}

    // [EDIT-CALLOUT] Menu di editing per il TOUCH (rilascio dopo pressione
    // prolungata/maniglie, tap sulla selezione): default = lo stesso menu
    // contestuale; CsoundCodeEditor lo sostituisce con il callout non
    // modale (CsoundEditCallout). hideTouchEditMenu viene chiamato quando
    // selezione, scroll, testo o focus cambiano. Per rimuovere il callout
    // basta cancellare questi due hook e le loro chiamate (cercare il tag).
    virtual void showTouchEditMenu (juce::Rectangle<int> selectionArea)   { showContextMenu (selectionArea.getPosition()); }
    virtual void hideTouchEditMenu() {}

    //==========================================================================
    // juce::Component
    void paint (juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void colourChanged() override                           { repaint(); }
    bool keyPressed (const juce::KeyPress& key) override;
    void focusGained (FocusChangeType cause) override;
    void focusLost (FocusChangeType cause) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;

private:
    //==========================================================================
    using DragPosition = juce::AnimatedPosition<juce::AnimatedPositionBehaviours::ContinuousWithMomentum>;

    // Timer "a funzione" (servono tre timer indipendenti: lampeggio del
    // caret, pressione prolungata, auto-scroll durante la selezione).
    struct CallbackTimer final : public juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override { if (onTick) onTick(); }
    };

    struct LineTokens
    {
        struct Segment { int type; int start, end; }; // indici nella riga [start, end)
        std::vector<Segment> segments;
        bool valid = false;
    };

    enum class Gesture
    {
        none,
        mouseSelecting,        // drag col mouse: estende la selezione
        touchUndecided,        // dito giu', ancora ne' scroll ne' selezione
        touchScrolling,        // pan con un dito
        touchSelecting,        // dopo la pressione prolungata: trascina la selezione
        touchHandleDragging,   // trascina una maniglia della selezione
        touchCaretDragging     // pressione prolungata con la tastiera aperta: sposta il caret (lente, nessuna selezione)
    };

    // Listener del documento come OGGETTO membro, non come classe base:
    // le sottoclassi (CsoundCodeEditor) sono a loro volta
    // CodeDocument::Listener e l'ereditarieta' doppia renderebbe ambigua la
    // conversione a Listener.
    struct DocumentListener final : public juce::CodeDocument::Listener
    {
        explicit DocumentListener (CodeView& o) : owner (o) {}
        void codeDocumentTextInserted (const juce::String& newText, int insertIndex) override;
        void codeDocumentTextDeleted (int startIndex, int endIndex) override;
        CodeView& owner;
    };

    // juce::ScrollBar::Listener
    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override;

    // DragPosition::Listener (inerzia del pan touch)
    void positionChanged (DragPosition&, double) override;

    //==========================================================================
    void documentChanged (int fromLine);
    void ensureTokensValidUpTo (int lastLine);
    void recomputeContentSize();
    void updateScrollBars();
    void updateFontMetrics();
    void updateGutterWidth();
    juce::Rectangle<int> getTextArea() const;

    // colonne <-> indici (tab espansi)
    int columnOfIndex (const juce::String& line, int index) const;
    int indexForColumn (const juce::String& line, int column) const;
    int columnOfPosition (const juce::CodeDocument::Position& pos) const;

    void setCaretAndAnchor (const juce::CodeDocument::Position& caret, const juce::CodeDocument::Position& anchor);
    void caretMoved (bool keepOnScreen);
    void restartCaretBlink();

    juce::CodeDocument::Position wordStart (const juce::CodeDocument::Position& pos) const;
    juce::CodeDocument::Position wordEnd (const juce::CodeDocument::Position& pos) const;
    void selectWordAt (const juce::CodeDocument::Position& pos);
    void selectLineAt (const juce::CodeDocument::Position& pos);
    juce::Range<int> getSelectedLineRange() const;

    void beginTypingTransactionIfNeeded (bool forceNew);
    void insertTextInternal (const juce::String& text);

    // gesture touch
    void startLongPressTimer();
    void cancelLongPress();
    void longPressFired();
    void stopMomentum();
    void beginTouchScroll (const juce::MouseEvent& e);
    bool hitTestHandle (juce::Point<float> p, bool& isStartHandle) const;
    juce::Point<float> handleCentre (bool startHandle) const;
    void autoScrollTick();
    void showMenuForSelection();
    void reshowOnScreenKeyboard();

    void paintText (juce::Graphics& g, juce::Rectangle<int> area);
    void paintGutter (juce::Graphics& g);
    void paintHandles (juce::Graphics& g);
    void paintMagnifier (juce::Graphics& g);

    //==========================================================================
    juce::CodeDocument& document;
    juce::CodeTokeniser* tokeniser;
    DocumentListener documentListener { *this };
    ColourScheme colourScheme;

    juce::Font font { juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain) };
    float charWidth = 8.0f;
    int lineHeight = 15;
    float fontAscent = 11.0f;
    int tabSize = 4;
    bool useSpacesForTabs = true;
    bool showLineNumbers = true;
    bool readOnly = false;
    int gutterWidth = 0;

    juce::CodeDocument::Position caretPos, selectionAnchor;
    int columnToMaintain = -1; // per le freccie su/giu

    // Scroll in pixel; contenuto = tutte le righe (+ un margine in fondo).
    double scrollX = 0.0, scrollY = 0.0;
    double contentWidth = 0.0, contentHeight = 0.0;
    int maxLineColumns = 0;
    bool contentSizeDirty = true;

    juce::ScrollBar verticalScrollBar { true }, horizontalScrollBar { false };
    int scrollBarThickness = 12;

    std::vector<LineTokens> lineTokens;
    int firstInvalidLine = 0;

    CallbackTimer caretBlinkTimer, longPressTimer, autoScrollTimer;
    bool caretVisible = true;

    // Gesture
    Gesture gesture = Gesture::none;
    int gestureSourceIndex = -1;
    juce::Point<float> gestureStartPoint, lastDragPoint;
    juce::Point<double> scrollAtGestureStart;
    DragPosition dragOffsetX, dragOffsetY;
    bool draggingStartHandle = false;
    bool momentumActive = false;        // inerzia in corso dopo un pan (vedi positionChanged)
    bool textInputSuspended = false;    // vedi reshowOnScreenKeyboard
    bool showHandles = false;           // maniglie visibili (selezione fatta col dito)
    bool magnifierVisible = false;
    juce::Point<float> magnifierPoint;

    double lastTypingTime = 0.0;
    juce::Array<juce::Range<int>> temporaryUnderlines; // IME (setTemporaryUnderlining)

    static constexpr int   kLongPressMs   = 450;
    static constexpr float kTouchSlop     = 10.0f;
    static constexpr float kHandleRadius  = 7.0f;
    static constexpr float kHandleHitSize = 26.0f;
    static constexpr float kMagnifierRadius = 48.0f;
    static constexpr float kMagnifierScale  = 1.8f;
    static constexpr double kTypingTransactionGapSec = 0.8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CodeView)
};
