#include "CsoundCodeView.h"
#include "IOSKeyboard.h"
#include <cmath>

namespace
{
    bool isWordChar (juce::juce_wchar c) noexcept
    {
        return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
    }

    double nowSeconds() noexcept
    {
        return juce::Time::getMillisecondCounterHiRes() * 0.001;
    }
}

//==============================================================================
CodeView::CodeView (juce::CodeDocument& doc, juce::CodeTokeniser* tok)
    : document (doc),
      tokeniser (tok),
      caretPos (doc, 0),
      selectionAnchor (doc, 0)
{
    caretPos.setPositionMaintained (true);
    selectionAnchor.setPositionMaintained (true);

    if (tokeniser != nullptr)
        colourScheme = tokeniser->getDefaultColourScheme();

    setOpaque (true);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (false); // il focus lo diamo noi (tap/click, non pan)
    setMouseCursor (juce::MouseCursor::IBeamCursor);

    addChildComponent (verticalScrollBar);
    addChildComponent (horizontalScrollBar);
    verticalScrollBar.setAutoHide (true);
    horizontalScrollBar.setAutoHide (true);
    verticalScrollBar.addListener (this);
    horizontalScrollBar.addListener (this);
    scrollBarThickness = getLookAndFeel().getDefaultScrollbarWidth();

    // Inerzia del pan touch: stesso motore di juce::Viewport.
    dragOffsetX.addListener (this);
    dragOffsetY.addListener (this);
    dragOffsetX.behaviour.setMinimumVelocity (60);
    dragOffsetY.behaviour.setMinimumVelocity (60);

    caretBlinkTimer.onTick = [this]
    {
        caretVisible = ! caretVisible;
        repaint (getCaretRectangle().expanded (2));
    };

    longPressTimer.onTick = [this] { longPressFired(); };
    autoScrollTimer.onTick = [this] { autoScrollTick(); };

    IOSKeyboard::initialise(); // osservatori della tastiera iOS attivi PRIMA della prima apertura

    document.addListener (&documentListener);
    updateFontMetrics();
    documentChanged (0);
}

CodeView::~CodeView()
{
    document.removeListener (&documentListener);
    dragOffsetX.removeListener (this);
    dragOffsetY.removeListener (this);
}

//==============================================================================
void CodeView::setFont (const juce::Font& newFont)
{
    font = newFont;
    updateFontMetrics();
    contentSizeDirty = true;
    updateScrollBars();
    repaint();
}

void CodeView::setColourScheme (const ColourScheme& scheme)
{
    colourScheme = scheme;
    repaint();
}

void CodeView::setTabSize (int numSpaces, bool insertSpaces)
{
    tabSize = juce::jmax (1, numSpaces);
    useSpacesForTabs = insertSpaces;
    repaint();
}

void CodeView::setLineNumbersShown (bool shouldBeShown)
{
    showLineNumbers = shouldBeShown;
    updateGutterWidth();
    repaint();
}

void CodeView::updateFontMetrics()
{
    charWidth   = juce::jmax (1.0f, juce::GlyphArrangement::getStringWidth (font, "MMMMMMMMMM") / 10.0f);
    lineHeight  = juce::jmax (1, juce::roundToInt (font.getHeight() * 1.15f));
    fontAscent  = font.getAscent() + (float) (lineHeight - font.getHeight()) * 0.5f;
    updateGutterWidth();
}

void CodeView::updateGutterWidth()
{
    if (! showLineNumbers)
    {
        gutterWidth = 6;
        return;
    }

    const int digits = juce::jmax (3, juce::String (document.getNumLines()).length());
    gutterWidth = juce::roundToInt ((float) digits * charWidth) + 18;
}

void CodeView::lookAndFeelChanged()
{
    scrollBarThickness = getLookAndFeel().getDefaultScrollbarWidth();
    resized();
    repaint();
}

juce::Rectangle<int> CodeView::getTextArea() const
{
    auto r = getLocalBounds();
    r.removeFromLeft (gutterWidth);

    if (verticalScrollBar.isVisible())
        r.removeFromRight (scrollBarThickness);

    if (horizontalScrollBar.isVisible())
        r.removeFromBottom (scrollBarThickness);

    return r;
}

void CodeView::resized()
{
    updateScrollBars();
}

//==============================================================================
// Documento
void CodeView::DocumentListener::codeDocumentTextInserted (const juce::String&, int insertIndex)
{
    owner.documentChanged (juce::CodeDocument::Position (owner.document, insertIndex).getLineNumber());
}

void CodeView::DocumentListener::codeDocumentTextDeleted (int startIndex, int)
{
    owner.documentChanged (juce::CodeDocument::Position (owner.document, startIndex).getLineNumber());
}

void CodeView::documentChanged (int fromLine)
{
    const int numLines = document.getNumLines();
    lineTokens.resize ((size_t) juce::jmax (0, numLines));

    firstInvalidLine = juce::jlimit (0, numLines, juce::jmin (firstInvalidLine, fromLine));

    for (int i = firstInvalidLine; i < numLines; ++i)
        lineTokens[(size_t) i].valid = false;

    contentSizeDirty = true;
    updateGutterWidth();
    updateScrollBars();
    hideTouchEditMenu(); // [EDIT-CALLOUT]
    repaint();
}

void CodeView::ensureTokensValidUpTo (int lastLine)
{
    const int numLines = document.getNumLines();
    lastLine = juce::jmin (lastLine, numLines - 1);

    if (lastLine < firstInvalidLine)
        return;

    if (tokeniser == nullptr)
    {
        for (int line = firstInvalidLine; line <= lastLine; ++line)
        {
            auto& lt = lineTokens[(size_t) line];
            lt.segments.clear();
            lt.segments.push_back ({ 0, 0, document.getLine (line).length() });
            lt.valid = true;
        }

        firstInvalidLine = lastLine + 1;
        return;
    }

    // Un solo iteratore che scorre dalla prima riga non valida in poi: i
    // token che attraversano piu' righe (commenti /* */) vengono spezzati
    // riga per riga.
    juce::CodeDocument::Iterator it (juce::CodeDocument::Position (document, firstInvalidLine, 0));

    for (int line = firstInvalidLine; line <= lastLine; ++line)
        lineTokens[(size_t) line].segments.clear();

    const int lastLineEnd = juce::CodeDocument::Position (document, lastLine, 0).getPosition()
                            + document.getLine (lastLine).length();

    while (! it.isEOF() && it.getPosition() < lastLineEnd)
    {
        const int tokStart = it.getPosition();
        const int type = tokeniser->readNextToken (it);
        const int tokEnd = it.getPosition();

        if (tokEnd <= tokStart)
            break; // il tokeniser non avanza: evita un loop infinito

        const juce::CodeDocument::Position a (document, tokStart);
        const juce::CodeDocument::Position b (document, tokEnd);

        for (int line = a.getLineNumber(); line <= juce::jmin (b.getLineNumber(), lastLine); ++line)
        {
            const int lineStart = juce::CodeDocument::Position (document, line, 0).getPosition();
            const int lineLen   = document.getLine (line).length();

            const int s = juce::jmax (0, tokStart - lineStart);
            const int e = juce::jmin (lineLen, tokEnd - lineStart);

            if (e > s && line >= firstInvalidLine)
                lineTokens[(size_t) line].segments.push_back ({ type, s, e });
        }
    }

    for (int line = firstInvalidLine; line <= lastLine; ++line)
        lineTokens[(size_t) line].valid = true;

    firstInvalidLine = lastLine + 1;
}

void CodeView::recomputeContentSize()
{
    if (! contentSizeDirty)
        return;

    contentSizeDirty = false;

    maxLineColumns = document.getMaximumLineLength();
    contentWidth  = (double) (maxLineColumns + 4) * charWidth + textInsetLeft;
    contentHeight = (double) document.getNumLines() * lineHeight + lineHeight + textInsetTop; // un margine sotto l'ultima riga
}

void CodeView::updateScrollBars()
{
    recomputeContentSize();

    // Due passate: la visibilita' di una barra cambia l'area disponibile.
    for (int pass = 0; pass < 2; ++pass)
    {
        const auto area = getTextArea();

        const bool needV = contentHeight > area.getHeight();
        const bool needH = contentWidth  > area.getWidth();

        verticalScrollBar.setVisible (needV);
        horizontalScrollBar.setVisible (needH);
    }

    const auto area = getTextArea();

    const double maxX = juce::jmax (0.0, contentWidth  - area.getWidth());
    const double maxY = juce::jmax (0.0, contentHeight - area.getHeight());
    scrollX = juce::jlimit (0.0, maxX, scrollX);
    scrollY = juce::jlimit (0.0, maxY, scrollY);

    verticalScrollBar.setBounds (getWidth() - scrollBarThickness, 0, scrollBarThickness,
                                 getHeight() - (horizontalScrollBar.isVisible() ? scrollBarThickness : 0));
    horizontalScrollBar.setBounds (gutterWidth, getHeight() - scrollBarThickness,
                                   getWidth() - gutterWidth - (verticalScrollBar.isVisible() ? scrollBarThickness : 0),
                                   scrollBarThickness);

    verticalScrollBar.setRangeLimits (0.0, juce::jmax (contentHeight, (double) area.getHeight()), juce::dontSendNotification);
    verticalScrollBar.setCurrentRange (scrollY, area.getHeight(), juce::dontSendNotification);
    verticalScrollBar.setSingleStepSize (lineHeight);

    horizontalScrollBar.setRangeLimits (0.0, juce::jmax (contentWidth, (double) area.getWidth()), juce::dontSendNotification);
    horizontalScrollBar.setCurrentRange (scrollX, area.getWidth(), juce::dontSendNotification);
    horizontalScrollBar.setSingleStepSize (charWidth * 4.0);
}

void CodeView::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &verticalScrollBar)
        setScrollPosition (scrollX, newRangeStart);
    else if (bar == &horizontalScrollBar)
        setScrollPosition (newRangeStart, scrollY);
}

//==============================================================================
// Colonne <-> indici (i tab esistenti nel file sono espansi alla tabulazione)
int CodeView::columnOfIndex (const juce::String& line, int index) const
{
    int col = 0;
    auto p = line.getCharPointer();

    for (int i = 0; i < index && ! p.isEmpty(); ++i)
    {
        const auto c = p.getAndAdvance();
        col = (c == '\t') ? ((col / tabSize) + 1) * tabSize : col + 1;
    }

    return col;
}

int CodeView::indexForColumn (const juce::String& line, int column) const
{
    int col = 0, index = 0;
    auto p = line.getCharPointer();

    while (! p.isEmpty())
    {
        const auto c = *p;
        const int next = (c == '\t') ? ((col / tabSize) + 1) * tabSize : col + 1;

        if (next > column)
            return (column - col) * 2 >= (next - col) ? index + 1 : index; // al piu' vicino

        col = next;
        ++index;
        ++p;
    }

    return index;
}

int CodeView::columnOfPosition (const juce::CodeDocument::Position& pos) const
{
    return columnOfIndex (document.getLine (pos.getLineNumber()), pos.getIndexInLine());
}

//==============================================================================
// Geometria
int CodeView::getFirstLineOnScreen() const
{
    return juce::jmax (0, (int) std::floor ((scrollY - textInsetTop) / (double) lineHeight));
}

int CodeView::getNumLinesOnScreen() const
{
    return juce::jmax (1, (int) std::ceil ((double) getTextArea().getHeight() / (double) lineHeight));
}

juce::Rectangle<int> CodeView::getCharacterBounds (const juce::CodeDocument::Position& pos) const
{
    const int col = columnOfPosition (pos);
    const int x = gutterWidth + textInsetLeft + juce::roundToInt ((double) col * charWidth - scrollX);
    const int y = juce::roundToInt ((double) pos.getLineNumber() * lineHeight - scrollY + textInsetTop);

    return { x, y, juce::roundToInt (charWidth), lineHeight };
}

juce::CodeDocument::Position CodeView::getPositionAt (int x, int y) const
{
    const int numLines = document.getNumLines();
    const int line = juce::jlimit (0, juce::jmax (0, numLines - 1),
                                   (int) std::floor (((double) (y - textInsetTop) + scrollY) / (double) lineHeight));

    const double colF = ((double) (x - gutterWidth - textInsetLeft) + scrollX) / (double) charWidth;
    const int column = juce::jmax (0, juce::roundToInt (colF));

    const auto lineText = document.getLine (line);
    // getLine include il fine riga: non si puo' andare oltre l'ultimo carattere "vero"
    int lineLen = lineText.length();
    while (lineLen > 0 && (lineText[lineLen - 1] == '\n' || lineText[lineLen - 1] == '\r'))
        --lineLen;

    const int index = juce::jmin (lineLen, indexForColumn (lineText, column));
    return { document, line, index };
}

void CodeView::setScrollPosition (double newX, double newY)
{
    recomputeContentSize();
    const auto area = getTextArea();

    const double maxX = juce::jmax (0.0, contentWidth  - area.getWidth());
    const double maxY = juce::jmax (0.0, contentHeight - area.getHeight());

    newX = juce::jlimit (0.0, maxX, newX);
    newY = juce::jlimit (0.0, maxY, newY);

    if (juce::approximatelyEqual (newX, scrollX) && juce::approximatelyEqual (newY, scrollY))
        return;

    scrollX = newX;
    scrollY = newY;

    hideTouchEditMenu(); // [EDIT-CALLOUT]

    verticalScrollBar.setCurrentRange (scrollY, area.getHeight(), juce::dontSendNotification);
    horizontalScrollBar.setCurrentRange (scrollX, area.getWidth(), juce::dontSendNotification);

    editorViewportPositionChanged();
    repaint();
}

void CodeView::scrollToLine (int line)
{
    setScrollPosition (scrollX, (double) juce::jmax (0, line) * lineHeight);
}

void CodeView::scrollBy (int deltaLines)
{
    setScrollPosition (scrollX, scrollY + (double) deltaLines * lineHeight);
}

void CodeView::scrollToColumn (int column)
{
    setScrollPosition ((double) juce::jmax (0, column) * charWidth, scrollY);
}

void CodeView::scrollToKeepCaretOnScreen()
{
    const auto area = getTextArea();

    if (area.isEmpty())
        return;

    const auto caretRect = getCharacterBounds (caretPos);
    double newX = scrollX, newY = scrollY;

    // Verticale: una riga di margine quando possibile.
    if (caretRect.getY() < area.getY())
        newY = scrollY + (caretRect.getY() - area.getY()) - lineHeight;
    else if (caretRect.getBottom() > area.getBottom())
        newY = scrollY + (caretRect.getBottom() - area.getBottom()) + lineHeight;

    // Orizzontale: qualche colonna di margine.
    const int marginX = juce::roundToInt (charWidth * 4.0f);

    if (caretRect.getX() < area.getX() + marginX)
        newX = scrollX + (caretRect.getX() - area.getX()) - marginX;
    else if (caretRect.getRight() > area.getRight() - marginX)
        newX = scrollX + (caretRect.getRight() - area.getRight()) + marginX;

    setScrollPosition (newX, newY);
}

//==============================================================================
// Caret e selezione
juce::CodeDocument::Position CodeView::getSelectionStart() const
{
    return caretPos.getPosition() <= selectionAnchor.getPosition() ? caretPos : selectionAnchor;
}

juce::CodeDocument::Position CodeView::getSelectionEnd() const
{
    return caretPos.getPosition() <= selectionAnchor.getPosition() ? selectionAnchor : caretPos;
}

juce::Range<int> CodeView::getHighlightedRegion() const
{
    return { getSelectionStart().getPosition(), getSelectionEnd().getPosition() };
}

bool CodeView::isHighlightActive() const
{
    return caretPos.getPosition() != selectionAnchor.getPosition();
}

void CodeView::setCaretAndAnchor (const juce::CodeDocument::Position& caret, const juce::CodeDocument::Position& anchor)
{
    caretPos.setPosition (caret.getPosition());
    selectionAnchor.setPosition (anchor.getPosition());
}

void CodeView::moveCaretTo (const juce::CodeDocument::Position& newPos, bool selecting)
{
    columnToMaintain = -1;

    if (selecting)
        caretPos.setPosition (newPos.getPosition());
    else
        setCaretAndAnchor (newPos, newPos);

    caretMoved (true);
}

void CodeView::selectRegion (const juce::CodeDocument::Position& start, const juce::CodeDocument::Position& end)
{
    columnToMaintain = -1;
    setCaretAndAnchor (end, start);
    caretMoved (true);
}

void CodeView::setHighlightedRegion (const juce::Range<int>& region)
{
    selectRegion (juce::CodeDocument::Position (document, region.getStart()),
                  juce::CodeDocument::Position (document, region.getEnd()));
}

void CodeView::setTemporaryUnderlining (const juce::Array<juce::Range<int>>& underlinedRegions)
{
    temporaryUnderlines = underlinedRegions;
    repaint();
}

juce::String CodeView::getTextInRange (const juce::Range<int>& range) const
{
    return document.getTextBetween (juce::CodeDocument::Position (document, range.getStart()),
                                    juce::CodeDocument::Position (document, range.getEnd()));
}

juce::Rectangle<int> CodeView::getCaretRectangleForCharIndex (int characterIndex) const
{
    const auto r = getCharacterBounds (juce::CodeDocument::Position (document, characterIndex));
    return { r.getX(), r.getY(), 2, lineHeight };
}

int CodeView::getCharIndexForPoint (juce::Point<int> point) const
{
    return getPositionAt (point.x, point.y).getPosition();
}

juce::RectangleList<int> CodeView::getTextBounds (juce::Range<int> textRange) const
{
    juce::RectangleList<int> list;
    const juce::CodeDocument::Position start (document, textRange.getStart());
    const juce::CodeDocument::Position end   (document, textRange.getEnd());

    for (int line = start.getLineNumber(); line <= end.getLineNumber(); ++line)
    {
        const auto lineText = document.getLine (line);
        const int a = line == start.getLineNumber() ? start.getIndexInLine() : 0;
        const int b = line == end.getLineNumber()   ? end.getIndexInLine()   : lineText.length();

        const int x1 = gutterWidth + textInsetLeft + juce::roundToInt ((double) columnOfIndex (lineText, a) * charWidth - scrollX);
        const int x2 = gutterWidth + textInsetLeft + juce::roundToInt ((double) columnOfIndex (lineText, b) * charWidth - scrollX);
        const int y  = juce::roundToInt ((double) line * lineHeight - scrollY + textInsetTop);

        list.add ({ x1, y, juce::jmax (1, x2 - x1), lineHeight });
    }

    return list;
}

bool CodeView::selectAll()
{
    columnToMaintain = -1;
    setCaretAndAnchor (juce::CodeDocument::Position (document, document.getNumCharacters()),
                       juce::CodeDocument::Position (document, 0));
    caretMoved (false);
    return true;
}

void CodeView::deselectAll()
{
    if (isHighlightActive())
    {
        selectionAnchor.setPosition (caretPos.getPosition());
        caretMoved (false);
    }
}

void CodeView::caretMoved (bool keepOnScreen)
{
    hideTouchEditMenu(); // [EDIT-CALLOUT] ricompare, se serve, da chi ha mosso la selezione

    if (keepOnScreen)
        scrollToKeepCaretOnScreen();

    restartCaretBlink();
    caretPositionMoved();
    repaint();
}

void CodeView::restartCaretBlink()
{
    caretVisible = true;

    if (hasKeyboardFocus (false))
        caretBlinkTimer.startTimer (500);
}

//==============================================================================
// Parole / righe
juce::CodeDocument::Position CodeView::wordStart (const juce::CodeDocument::Position& pos) const
{
    const auto line = document.getLine (pos.getLineNumber());
    int i = pos.getIndexInLine();

    while (i > 0 && isWordChar (line[i - 1]))
        --i;

    return { document, pos.getLineNumber(), i };
}

juce::CodeDocument::Position CodeView::wordEnd (const juce::CodeDocument::Position& pos) const
{
    const auto line = document.getLine (pos.getLineNumber());
    int i = pos.getIndexInLine();

    while (i < line.length() && isWordChar (line[i]))
        ++i;

    return { document, pos.getLineNumber(), i };
}

void CodeView::selectWordAt (const juce::CodeDocument::Position& pos)
{
    auto start = wordStart (pos);
    auto end   = wordEnd (pos);

    if (start.getPosition() == end.getPosition())
    {
        // Nessuna parola sotto: seleziona il singolo carattere (se c'e')
        const auto line = document.getLine (pos.getLineNumber());
        const int i = pos.getIndexInLine();

        if (i < line.length() && line[i] != '\n' && line[i] != '\r')
            end = juce::CodeDocument::Position (document, pos.getLineNumber(), i + 1);
    }

    selectRegion (start, end);
}

void CodeView::selectLineAt (const juce::CodeDocument::Position& pos)
{
    const int line = pos.getLineNumber();
    const juce::CodeDocument::Position start (document, line, 0);
    const juce::CodeDocument::Position end = line + 1 < document.getNumLines()
                                                ? juce::CodeDocument::Position (document, line + 1, 0)
                                                : juce::CodeDocument::Position (document, document.getNumCharacters());
    selectRegion (start, end);
}

juce::Range<int> CodeView::getSelectedLineRange() const
{
    const auto start = getSelectionStart();
    const auto end   = getSelectionEnd();

    int firstLine = start.getLineNumber();
    int lastLine  = end.getLineNumber();

    if (lastLine > firstLine && end.getIndexInLine() == 0)
        --lastLine;

    return { firstLine, lastLine + 1 };
}

//==============================================================================
// Editing
void CodeView::beginTypingTransactionIfNeeded (bool forceNew)
{
    const double now = nowSeconds();

    if (forceNew || now - lastTypingTime > kTypingTransactionGapSec)
        document.newTransaction();

    lastTypingTime = now;
}

void CodeView::insertTextInternal (const juce::String& text)
{
    if (readOnly)
        return;

    const int insertIndex = getSelectionStart().getPosition();

    if (isHighlightActive())
        document.deleteSection (getSelectionStart().getPosition(), getSelectionEnd().getPosition());

    // Niente azioni "vuote" nella cronologia del documento: il ponte verso
    // lo sharedUndoManager (PluginEditor) conta le azioni per transazione.
    if (text.isNotEmpty())
        document.insertText (insertIndex, text);

    const juce::CodeDocument::Position after (document, insertIndex + text.length());
    setCaretAndAnchor (after, after);
    columnToMaintain = -1;
    showHandles = false;
    caretMoved (true);
}

void CodeView::insertTextAtCaret (const juce::String& text)
{
    // Una battuta singola si accoda alla transazione corrente (gruppo di
    // digitazione); un blocco (paste, a capo...) apre un passo di undo suo.
    // Battuta singola (anche il backspace di iOS, che arriva come
    // setHighlightedRegion(1 carattere) + insertTextAtCaret("")) ->
    // si accoda al gruppo di digitazione; a capo, blocchi (paste, IME) o
    // sostituzione di una selezione "vera" -> passo di undo a se'.
    const bool replacingSelection = isHighlightActive() && getHighlightedRegion().getLength() > 1;
    beginTypingTransactionIfNeeded (text.length() > 1 || text.containsChar ('\n') || replacingSelection);
    insertTextInternal (text);
}

void CodeView::insertTabAtCaret()
{
    if (useSpacesForTabs)
    {
        const int col = columnOfPosition (caretPos);
        const int spaces = tabSize - (col % tabSize);
        insertTextAtCaret (juce::String::repeatedString (" ", spaces));
    }
    else
    {
        insertTextAtCaret ("\t");
    }
}

bool CodeView::deleteSelection()
{
    if (! isHighlightActive() || readOnly)
        return false;

    document.newTransaction();
    const int start = getSelectionStart().getPosition();
    document.deleteSection (start, getSelectionEnd().getPosition());

    const juce::CodeDocument::Position p (document, start);
    setCaretAndAnchor (p, p);
    showHandles = false;
    caretMoved (true);
    return true;
}

bool CodeView::cutToClipboard()
{
    copyToClipboard();
    deleteSelection();
    return true;
}

bool CodeView::copyToClipboard()
{
    if (isHighlightActive())
        juce::SystemClipboard::copyTextToClipboard (document.getTextBetween (getSelectionStart(), getSelectionEnd()));

    return true;
}

bool CodeView::pasteFromClipboard()
{
    const auto text = juce::SystemClipboard::getTextFromClipboard();

    if (text.isNotEmpty())
    {
        document.newTransaction();
        insertTextInternal (text.replace ("\r\n", "\n").replace ("\r", "\n"));
    }

    return true;
}

bool CodeView::undo()
{
    document.undo();
    showHandles = false;
    columnToMaintain = -1;
    caretMoved (true);
    return true;
}

bool CodeView::redo()
{
    document.redo();
    showHandles = false;
    columnToMaintain = -1;
    caretMoved (true);
    return true;
}

bool CodeView::moveCaretLeft (bool moveInWholeWordSteps, bool selecting)
{
    if (! selecting && isHighlightActive())
    {
        moveCaretTo (getSelectionStart(), false);
        return true;
    }

    auto pos = caretPos;

    if (moveInWholeWordSteps)
    {
        // Salta gli spazi, poi la parola.
        pos.moveBy (-1);
        while (pos.getPosition() > 0 && ! isWordChar (pos.getCharacter()))
            pos.moveBy (-1);
        while (pos.getPosition() > 0 && isWordChar (pos.movedBy (-1).getCharacter()))
            pos.moveBy (-1);
    }
    else
    {
        pos.moveBy (-1);
    }

    moveCaretTo (pos, selecting);
    return true;
}

bool CodeView::moveCaretRight (bool moveInWholeWordSteps, bool selecting)
{
    if (! selecting && isHighlightActive())
    {
        moveCaretTo (getSelectionEnd(), false);
        return true;
    }

    auto pos = caretPos;
    const int total = document.getNumCharacters();

    if (moveInWholeWordSteps)
    {
        while (pos.getPosition() < total && ! isWordChar (pos.getCharacter()))
            pos.moveBy (1);
        while (pos.getPosition() < total && isWordChar (pos.getCharacter()))
            pos.moveBy (1);
    }
    else
    {
        pos.moveBy (1);
    }

    moveCaretTo (pos, selecting);
    return true;
}

bool CodeView::moveCaretUp (bool selecting)
{
    if (columnToMaintain < 0)
        columnToMaintain = columnOfPosition (caretPos);

    const int col = columnToMaintain;
    const int line = caretPos.getLineNumber();

    if (line == 0)
        moveCaretTo (juce::CodeDocument::Position (document, 0), selecting);
    else
        moveCaretTo (juce::CodeDocument::Position (document, line - 1, indexForColumn (document.getLine (line - 1).trimEnd(), col)), selecting);

    columnToMaintain = col;
    return true;
}

bool CodeView::moveCaretDown (bool selecting)
{
    if (columnToMaintain < 0)
        columnToMaintain = columnOfPosition (caretPos);

    const int col = columnToMaintain;
    const int line = caretPos.getLineNumber();

    if (line >= document.getNumLines() - 1)
        moveCaretTo (juce::CodeDocument::Position (document, document.getNumCharacters()), selecting);
    else
        moveCaretTo (juce::CodeDocument::Position (document, line + 1, indexForColumn (document.getLine (line + 1).trimEnd(), col)), selecting);

    columnToMaintain = col;
    return true;
}

bool CodeView::moveCaretToStartOfLine (bool selecting)
{
    // Prima alla prima colonna non bianca, poi (se gia' li') a colonna 0.
    const int line = caretPos.getLineNumber();
    const auto text = document.getLine (line);
    int firstNonWs = 0;

    while (firstNonWs < text.length() && (text[firstNonWs] == ' ' || text[firstNonWs] == '\t'))
        ++firstNonWs;

    const int target = (caretPos.getIndexInLine() == firstNonWs || firstNonWs >= text.trimEnd().length()) ? 0 : firstNonWs;
    moveCaretTo (juce::CodeDocument::Position (document, line, target), selecting);
    return true;
}

bool CodeView::moveCaretToEndOfLine (bool selecting)
{
    const int line = caretPos.getLineNumber();
    moveCaretTo (juce::CodeDocument::Position (document, line, document.getLine (line).trimEnd().length()), selecting);
    return true;
}

bool CodeView::moveCaretToTop (bool selecting)
{
    moveCaretTo (juce::CodeDocument::Position (document, 0), selecting);
    return true;
}

bool CodeView::moveCaretToEnd (bool selecting)
{
    moveCaretTo (juce::CodeDocument::Position (document, document.getNumCharacters()), selecting);
    return true;
}

bool CodeView::pageUp (bool selecting)
{
    const int lines = juce::jmax (1, getNumLinesOnScreen() - 1);

    if (columnToMaintain < 0)
        columnToMaintain = columnOfPosition (caretPos);

    const int col = columnToMaintain;
    const int line = juce::jmax (0, caretPos.getLineNumber() - lines);
    scrollBy (-lines);
    moveCaretTo (juce::CodeDocument::Position (document, line, indexForColumn (document.getLine (line).trimEnd(), col)), selecting);
    columnToMaintain = col;
    return true;
}

bool CodeView::pageDown (bool selecting)
{
    const int lines = juce::jmax (1, getNumLinesOnScreen() - 1);

    if (columnToMaintain < 0)
        columnToMaintain = columnOfPosition (caretPos);

    const int col = columnToMaintain;
    const int line = juce::jmin (document.getNumLines() - 1, caretPos.getLineNumber() + lines);
    scrollBy (lines);
    moveCaretTo (juce::CodeDocument::Position (document, line, indexForColumn (document.getLine (line).trimEnd(), col)), selecting);
    columnToMaintain = col;
    return true;
}

bool CodeView::deleteBackwards (bool moveInWholeWordSteps)
{
    if (readOnly)
        return true;

    if (deleteSelection())
        return true;

    if (caretPos.getPosition() == 0)
        return true;

    auto start = caretPos;

    if (moveInWholeWordSteps)
    {
        start.moveBy (-1);
        while (start.getPosition() > 0 && ! isWordChar (start.getCharacter()) && start.getCharacter() != '\n')
            start.moveBy (-1);
        while (start.getPosition() > 0 && isWordChar (start.movedBy (-1).getCharacter()))
            start.moveBy (-1);
    }
    else
    {
        // Solo spazi prima del caret: cancella fino alla tabulazione precedente
        const auto line = document.getLine (caretPos.getLineNumber());
        const int idx = caretPos.getIndexInLine();
        bool onlySpaces = idx > 0;

        for (int i = 0; i < idx; ++i)
            if (line[i] != ' ') { onlySpaces = false; break; }

        if (onlySpaces && useSpacesForTabs)
        {
            const int col = columnOfIndex (line, idx);
            const int targetCol = ((col - 1) / tabSize) * tabSize;
            start = juce::CodeDocument::Position (document, caretPos.getLineNumber(), juce::jmax (0, idx - (col - targetCol)));
        }
        else
        {
            start.moveBy (-1);
        }
    }

    beginTypingTransactionIfNeeded (moveInWholeWordSteps);
    document.deleteSection (start.getPosition(), caretPos.getPosition());
    setCaretAndAnchor (start, start);
    columnToMaintain = -1;
    caretMoved (true);
    return true;
}

bool CodeView::deleteForwards (bool moveInWholeWordSteps)
{
    if (readOnly)
        return true;

    if (deleteSelection())
        return true;

    auto end = caretPos;
    const int total = document.getNumCharacters();

    if (end.getPosition() >= total)
        return true;

    if (moveInWholeWordSteps)
    {
        while (end.getPosition() < total && ! isWordChar (end.getCharacter()) && end.getCharacter() != '\n')
            end.moveBy (1);
        while (end.getPosition() < total && isWordChar (end.getCharacter()))
            end.moveBy (1);
    }
    else
    {
        end.moveBy (1);
    }

    beginTypingTransactionIfNeeded (moveInWholeWordSteps);
    document.deleteSection (caretPos.getPosition(), end.getPosition());
    columnToMaintain = -1;
    caretMoved (true);
    return true;
}

bool CodeView::scrollUp()    { scrollBy (-1); return true; }
bool CodeView::scrollDown()  { scrollBy (1);  return true; }

void CodeView::handleReturnKey()
{
    const auto line = document.getLine (caretPos.getLineNumber());
    juce::String indent;

    for (int i = 0; i < line.length() && i < caretPos.getIndexInLine() && (line[i] == ' ' || line[i] == '\t'); ++i)
        indent << line[i];

    insertTextAtCaret ("\n" + indent);
}

void CodeView::handleTabKey()
{
    if (isHighlightActive() && getSelectionStart().getLineNumber() != getSelectionEnd().getLineNumber())
        indentSelection();
    else
        insertTabAtCaret();
}

void CodeView::indentSelection()
{
    if (readOnly)
        return;

    const auto lines = getSelectedLineRange();
    const auto indent = useSpacesForTabs ? juce::String::repeatedString (" ", tabSize) : juce::String ("\t");

    document.newTransaction();

    for (int line = lines.getEnd() - 1; line >= lines.getStart(); --line)
        if (line < document.getNumLines() && document.getLine (line).trim().isNotEmpty())
            document.insertText (juce::CodeDocument::Position (document, line, 0).getPosition(), indent);

    document.newTransaction();

    const juce::CodeDocument::Position start (document, lines.getStart(), 0);
    const juce::CodeDocument::Position end = lines.getEnd() < document.getNumLines()
                                                ? juce::CodeDocument::Position (document, lines.getEnd(), 0)
                                                : juce::CodeDocument::Position (document, document.getNumCharacters());
    selectRegion (start, end);
}

void CodeView::unindentSelection()
{
    if (readOnly)
        return;

    const auto lines = getSelectedLineRange();
    document.newTransaction();

    for (int line = lines.getEnd() - 1; line >= lines.getStart(); --line)
    {
        if (line >= document.getNumLines())
            continue;

        const auto text = document.getLine (line);
        int remove = 0;

        if (text.startsWithChar ('\t'))
            remove = 1;
        else
            while (remove < tabSize && remove < text.length() && text[remove] == ' ')
                ++remove;

        if (remove > 0)
        {
            const int lineStart = juce::CodeDocument::Position (document, line, 0).getPosition();
            document.deleteSection (lineStart, lineStart + remove);
        }
    }

    document.newTransaction();

    const juce::CodeDocument::Position start (document, lines.getStart(), 0);
    const juce::CodeDocument::Position end = lines.getEnd() < document.getNumLines()
                                                ? juce::CodeDocument::Position (document, lines.getEnd(), 0)
                                                : juce::CodeDocument::Position (document, document.getNumCharacters());
    selectRegion (start, end);
}

//==============================================================================
// Tastiera
bool CodeView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
        return handleEscapeKey();

    if (juce::TextEditorKeyMapper<CodeView>::invokeKeyFunction (*this, key))
        return true;

    if (key == juce::KeyPress::returnKey)
    {
        if (! readOnly)
            handleReturnKey();

        return true;
    }

    if (key.isKeyCode (juce::KeyPress::tabKey))
    {
        if (readOnly)
            return true;

        if (key.getModifiers().isShiftDown())
            unindentSelection();
        else
            handleTabKey();

        return true;
    }

    if (key == juce::KeyPress (']', juce::ModifierKeys::commandModifier, 0)) { indentSelection();   return true; }
    if (key == juce::KeyPress ('[', juce::ModifierKeys::commandModifier, 0)) { unindentSelection(); return true; }

    const auto c = key.getTextCharacter();

    if (c >= ' ' && ! key.getModifiers().isCommandDown() && ! key.getModifiers().isCtrlDown())
    {
        if (! readOnly)
            insertTextAtCaret (juce::String::charToString (c));

        return true;
    }

    return false;
}

void CodeView::focusGained (FocusChangeType)
{
    restartCaretBlink();
    repaint();
}

void CodeView::focusLost (FocusChangeType)
{
    hideTouchEditMenu(); // [EDIT-CALLOUT]
    caretBlinkTimer.stopTimer();
    caretVisible = false;
    repaint();
}

//==============================================================================
// Mouse / touch
void CodeView::stopMomentum()
{
    // momentumActive = false PRIMA: setPosition richiama positionChanged,
    // che altrimenti riporterebbe lo scroll al valore della vecchia gesture.
    momentumActive = false;
    dragOffsetX.setPosition (dragOffsetX.getPosition());
    dragOffsetY.setPosition (dragOffsetY.getPosition());
}

void CodeView::startLongPressTimer()   { longPressTimer.startTimer (kLongPressMs); }
void CodeView::cancelLongPress()       { longPressTimer.stopTimer(); }

juce::Point<float> CodeView::handleCentre (bool startHandle) const
{
    const auto r = getCharacterBounds (startHandle ? getSelectionStart() : getSelectionEnd());

    return startHandle ? juce::Point<float> ((float) r.getX(), (float) r.getY() - kHandleRadius)
                       : juce::Point<float> ((float) r.getX(), (float) r.getBottom() + kHandleRadius);
}

bool CodeView::hitTestHandle (juce::Point<float> p, bool& isStartHandle) const
{
    if (! showHandles || ! isHighlightActive())
        return false;

    if (handleCentre (true).getDistanceFrom (p) <= kHandleHitSize)  { isStartHandle = true;  return true; }
    if (handleCentre (false).getDistanceFrom (p) <= kHandleHitSize) { isStartHandle = false; return true; }

    return false;
}

void CodeView::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent != this)
        return;

    stopMomentum();
    autoScrollTimer.stopTimer();
    hideTouchEditMenu(); // [EDIT-CALLOUT] tocco altrove (o su una maniglia): via la bolla

    if (e.mods.isPopupMenu())
    {
        grabKeyboardFocus();

        // Click destro fuori dalla selezione: sposta il caret li' (come
        // ogni editor), dentro: lascia la selezione per il menu.
        const auto pos = getPositionAt (e.x, e.y);

        if (! getHighlightedRegion().contains (pos.getPosition()))
            moveCaretTo (pos, false);

        showContextMenu (e.getPosition());
        return;
    }

    if (! e.source.isTouch())
    {
        grabKeyboardFocus();
        showHandles = false;

        const auto pos = getPositionAt (e.x, e.y);
        const int clicks = e.getNumberOfClicks();

        if (clicks >= 3)       selectLineAt (pos);
        else if (clicks == 2)  selectWordAt (pos);
        else                   moveCaretTo (pos, e.mods.isShiftDown());

        gesture = Gesture::mouseSelecting;
        lastDragPoint = e.position;
        return;
    }

    // Touch -----------------------------------------------------------------
    gestureSourceIndex = e.source.getIndex();
    gestureStartPoint = e.position;
    lastDragPoint = e.position;

    bool startHandle = false;

    if (hitTestHandle (e.position, startHandle))
    {
        // Trascinare la maniglia di inizio = il caret e' l'inizio e
        // l'ancora la fine (e viceversa), cosi' moveCaretTo(…, true) muove
        // il capo giusto.
        const auto s = getSelectionStart(), en = getSelectionEnd();
        if (startHandle) setCaretAndAnchor (s, en); else setCaretAndAnchor (en, s);

        draggingStartHandle = startHandle;
        gesture = Gesture::touchHandleDragging;
        magnifierVisible = true;
        magnifierPoint = e.position;
        repaint();
        return;
    }

    gesture = Gesture::touchUndecided;
    startLongPressTimer();
}

void CodeView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.eventComponent != this)
        return;

    switch (gesture)
    {
        case Gesture::mouseSelecting:
        {
            lastDragPoint = e.position;
            moveCaretTo (getPositionAt (e.x, e.y), true);

            if (! getTextArea().contains (e.getPosition()))
                autoScrollTimer.startTimer (50);
            else
                autoScrollTimer.stopTimer();
            break;
        }

        case Gesture::touchUndecided:
        {
            if (e.source.getIndex() != gestureSourceIndex)
                break;

            if (e.position.getDistanceFrom (gestureStartPoint) > kTouchSlop)
            {
                cancelLongPress();
                beginTouchScroll (e);
            }
            break;
        }

        case Gesture::touchScrolling:
        {
            if (e.source.getIndex() != gestureSourceIndex)
                break;

            const auto offset = e.getOffsetFromDragStart().toFloat();
            dragOffsetX.drag ((double) offset.x);
            dragOffsetY.drag ((double) offset.y);
            break;
        }

        case Gesture::touchSelecting:
        case Gesture::touchHandleDragging:
        case Gesture::touchCaretDragging:
        {
            if (e.source.getIndex() != gestureSourceIndex)
                break;

            lastDragPoint = e.position;
            magnifierPoint = e.position;
            moveCaretTo (getPositionAt (e.x, e.y), gesture != Gesture::touchCaretDragging);

            if (! getTextArea().contains (e.getPosition()))
                autoScrollTimer.startTimer (50);
            else
                autoScrollTimer.stopTimer();

            repaint();
            break;
        }

        case Gesture::none:
        default:
            break;
    }
}

void CodeView::beginTouchScroll (const juce::MouseEvent& e)
{
    gesture = Gesture::touchScrolling;
    scrollAtGestureStart = { scrollX, scrollY };

    dragOffsetX.setPosition (0.0);
    dragOffsetY.setPosition (0.0);
    dragOffsetX.beginDrag();
    dragOffsetY.beginDrag();

    const auto offset = e.getOffsetFromDragStart().toFloat();
    dragOffsetX.drag ((double) offset.x);
    dragOffsetY.drag ((double) offset.y);
}

void CodeView::positionChanged (DragPosition&, double)
{
    // Chiamato sia durante il trascinamento sia durante l'inerzia.
    if (gesture != Gesture::touchScrolling && ! momentumActive)
        return;

    const double targetX = scrollAtGestureStart.x - dragOffsetX.getPosition();
    const double targetY = scrollAtGestureStart.y - dragOffsetY.getPosition();

    setScrollPosition (targetX, targetY);

    // Ai bordi l'inerzia si ferma (niente rubber band): se il valore e'
    // stato limitato, ferma l'animazione su quell'asse.
    if (gesture != Gesture::touchScrolling)
    {
        if (! juce::approximatelyEqual (targetX, scrollX)) dragOffsetX.setPosition (scrollAtGestureStart.x - scrollX);
        if (! juce::approximatelyEqual (targetY, scrollY)) dragOffsetY.setPosition (scrollAtGestureStart.y - scrollY);
    }
}

void CodeView::mouseUp (const juce::MouseEvent& e)
{
    if (e.eventComponent != this)
        return;

    autoScrollTimer.stopTimer();

    switch (gesture)
    {
        case Gesture::mouseSelecting:
            gesture = Gesture::none;
            break;

        case Gesture::touchUndecided:
        {
            // Tap: caret + tastiera. Doppio tap: parola. Tap sulla
            // selezione: menu.
            cancelLongPress();
            gesture = Gesture::none;

            const auto pos = getPositionAt (e.x, e.y);
            const bool onSelection = isHighlightActive() && getHighlightedRegion().contains (pos.getPosition());

            //const bool hadFocus = hasKeyboardFocus (true);
            grabKeyboardFocus();

           #if JUCE_IOS
            if (hadFocus && ! IOSKeyboard::isVisible())
                reshowOnScreenKeyboard();
           #endif

            if (e.getNumberOfClicks() >= 2)
            {
                // Doppio tap: seleziona la parola con le maniglie e apre il
                // menu (callout non modale, come iOS - anche con la
                // tastiera aperta).
                selectWordAt (pos);
                showHandles = true;
                showMenuForSelection();
            }
            else if (onSelection && showHandles)
            {
                showMenuForSelection();
            }
            else
            {
                showHandles = false;
                moveCaretTo (pos, false);
            }

            repaint();
            break;
        }

        case Gesture::touchScrolling:
            gesture = Gesture::none;
            momentumActive = true;
            dragOffsetX.endDrag(); // l'inerzia prosegue via positionChanged
            dragOffsetY.endDrag();
            break;

        case Gesture::touchSelecting:
        case Gesture::touchHandleDragging:
            gesture = Gesture::none;
            magnifierVisible = false;
            showHandles = isHighlightActive();
            repaint();

            if (isHighlightActive())
                showMenuForSelection();
            break;

        case Gesture::touchCaretDragging:
            gesture = Gesture::none;
            magnifierVisible = false;
            repaint();
            break;

        case Gesture::none:
        default:
            break;
    }
}

void CodeView::mouseDoubleClick (const juce::MouseEvent&)
{
    // Gestito in mouseDown tramite getNumberOfClicks().
}

void CodeView::reshowOnScreenKeyboard()
{
    // Il focus e' gia' nostro ma la tastiera a schermo e' chiusa (tasto
    // "nascondi" di iOS). Il peer la richiede solo quando CAMBIA il
    // TextInputTarget (refreshTextInputTarget; textInputRequired e'
    // privato): lo facciamo cambiare noi - prima "nessun target"
    // (isTextInputActive() = false -> il peer chiude l'input), poi, al giro
    // di eventi successivo, di nuovo noi (-> il peer riapre la tastiera).
    // In due giri distinti: chiusura e riapertura nello stesso giro
    // lasciavano la tastiera chiusa.
    auto* peer = getPeer();

    if (peer == nullptr)
        return;

    textInputSuspended = true;
    peer->refreshTextInputTarget();

    juce::Component::SafePointer<CodeView> safeThis (this);

    juce::MessageManager::callAsync ([safeThis]
    {
        if (safeThis == nullptr)
            return;

        safeThis->textInputSuspended = false;

        if (auto* p = safeThis->getPeer())
            p->refreshTextInputTarget();
    });
}

void CodeView::longPressFired()
{
    if (gesture != Gesture::touchUndecided)
        return;

    const auto pos = getPositionAt ((int) gestureStartPoint.x, (int) gestureStartPoint.y);

   #if JUCE_IOS
    // Tastiera aperta (si sta scrivendo): come in iOS la pressione
    // prolungata NON seleziona ma diventa "navigazione" - la lente segue
    // il dito e il caret si sposta carattere per carattere; al rilascio
    // nessun menu. Con la tastiera chiusa: selezione della parola.
    if (hasKeyboardFocus (true) && IOSKeyboard::isVisible())
    {
        gesture = Gesture::touchCaretDragging;
        showHandles = false;
        moveCaretTo (pos, false);
        magnifierVisible = true;
        magnifierPoint = gestureStartPoint;
        repaint();
        return;
    }
   #endif

    gesture = Gesture::touchSelecting;
    grabKeyboardFocus();
    selectWordAt (pos);

    // Da qui il dito trascina la FINE della selezione: ancora = inizio.
    setCaretAndAnchor (getSelectionEnd(), getSelectionStart());

    showHandles = true;
    magnifierVisible = true;
    magnifierPoint = gestureStartPoint;
    repaint();
}

void CodeView::showMenuForSelection()
{
    // [EDIT-CALLOUT] area della selezione (in coordinate del componente)
    // come ancora del menu touch.
    const auto a = getCharacterBounds (getSelectionStart());
    const auto b = getCharacterBounds (getSelectionEnd());
    const auto textArea = getTextArea();

    juce::Rectangle<int> area = a.getY() == b.getY()
        ? juce::Rectangle<int> (a.getX(), a.getY(), juce::jmax (a.getWidth(), b.getX() - a.getX()), lineHeight)
        : juce::Rectangle<int> (textArea.getX(), a.getY(), textArea.getWidth(), b.getBottom() - a.getY());

    showTouchEditMenu (area);
}

void CodeView::autoScrollTick()
{
    // Selezione trascinata oltre i bordi: scorri e continua a estendere.
    const auto area = getTextArea();
    double dx = 0.0, dy = 0.0;

    if (lastDragPoint.y < (float) area.getY())            dy = -lineHeight;
    else if (lastDragPoint.y > (float) area.getBottom())  dy =  lineHeight;

    if (lastDragPoint.x < (float) area.getX())            dx = -charWidth * 4.0;
    else if (lastDragPoint.x > (float) area.getRight())   dx =  charWidth * 4.0;

    if (dx == 0.0 && dy == 0.0)
        return;

    setScrollPosition (scrollX + dx, scrollY + dy);
    moveCaretTo (getPositionAt ((int) lastDragPoint.x, (int) lastDragPoint.y), gesture != Gesture::touchCaretDragging);
}

void CodeView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isAltDown())
    {
        Component::mouseWheelMove (e, wheel);
        return;
    }

    stopMomentum();

    // Stessa taratura di juce::Viewport (14 * passo di 16 px per unita'),
    // in pixel e senza arrotondare a righe intere: lo swipe del trackpad
    // scorre in modo continuo e si ferma quando si ferma il gesto.
    constexpr double pixelsPerUnit = 14.0 * 16.0;

    double dx = -wheel.deltaX * pixelsPerUnit;
    double dy = -wheel.deltaY * pixelsPerUnit;

    if (e.mods.isShiftDown() && juce::approximatelyEqual (dx, 0.0))
        std::swap (dx, dy);

    setScrollPosition (scrollX + dx, scrollY + dy);
}

void CodeView::mouseMagnify (const juce::MouseEvent&, float)
{
    // Pinch: niente zoom del font per ora (lo gestisce la LookAndFeel).
}

//==============================================================================
// Disegno
void CodeView::paint (juce::Graphics& g)
{
    const auto background = findColour (backgroundColourId);
    g.fillAll (background);

    const auto textArea = getTextArea();

    if (! textArea.isEmpty())
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (textArea);
        paintText (g, textArea);
    }

    paintGutter (g);

    if (showHandles && isHighlightActive())
        paintHandles (g);

    if (magnifierVisible)
        paintMagnifier (g);
}

void CodeView::paintText (juce::Graphics& g, juce::Rectangle<int> area)
{
    const int numLines = document.getNumLines();
    const int firstLine = getFirstLineOnScreen();
    const int lastLine  = juce::jmin (numLines - 1, firstLine + getNumLinesOnScreen() + 1);

    if (firstLine > lastLine)
        return;

    ensureTokensValidUpTo (lastLine);

    const auto defaultColour   = findColour (defaultTextColourId);
    const auto highlightColour = findColour (highlightColourId);
    const bool hasSelection    = isHighlightActive();
    const int selStart = getSelectionStart().getPosition();
    const int selEnd   = getSelectionEnd().getPosition();
    const int caretLine = caretPos.getLineNumber();

    g.setFont (font);

    for (int line = firstLine; line <= lastLine; ++line)
    {
        const float y = (float) ((double) line * lineHeight - scrollY + textInsetTop);
        const auto lineText = document.getLine (line);
        const int lineStartPos = juce::CodeDocument::Position (document, line, 0).getPosition();
        const int lineLen = lineText.length();

        // Riga corrente (solo senza selezione, come Xcode)
        if (! hasSelection && line == caretLine && hasKeyboardFocus (false))
        {
            g.setColour (highlightColour.withMultipliedAlpha (0.35f));
            g.fillRect ((float) area.getX(), y, (float) area.getWidth(), (float) lineHeight);
        }

        // Selezione
        if (hasSelection)
        {
            const int lineEndPos = lineStartPos + lineLen;

            if (selEnd > lineStartPos && selStart < lineEndPos)
            {
                const int a = juce::jmax (selStart, lineStartPos) - lineStartPos;
                const int b = juce::jmin (selEnd, lineEndPos) - lineStartPos;

                const bool toEndOfLine = selEnd >= lineEndPos && lineLen > 0
                                         && (lineText[lineLen - 1] == '\n' || lineText[lineLen - 1] == '\r');

                const float x1 = (float) ((double) (gutterWidth + textInsetLeft) + columnOfIndex (lineText, a) * charWidth - scrollX);
                float x2 = (float) ((double) (gutterWidth + textInsetLeft) + columnOfIndex (lineText, b) * charWidth - scrollX);

                if (toEndOfLine)
                    x2 = juce::jmax (x2, x1 + charWidth * 0.5f); // mostra che l'a-capo e' incluso

                g.setColour (highlightColour);
                g.fillRect (x1, y, juce::jmax (1.0f, x2 - x1), (float) lineHeight);
            }
        }

        // Testo, per segmento di token
        const auto& lt = lineTokens[(size_t) line];
        const float baseline = y + fontAscent;

        for (const auto& seg : lt.segments)
        {
            const int s = juce::jmin (seg.start, lineLen);
            const int e = juce::jmin (seg.end, lineLen);

            if (e <= s)
                continue;

            auto text = lineText.substring (s, e);

            if (text.trimEnd().isEmpty())
                continue; // solo spazi/fine riga

            if (text.containsChar ('\t'))
            {
                // Tab espansi a spazi per il disegno
                juce::String expanded;
                int col = columnOfIndex (lineText, s);

                for (auto p = text.getCharPointer(); ! p.isEmpty();)
                {
                    const auto c = p.getAndAdvance();

                    if (c == '\t')
                    {
                        const int next = ((col / tabSize) + 1) * tabSize;
                        expanded << juce::String::repeatedString (" ", next - col);
                        col = next;
                    }
                    else
                    {
                        expanded << juce::String::charToString (c);
                        ++col;
                    }
                }

                text = expanded;
            }

            const float x = (float) ((double) (gutterWidth + textInsetLeft) + columnOfIndex (lineText, s) * charWidth - scrollX);

            if (x > (float) area.getRight())
                break;

            const auto colour = (seg.type >= 0 && seg.type < colourScheme.types.size())
                                    ? colourScheme.types.getReference (seg.type).colour
                                    : defaultColour;

            g.setColour (colour);
            g.drawSingleLineText (text.trimEnd(), juce::roundToInt (x), juce::roundToInt (baseline));
        }
    }

    // Caret
    if (caretVisible && hasKeyboardFocus (false) && ! readOnly)
    {
        g.setColour (findColour (caretColourId));
        g.fillRect (getCaretRectangle());
    }
}

void CodeView::paintGutter (juce::Graphics& g)
{
    if (gutterWidth <= 0)
        return;

    const auto gutter = getLocalBounds().removeFromLeft (gutterWidth)
                            .withTrimmedBottom (horizontalScrollBar.isVisible() ? scrollBarThickness : 0);

    g.setColour (findColour (lineNumberBackgroundId));
    g.fillRect (gutter);

    if (! showLineNumbers)
        return;

    const int numLines = document.getNumLines();
    const int firstLine = getFirstLineOnScreen();
    const int lastLine  = juce::jmin (numLines - 1, firstLine + getNumLinesOnScreen() + 1);
    const int caretLine = caretPos.getLineNumber();

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (gutter);

    const auto numberFont = font.withHeight (font.getHeight() * 0.85f);
    g.setFont (numberFont);

    const auto textColour = findColour (lineNumberTextId);

    for (int line = firstLine; line <= lastLine; ++line)
    {
        const int y = juce::roundToInt ((double) line * lineHeight - scrollY + textInsetTop);
        g.setColour (line == caretLine ? textColour.contrasting (0.3f) : textColour);
        g.drawText (juce::String (line + 1), 0, y, gutterWidth - 8, lineHeight, juce::Justification::centredRight, false);
    }
}

void CodeView::paintHandles (juce::Graphics& g)
{
    const auto accent = findColour (highlightColourId).withAlpha (1.0f);

    for (bool startHandle : { true, false })
    {
        const auto c = handleCentre (startHandle);
        const auto r = getCharacterBounds (startHandle ? getSelectionStart() : getSelectionEnd());

        g.setColour (accent);
        g.fillRect ((float) r.getX() - 1.0f, (float) r.getY(), 2.0f, (float) lineHeight);
        g.fillEllipse (c.x - kHandleRadius, c.y - kHandleRadius, kHandleRadius * 2.0f, kHandleRadius * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (c.x - kHandleRadius, c.y - kHandleRadius, kHandleRadius * 2.0f, kHandleRadius * 2.0f, 1.5f);
    }
}

void CodeView::paintMagnifier (juce::Graphics& g)
{
    // Lente: la zona sotto il dito ridisegnata ingrandita in un cerchio
    // sopra il dito (come la lente di iOS).
    const float r = kMagnifierRadius;
    const juce::Point<float> centre (juce::jlimit (r, (float) getWidth() - r, magnifierPoint.x),
                                     juce::jmax (r, magnifierPoint.y - r - 24.0f));

    juce::Path clip;
    clip.addEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (clip);
        g.fillAll (findColour (backgroundColourId));

        // Scala attorno al punto sotto il dito, traslata nel cerchio.
        const auto t = juce::AffineTransform::translation (-magnifierPoint.x, -magnifierPoint.y)
                           .scaled (kMagnifierScale)
                           .translated (centre.x, centre.y);
        g.addTransform (t);
        paintText (g, getLocalBounds().expanded (2000)); // area "infinita": il clip e' il cerchio
    }

    g.setColour (findColour (highlightColourId).withAlpha (1.0f));
    g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 2.0f);
}
