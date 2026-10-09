#include "CsoundCodeEditor.h"
#include "CsoundActionSheet.h"
#include "IOSKeyboard.h"
#include "CsoundEditCallout.h" // [EDIT-CALLOUT]
#include <cmath>
#include <limits>

CsoundCodeEditor::CsoundCodeEditor (juce::CodeDocument& doc, juce::CodeTokeniser* tok)
    : CodeView (doc, tok),
      codeDocument (doc)
{
    setTabSize (indentSpaces, true); // Tab = 4 spazi, mai caratteri tab reali.
    codeDocument.addListener (this);

    // Autocompletamento disponibile da subito, prima ancora che il motore
    // Csound sia mai partito: seed con la tabella del manuale, che poi
    // setOpcodeSignatures() amplia con l'elenco reale del motore.
    for (auto& entry : CsoundOpcodeHelpData::table)
        allOpcodeNames.addIfNotAlreadyThere (juce::String (entry.name));
    allOpcodeNames.sort (true);

    // Il popup di autocompletamento vive come figlio dell'editor stesso:
    // intercetta il mouse (per hover/click, vedi i due lambda sotto) ma
    // non diventa MAI focalizzabile da tastiera - altrimenti si rischia
    // di riaprire la stessa classe di problemi di focus vista con la DAW.
    addChildComponent (suggestionPopup);
    suggestionPopup.setVisible (false);

    // Hover -> aggiorna la selezione (sincronizzata con quella da tastiera);
    // click -> accetta il suggerimento sotto il mouse. Nessuno dei due
    // richiede il focus da tastiera (SuggestionPopup non lo richiede mai),
    // quindi non interferiscono con la questione del focus-stealing.
    suggestionPopup.onHoverIndex = [this] (int index)
    {
        if (index >= 0 && index < currentSuggestions.size())
        {
            suggestionPopup.selectedIndex = index;
            suggestionPopup.repaint();
            pushSignatureHelpForSelectedSuggestion();
        }
    };

    suggestionPopup.onClickIndex = [this] (int index)
    {
        if (index >= 0 && index < currentSuggestions.size())
        {
            acceptSuggestion (currentSuggestions.getReference (index));
            grabKeyboardFocus(); // il click e' andato al popup, non all'editor
        }
    };

    // [EDIT-CALLOUT] bolla di editing touch (vedi CsoundEditCallout.h)
    addChildComponent (editCallout);
    editCallout.onItemSelected = [this] (int id) { performContextMenuItem (id); };
}

CsoundCodeEditor::~CsoundCodeEditor()
{
    codeDocument.removeListener (this);
}

//==============================================================================
juce::String CsoundCodeEditor::stripCommentAndTrim (const juce::String& lineText)
{
    auto s = lineText;

    const auto semicolon = s.indexOfChar (';');
    if (semicolon >= 0)
        s = s.substring (0, semicolon);

    const auto slashSlash = s.indexOf ("//");
    if (slashSlash >= 0)
        s = s.substring (0, slashSlash);

    return s.trim();
}

bool CsoundCodeEditor::startsWithDedentKeyword (const juce::String& trimmedLine)
{
    if (trimmedLine.isEmpty())
        return false;

    static const juce::StringArray dedentStarters { "endin", "endif", "od", "endop" };

    const auto firstWord = trimmedLine.upToFirstOccurrenceOf (" ", false, false)
                                       .upToFirstOccurrenceOf ("\t", false, false)
                                       .toLowerCase();

    if (dedentStarters.contains (firstWord))
        return true;

    // "else"/"elseif" chiudono il ramo if/elseif precedente prima di
    // eventualmente aprirne uno nuovo (vedi opensBlock).
    return firstWord == "else" || firstWord == "elseif";
}

bool CsoundCodeEditor::opensBlock (const juce::String& trimmedLine)
{
    if (trimmedLine.isEmpty())
        return false;

    const auto lower = trimmedLine.toLowerCase();
    const auto firstWord = lower.upToFirstOccurrenceOf (" ", false, false)
                                 .upToFirstOccurrenceOf ("\t", false, false);

    if (firstWord == "instr" || firstWord == "opcode")
        return true;

    // if ... then / elseif ... then
    if (lower.endsWith (" then") || lower == "then")
        return true;

    // while ... do / until ... do
    if (lower.endsWith (" do") || lower == "do")
        return true;

    if (firstWord == "else")
        return true;

    return false;
}

int CsoundCodeEditor::indentDepthBeforeLine (int lineIndex) const
{
    auto& doc = getDocument();
    int depth = 0;

    for (int i = 0; i < lineIndex; ++i)
    {
        const auto trimmed = stripCommentAndTrim (doc.getLine (i));

        if (trimmed.isEmpty())
            continue;

        if (startsWithDedentKeyword (trimmed))
            depth = juce::jmax (0, depth - 1);

        if (opensBlock (trimmed))
            ++depth;
    }

    return depth;
}

//==============================================================================
void CsoundCodeEditor::handleReturnKey()
{
    auto& doc = getDocument();
    const auto caret = getCaretPos();
    const int line = caret.getLineNumber();

    const auto textUpToCaret   = doc.getLine (line).substring (0, caret.getIndexInLine());
    const auto trimmedCurrent  = stripCommentAndTrim (textUpToCaret);

    int depth = indentDepthBeforeLine (line);

    if (startsWithDedentKeyword (trimmedCurrent))
        depth = juce::jmax (0, depth - 1);

    if (opensBlock (trimmedCurrent))
        ++depth;

    const auto newIndent = juce::String::repeatedString (" ", indentSpaces * depth);

    CodeView::insertTextAtCaret ("\n" + newIndent);
}

bool CsoundCodeEditor::keyPressed (const juce::KeyPress& key)
{
    // Cmd+Z / Cmd+Shift+Z: intercettati QUI, PRIMA di passare alla classe
    // base, perche' CodeEditorComponent::keyPressed (tramite
    // TextEditorKeyMapper::invokeKeyFunction) li consumerebbe da solo per
    // il proprio undo/redo testuale interno (document.getUndoManager()),
    // bypassando completamente la cronologia unica condivisa con il
    // pannello Parametri (vedi sharedUndoManager/performUndo()/
    // performRedo() in PluginEditor). Instradandoli qui verso
    // onUndoRequested/onRedoRequested, editor di codice e pannello
    // Parametri restano sempre sincronizzati con Cmd+Z tanto quanto con il
    // menu hamburger.
    // 'Z' MAIUSCOLA, non 'z': per convenzione JUCE, KeyPress::keyCode per i
    // tasti-lettera e' SEMPRE il codice ASCII maiuscolo, indipendentemente
    // da Shift (che e' nei modifiers, non nel keyCode) - BUG corretto qui
    // (era 'z' minuscola, che non corrisponde MAI a un vero evento Cmd+Z/
    // Cmd+Shift+Z): per questo lo shortcut da tastiera non funzionava piu'
    // una volta che l'editor ha iniziato a intercettarlo qui PRIMA della
    // classe base invece di lasciarlo passare al suo undo testuale interno
    // (che usa internamente i codici giusti, da cui "prima funzionava").
    if (key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier, 0))
    {
        if (onUndoRequested)
        {
            onUndoRequested();
            return true;
        }
    }

    // Cmd+Shift+Z e ANCHE Cmd+Y: TextEditorKeyMapper (usato da
    // CodeEditorComponent::keyPressed) accetta entrambi per il redo. Se
    // Cmd+Y arrivasse alla classe base farebbe redo SOLO sul document,
    // disallineando le due cronologie (vedi CodeEditTransactionProxy).
    if (key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('Y', juce::ModifierKeys::commandModifier, 0))
    {
        if (onRedoRequested)
        {
            onRedoRequested();
            return true;
        }
    }

    // Esc e' un interruttore per il popup dei suggerimenti: se e' chiuso,
    // la prima pressione lo apre (invocazione manuale dell'autocompletamento
    // sulla parola al caret, anche se non e' ancora stata digitata una
    // nuova lettera); se e' gia' aperto, la pressione successiva lo
    // richiude. Se non c'e' nulla da aprire (nessuna parola al caret),
    // lasciamo che Esc continui a fare quel che farebbe normalmente (es.
    // l'host potrebbe usarlo per altro).
    if (key == juce::KeyPress::escapeKey)
    {
        if (suggestionPopup.isVisible())
        {
            hideSuggestions();
            return true;
        }

        return forceShowSuggestions();
    }

    // Mentre il popup di autocompletamento e' visibile, le freccie Su/Giu
    // spostano la selezione (invece di spostare il caret nel documento),
    // Invio e Tab accettano il suggerimento evidenziato. Qualunque altro
    // tasto passa al comportamento normale (e quindi, piu' sotto,
    // ricalcola l'help/i suggerimenti in base alla nuova posizione del
    // caret o al nuovo testo digitato).
    if (! currentSuggestions.isEmpty())
    {
        if (key == juce::KeyPress::downKey)
        {
            moveSuggestionSelection (1);
            return true;
        }

        if (key == juce::KeyPress::upKey)
        {
            moveSuggestionSelection (-1);
            return true;
        }

        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::tabKey)
        {
            acceptSuggestion (currentSuggestions.getReference (suggestionPopup.selectedIndex));
            return true;
        }
    }

    // Modalita' parametri (vedi enterParameterMode): Tab/Shift+Tab
    // navigano i token della riga inserita; Esc, Invio e freccie escono.
    if (parameterMode)
    {
        if (key.isKeyCode (juce::KeyPress::tabKey))
        {
            if (! selectParameterToken (! key.getModifiers().isShiftDown()))
                exitParameterMode();

            return true;
        }

        if (key == juce::KeyPress::escapeKey)
        {
            exitParameterMode();
            return true;
        }

        if (key == juce::KeyPress::returnKey || key.isKeyCode (juce::KeyPress::upKey) || key.isKeyCode (juce::KeyPress::downKey)
            || key.isKeyCode (juce::KeyPress::leftKey) || key.isKeyCode (juce::KeyPress::rightKey)
            || key.isKeyCode (juce::KeyPress::homeKey) || key.isKeyCode (juce::KeyPress::endKey))
            exitParameterMode();
    }

    const bool handled = CodeView::keyPressed (key);

    // Qualunque altro tasto (freccie, Home/End, Page Up/Down...) puo' aver
    // spostato il caret senza passare da codeDocumentTextInserted/Deleted:
    // ricalcoliamo l'help cosi' "portare la selezione sull'opcode" con la
    // tastiera funziona esattamente come cliccarci sopra o digitarlo.
    updateOpcodeHelp();

    return handled;
}

void CsoundCodeEditor::showContextMenu (juce::Point<int>)
{
    const bool hasSelection = getHighlightedRegion().getLength() > 0;
    const bool canPaste = IOSKeyboard::clipboardHasText(); // non legge gli appunti (avviso iOS 16+)

    std::vector<CsoundActionSheetItem> items;

    auto add = [&items] (int id, const juce::String& text, CsoundActionSheetIcon icon, bool enabled = true)
    {
        CsoundActionSheetItem item;
        item.id = id; item.text = text; item.icon = icon; item.enabled = enabled;
        items.push_back (item);
    };

    add (ctxCut,       "Cut",        CsoundActionSheetIcon::cut,       hasSelection);
    add (ctxCopy,      "Copy",       CsoundActionSheetIcon::copy,      hasSelection);
    add (ctxPaste,     "Paste",      CsoundActionSheetIcon::paste,     canPaste);
    add (ctxDelete,    "Delete",     CsoundActionSheetIcon::trash,     hasSelection);
    add (ctxSelectAll, "Select All", CsoundActionSheetIcon::selectAll);
    items.push_back (CsoundActionSheetItem::separator());
    add (ctxUndo, "Undo", CsoundActionSheetIcon::undo, onUndoRequested != nullptr || getDocument().getUndoManager().canUndo());
    add (ctxRedo, "Redo", CsoundActionSheetIcon::redo, onRedoRequested != nullptr || getDocument().getUndoManager().canRedo());
    items.push_back (CsoundActionSheetItem::separator());
    add (ctxIndent,  "Indent", CsoundActionSheetIcon::indent);
    add (ctxComment, areSelectedLinesCommented() ? "Uncomment" : "Comment", CsoundActionSheetIcon::comment);

    juce::Component::SafePointer<CsoundCodeEditor> safeThis (this);

    // show() risale da qui all'AudioProcessorEditor (vedi CsoundActionSheet::show).
    CsoundActionSheet::show (*this, "", std::move (items), [safeThis] (int result)
    {
        if (safeThis != nullptr)
            safeThis->performContextMenuItem (result);
    });
}

void CsoundCodeEditor::performContextMenuItem (int result)
{
    switch (result)
    {
        case ctxCut:       cutToClipboard(); break;
        case ctxCopy:      copyToClipboard(); break;
        case ctxPaste:     pasteFromClipboard(); break;
        case ctxDelete:    deleteSelection(); break;
        case ctxSelectAll: selectAll(); break;
        case ctxUndo:      if (onUndoRequested) onUndoRequested(); else undo(); break;
        case ctxRedo:      if (onRedoRequested) onRedoRequested(); else redo(); break;
        case ctxIndent:    indentSelectedLines(); break;
        case ctxComment:   toggleCommentOnSelectedLines(); break;
        default: break;
    }

   #if ! JUCE_IOS
    grabKeyboardFocus(); // su iOS farebbe comparire la tastiera a schermo dopo ogni voce
   #endif
}

// [EDIT-CALLOUT] ---------------------------------------------------------------
void CsoundCodeEditor::showTouchEditMenu (juce::Rectangle<int> selectionArea)
{
    const bool hasSelection = getHighlightedRegion().getLength() > 0;
    const bool canPaste = IOSKeyboard::clipboardHasText();

    std::vector<CsoundEditCallout::Item> items;
    auto add = [&items] (int id, const juce::String& text, CsoundActionSheetIcon icon, bool enabled = true)
    {
        items.push_back ({ id, text, enabled, icon });
    };

    add (ctxCut,       "Cut",        CsoundActionSheetIcon::cut,       hasSelection);
    add (ctxCopy,      "Copy",       CsoundActionSheetIcon::copy,      hasSelection);
    add (ctxPaste,     "Paste",      CsoundActionSheetIcon::paste,     canPaste);
    add (ctxSelectAll, "Select All", CsoundActionSheetIcon::selectAll);
    add (ctxUndo,      "Undo",       CsoundActionSheetIcon::undo, onUndoRequested != nullptr || getDocument().getUndoManager().canUndo());
    add (ctxRedo,      "Redo",       CsoundActionSheetIcon::redo, onRedoRequested != nullptr || getDocument().getUndoManager().canRedo());
    add (ctxIndent,    "Indent",     CsoundActionSheetIcon::indent);
    add (ctxComment,   areSelectedLinesCommented() ? "Uncomment" : "Comment", CsoundActionSheetIcon::comment);

    editCallout.show (selectionArea, std::move (items));
}

void CsoundCodeEditor::hideTouchEditMenu()
{
    editCallout.hide();
}
// ------------------------------------------------------------------------------

void CsoundCodeEditor::focusLost (juce::Component::FocusChangeType cause)
{
    CodeView::focusLost (cause);

    // Un click fuori dall'editor (o fuori dalla finestra del plugin) fa
    // perdere il focus da tastiera: e' il segnale piu' affidabile per
    // "l'utente e' andato altrove", quindi chiudiamo il popup di
    // autocompletamento. La barra di help fissa invece resta com'e': non
    // e' un popup "flottante" che infastidisce, puo' restare a mostrare
    // l'ultimo opcode selezionato.
    hideSuggestions();
}

void CsoundCodeEditor::caretPositionMoved()
{
    // Click, tap, trascinamento o freccie: l'help della barra segue il
    // caret (prima era fatto in mouseDown/mouseUp, ora e' CodeView a
    // gestire mouse e touch e a notificare qui).
    updateOpcodeHelp();
}

//==============================================================================
// Drag and drop di uno slot dal pannello parametri (CsoundParameterMappingPanel::
// ParamRow/IntParamRow/BoolParamRow/ChoiceParamRow/GenericParamRow, vedi
// PluginEditor::parameterPanel) - vedi il commento in testa alla classe in
// CsoundCodeEditor.h. Il "description" del drag e' una juce::var stringa nel
// formato "csoundChannel:<nome canale>\x01<commento di configurazione>",
// creata dal mouseDrag di quelle classi - il separatore e' il carattere di
// controllo 0x01 (SOH), che non puo' comparire ne' in un nome canale digitato
// dall'utente ne' nel commento stesso (tutti e due testo semplice), cosi'
// channelDragSeparatorChar non e' mai ambiguo con il resto del contenuto. Il
// commento (puo' essere vuoto, es. canale non ancora assegnato) e' gia'
// completamente formattato ("SLIDER FLOAT: Min=0; Max=10; ...", vedi
// makeFloatConfigComment() ecc. in CsoundParameterEditor.cpp) - qui serve
// solo anteporgli ";" e metterlo su una riga propria PRIMA del chnget
// (commento Csound classico, non "/* */" - vedi itemDropped sotto).
namespace
{
    const juce::String channelDragPrefix = "csoundChannel:";
    constexpr juce_wchar channelDragSeparatorChar = 0x01;
}

bool CsoundCodeEditor::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.toString().startsWith (channelDragPrefix);
}

void CsoundCodeEditor::itemDragEnter (const SourceDetails& dragSourceDetails)
{
    itemDragMove (dragSourceDetails);
}

void CsoundCodeEditor::itemDragMove (const SourceDetails& dragSourceDetails)
{
    const auto newLine = getPositionAt (dragSourceDetails.localPosition.x, dragSourceDetails.localPosition.y).getLineNumber();

    if (newLine != dragHighlightLine)
    {
        dragHighlightLine = newLine;
        repaint();
    }
}

void CsoundCodeEditor::itemDragExit (const SourceDetails&)
{
    dragHighlightLine = -1;
    repaint();
}

void CsoundCodeEditor::itemDropped (const SourceDetails& dragSourceDetails)
{
    dragHighlightLine = -1;
    repaint();

    const auto afterPrefix = dragSourceDetails.description.toString().fromFirstOccurrenceOf (channelDragPrefix, false, false);
    const auto channelName = afterPrefix.upToFirstOccurrenceOf (juce::String::charToString (channelDragSeparatorChar), false, false);
    const auto configComment = afterPrefix.fromFirstOccurrenceOf (juce::String::charToString (channelDragSeparatorChar), false, false);

    if (channelName.isEmpty())
        return;

    // Riga puntata dal mouse al rilascio - il chnget viene inserito come
    // riga propria PRIMA di quella, non in mezzo al testo esistente a
    // quella colonna (vedi commento in CsoundCodeEditor.h).
    const auto dropPosition = getPositionAt (dragSourceDetails.localPosition.x, dragSourceDetails.localPosition.y);
    const int dropLine = dropPosition.getLineNumber();
    const juce::CodeDocument::Position lineStart (codeDocument, dropLine, 0);

    // Nome variabile derivato dal nome canale: prefisso "k" (control-rate -
    // il caso piu' comune per un parametro automatizzato) + nome canale
    // senza spazi, cosi' "Nakisano Poms" diventa "kNakisanoPoms".
    const auto varName = "k" + channelName.removeCharacters (" \t");

    // Stessa indentazione che avrebbe una riga nuova inserita li' (vedi
    // indentDepthBeforeLine, usata anche da handleReturnKey/reindentLine per
    // lo stesso scopo): cosi' un chnget trascinato dentro un instr risulta
    // gia' indentato correttamente, non sempre a colonna 0.
    const auto indent = juce::String::repeatedString (" ", indentSpaces * indentDepthBeforeLine (dropLine));

    // Commento di configurazione (richiesta esplicita): un commento Csound
    // CLASSICO (";...", non "/* */") su una riga PROPRIA, PRIMA del chnget
    // (stessa indentazione) - non accodato in coda alla stessa riga. Vuoto
    // (nessuna riga di commento) se configComment non conteneva nulla
    // (dovrebbe sempre contenerlo, essendo lo slot gia' assegnato -
    // channelName non sarebbe vuoto altrimenti).
    const auto commentLine = configComment.isNotEmpty() ? (indent + ";" + configComment + "\n") : juce::String();
    const auto lineText = commentLine + indent + varName + " chnget \"" + channelName + "\"\n";

    moveCaretTo (lineStart, false);
    insertTextAtCaret (lineText);
    grabKeyboardFocus();
}

void CsoundCodeEditor::paintOverChildren (juce::Graphics& g)
{
    // Occorrenze della ricerca (vedi setSearchQuery): solo quelle sulle
    // righe visibili. Riempimento SEMI-trasparente, cosi' il testo sotto
    // resta leggibile; la corrente e' piu' marcata e ha un bordo.
    if (! searchMatches.empty())
    {
        const int firstLine = getFirstLineOnScreen();
        const int lastLine  = firstLine + getNumLinesOnScreen() + 1;
        const float lineHeight = (float) getLineHeight();

        for (int i = 0; i < (int) searchMatches.size(); ++i)
        {
            const auto& m = searchMatches[(size_t) i];

            if (m.line < firstLine)
                continue;
            if (m.line > lastLine)
                break; // le occorrenze sono in ordine di posizione, quindi di riga

            const auto a = getCharacterBounds (juce::CodeDocument::Position (codeDocument, m.line, m.column));
            const auto b = getCharacterBounds (juce::CodeDocument::Position (codeDocument, m.line, m.column + (m.end - m.start)));
            const juce::Rectangle<float> r ((float) a.getX(), (float) a.getY(),
                                            (float) juce::jmax (2, b.getX() - a.getX()), lineHeight);

            if (i == currentSearchMatch)
            {
                g.setColour (juce::Colour (0xffff9f1c).withAlpha (0.45f));
                g.fillRoundedRectangle (r, 2.0f);
                g.setColour (juce::Colour (0xffe07b00));
                g.drawRoundedRectangle (r.reduced (0.5f), 2.0f, 1.5f);
            }
            else
            {
                g.setColour (juce::Colour (0xffffe066).withAlpha (0.45f));
                g.fillRoundedRectangle (r, 2.0f);
            }
        }
    }

    if (dragHighlightLine < 0)
        return;

    // Scroll a pixel (CodeView): la Y viene dalla geometria reale della riga.
    const auto y = getCharacterBounds (juce::CodeDocument::Position (codeDocument, dragHighlightLine, 0)).getY();

    g.setColour (juce::Colour (0xff4aa3b8).withAlpha (0.25f));
    g.fillRect (getGutterWidth(), y, getWidth() - getGutterWidth(), getLineHeight());
}

void CsoundCodeEditor::resized()
{
    CodeView::resized();

    // La posizione del popup e' calcolata rispetto al caret al momento in
    // cui viene mostrato: dopo un resize (es. l'utente ridimensiona la
    // finestra) potrebbe non essere piu' corretta, quindi lo si nasconde
    // piuttosto che lasciarlo disegnato nel posto sbagliato. Ricompare
    // comunque alla prossima digitazione.
    hideSuggestions();
}

//==============================================================================
// Riallineamento "live" delle keyword di chiusura blocco. Agganciato al
// CodeDocument (non a insertTextAtCaret) cosi' funziona indipendentemente
// da come il testo arriva sul documento: digitazione normale, IME, undo/
// redo, inserimento programmatico.
void CsoundCodeEditor::codeDocumentTextInserted (const juce::String& newText, int insertIndex)
{
    userOpcodesDirty = true;

    // Ricerca attiva: le occorrenze seguono il testo (anche durante un
    // re-indent, un undo o una sostituzione - tranne replaceAll, che
    // ricalcola una volta sola alla fine).
    if (searchQuery.isNotEmpty() && ! suspendSearchRefresh)
        refreshSearchMatches();

    if (isReindenting)
        return;

    // Un inserimento multi-riga (paste, replaceAllContent...) e' gestito
    // riga per riga da chi lo genera: qui ci occupiamo solo del caso
    // "l'utente ha appena completato una parola a inizio riga".
    if (! newText.containsChar ('\n'))
    {
        const juce::CodeDocument::Position pos (codeDocument, insertIndex);
        const int line = pos.getLineNumber();

        const auto trimmed = stripCommentAndTrim (codeDocument.getLine (line));
        const auto lower = trimmed.toLowerCase();

        static const juce::StringArray dedentWholeLine { "endin", "endif", "od", "endop", "else" };

        if (dedentWholeLine.contains (lower) || lower.startsWith ("elseif"))
            reindentLine (line);
    }

    updateOpcodeHelp();
}

void CsoundCodeEditor::codeDocumentTextDeleted (int /*startIndex*/, int /*endIndex*/)
{
    userOpcodesDirty = true;

    if (searchQuery.isNotEmpty() && ! suspendSearchRefresh)
        refreshSearchMatches();

    // Non serve reagire alle cancellazioni per il re-indent: si ricalcola
    // alla prossima riga toccata da un inserimento. L'help/autocompletamento
    // invece deve aggiornarsi anche qui (es. l'utente cancella l'ultima
    // lettera di un opcode riconosciuto e il popup deve sparire o passare
    // da "help completo" a "suggerimenti").
    updateOpcodeHelp();
}

void CsoundCodeEditor::reindentLine (int lineIndex)
{
    auto& doc = getDocument();
    const auto lineText = doc.getLine (lineIndex);

    int firstNonWs = 0;
    while (firstNonWs < lineText.length()
           && (lineText[firstNonWs] == ' ' || lineText[firstNonWs] == '\t'))
        ++firstNonWs;

    int depth = indentDepthBeforeLine (lineIndex);

    const auto trimmedContent = stripCommentAndTrim (lineText.substring (firstNonWs));

    if (startsWithDedentKeyword (trimmedContent))
        depth = juce::jmax (0, depth - 1);

    const auto newIndent     = juce::String::repeatedString (" ", indentSpaces * depth);
    const auto currentIndent = lineText.substring (0, firstNonWs);

    if (newIndent == currentIndent)
        return;

    const juce::CodeDocument::Position lineStart    (doc, lineIndex, 0);
    const juce::CodeDocument::Position contentStart (doc, lineIndex, firstNonWs);

    const int lineStartOffset    = lineStart.getPosition();
    const int contentStartOffset = contentStart.getPosition();
    const int caretOffsetFromContent = juce::jmax (0, getCaretPos().getPosition() - contentStartOffset);

    isReindenting = true;
    doc.replaceSection (lineStartOffset, contentStartOffset, newIndent);
    isReindenting = false;

    const juce::CodeDocument::Position newCaretPos (doc, lineStartOffset + newIndent.length() + caretOffsetFromContent);
    moveCaretTo (newCaretPos, false);
}

//==============================================================================
// Menu contestuale: Indent / Comment ---------------------------------------
juce::Range<int> CsoundCodeEditor::getSelectedLineRange() const
{
    const auto start = getSelectionStart();
    const auto end   = getSelectionEnd();

    int firstLine = start.getLineNumber();
    int lastLine  = end.getLineNumber();

    if (lastLine > firstLine && end.getIndexInLine() == 0)
        --lastLine;

    return { firstLine, lastLine + 1 };
}

void CsoundCodeEditor::indentSelectedLines()
{
    const auto lines = getSelectedLineRange();
    auto& doc = getDocument();

    doc.newTransaction();

    // Dalla prima all'ultima: ogni riga dipende solo da quelle PRIMA di se'
    // (indentDepthBeforeLine), gia' sistemate quando tocca a lei.
    for (int line = lines.getStart(); line < lines.getEnd() && line < doc.getNumLines(); ++line)
        reindentLine (line);

    doc.newTransaction();
}

bool CsoundCodeEditor::areSelectedLinesCommented() const
{
    const auto lines = getSelectedLineRange();
    auto& doc = getDocument();
    bool anyContent = false;

    for (int line = lines.getStart(); line < lines.getEnd() && line < doc.getNumLines(); ++line)
    {
        const auto text = doc.getLine (line).trim();

        if (text.isEmpty())
            continue;

        anyContent = true;

        if (! text.startsWith (";"))
            return false;
    }

    return anyContent;
}

void CsoundCodeEditor::toggleCommentOnSelectedLines()
{
    const auto lines = getSelectedLineRange();
    auto& doc = getDocument();
    const bool uncomment = areSelectedLinesCommented();

    // Commento: ";" inserito alla colonna di indentazione minima del blocco,
    // cosi' il codice resta allineato e si puo' togliere in modo pulito.
    int minIndent = std::numeric_limits<int>::max();

    for (int line = lines.getStart(); line < lines.getEnd() && line < doc.getNumLines(); ++line)
    {
        const auto text = doc.getLine (line);

        if (text.trim().isEmpty())
            continue;

        int ws = 0;
        while (ws < text.length() && (text[ws] == ' ' || text[ws] == '\t'))
            ++ws;

        minIndent = juce::jmin (minIndent, ws);
    }

    if (minIndent == std::numeric_limits<int>::max())
        return; // solo righe vuote

    doc.newTransaction();
    isReindenting = true; // niente re-indent automatico mentre si inserisce

    // Dall'ultima alla prima: gli offset delle righe precedenti non cambiano.
    for (int line = lines.getEnd() - 1; line >= lines.getStart(); --line)
    {
        if (line >= doc.getNumLines())
            continue;

        const auto text = doc.getLine (line);

        if (text.trim().isEmpty())
            continue;

        const int lineStart = juce::CodeDocument::Position (doc, line, 0).getPosition();

        if (uncomment)
        {
            int ws = 0;
            while (ws < text.length() && (text[ws] == ' ' || text[ws] == '\t'))
                ++ws;

            if (ws < text.length() && text[ws] == ';')
            {
                const int removeLen = (ws + 1 < text.length() && text[ws + 1] == ' ') ? 2 : 1;
                doc.deleteSection (lineStart + ws, lineStart + ws + removeLen);
            }
        }
        else
        {
            doc.insertText (lineStart + minIndent, "; ");
        }
    }

    isReindenting = false;
    doc.newTransaction();

    // Selezione estesa alle righe intere appena toccate, cosi' un secondo
    // toggle agisce sullo stesso blocco.
    const juce::CodeDocument::Position selStart (doc, lines.getStart(), 0);
    const juce::CodeDocument::Position selEnd   (doc, juce::jmin (lines.getEnd(), doc.getNumLines()), 0);
    moveCaretTo (selStart, false);
    moveCaretTo (selEnd, true);
}

//==============================================================================
// Help inline + autocompletamento sugli opcode ---------------------------
void CsoundCodeEditor::setOpcodeSignatures (const juce::Array<CsoundLiveOpcodeInfo>& signatures)
{
    liveSignatureLines.clear();
    allOpcodeNames.clearQuick();

    for (auto& info : signatures)
    {
        if (info.name.isEmpty())
            continue;

        const auto lower = info.name.toLowerCase();
        liveSignatureLines[lower].add (formatSignatureLine (info));

        if (! allOpcodeNames.contains (info.name))
            allOpcodeNames.add (info.name);
    }

    // Gli opcode del manuale restano sempre completabili anche se il motore
    // collegato non li riporta (o non ha ancora riportato nulla).
    for (auto& entry : CsoundOpcodeHelpData::table)
        allOpcodeNames.addIfNotAlreadyThere (juce::String (entry.name));

    allOpcodeNames.sort (true);
}

juce::String CsoundCodeEditor::wordEndingAt (const juce::String& lineText, int indexInLine)
{
    int start = juce::jlimit (0, lineText.length(), indexInLine);
    const int end = start;

    while (start > 0)
    {
        const auto c = lineText[start - 1];

        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '_')
            --start;
        else
            break;
    }

    return lineText.substring (start, end);
}

juce::String CsoundCodeEditor::wordAroundCaret (const juce::String& lineText, int indexInLine)
{
    // Come wordEndingAt, ma si espande anche dopo l'indice: usata per
    // riconoscere l'opcode "sotto" al caret indipendentemente da dove
    // esattamente e' caduto il click dentro la parola (non solo alla sua
    // fine, come nel caso "appena digitato").
    int start = juce::jlimit (0, lineText.length(), indexInLine);
    int end = start;

    while (start > 0 && (juce::CharacterFunctions::isLetterOrDigit (lineText[start - 1]) || lineText[start - 1] == '_'))
        --start;

    while (end < lineText.length() && (juce::CharacterFunctions::isLetterOrDigit (lineText[end]) || lineText[end] == '_'))
        ++end;

    return lineText.substring (start, end);
}

namespace
{
    // Traduce i caratteri-tipo compatti che Csound usa internamente per
    // descrivere gli argomenti di un opcode (gli stessi che
    // csoundNewOpcodeList riporta in outypes/intypes) in un prefisso di
    // nome di variabile Csound - a/k/i/S/f/w, esattamente come un
    // programma Csound reale chiamerebbe le proprie variabili di quel
    // rate. I significati di base sono documentati nel Csound Reference
    // Manual, capitolo "Types, Constants and Variables"
    // (csound.com/docs/manual/OrchKvar.html, linkato da
    // https://csound.com/docs/manual/index.html): a = audio, k = control,
    // i = init, S = string, f/w = spettrale. I codici per gli argomenti
    // opzionali/multipli (o/p/q/v/j/h per i valori i-rate opzionali con
    // default diversi, x/s per "control o audio" - x e' anche il prefisso
    // che Csound stesso usa per questi argomenti, es. "xamp" - z/Z/y/m/n
    // per liste di lunghezza variabile, B/b per i booleani) sono una
    // convenzione interna stabile di Csound, non descritta opcode per
    // opcode nel manuale ma tradotta qui allo stesso modo.
    juce::String typeCharPrefix (juce::juce_wchar c)
    {
        switch (c)
        {
            case 'a': return "a";
            case 'k': case 'z': case 'B': return "k";
            case 'i': case 'b': case 'm': case 'n':
            case 'o': case 'p': case 'q': case 'v': case 'j': case 'h':
                return "i";
            case 'S': return "S";
            case 'f': return "f";
            case 'w': return "w";
            case 'x': case 's': case 'Z': case 'y': return "x";
            default: return juce::String::charToString (c);
        }
    }

    bool isOptionalTypeChar (juce::juce_wchar c)
    {
        return c == 'o' || c == 'p' || c == 'q' || c == 'v' || c == 'j' || c == 'h';
    }

    // Costruisce un elenco di nomi di variabile fittizi ma plausibili
    // (a1, a2, k3...) da una stringa di caratteri-tipo, cosi' come il
    // manuale elencherebbe argomenti con nomi reali (xamp, kcps...): qui i
    // nomi sono generici perche' csoundNewOpcodeList riporta solo i tipi,
    // non i nomi originali degli argomenti.
    juce::StringArray namedArgsFromTypes (const juce::String& types, bool& outHasOptional)
    {
        juce::StringArray names;
        outHasOptional = false;

        for (int i = 0; i < types.length(); ++i)
        {
            const auto c = types[i];

            // "k[]" (array, es. nelle UDO) -> il nome precedente diventa "k1[]"
            if (c == '[' || c == ']')
            {
                if (c == '[' && ! names.isEmpty())
                    names.set (names.size() - 1, names[names.size() - 1] + "[]");

                continue;
            }

            names.add (typeCharPrefix (c) + juce::String (names.size() + 1));

            if (isOptionalTypeChar (c))
                outHasOptional = true;
        }

        return names;
    }

    // Unisce righe di sintassi multi-variante che differiscono SOLO nel
    // tipo dell'outlet (es. "ares linen xamp, irise, idur, idec" / "kres
    // linen kamp, irise, idur, idec") in una sola riga non ambigua: "xres
    // linen ..." se le varianti sono esattamente a-rate/k-rate (stessa
    // convenzione "x" che Csound usa per gli argomenti in ingresso), oppure
    // "ares/kres/ires ..." con lo slash (non la virgola, che farebbe
    // sembrare un elenco di argomenti distinti) per qualunque altra
    // combinazione. Se le righe differiscono anche nel resto (non solo
    // nell'outlet - es. poscil, dove anche gli argomenti cambiano nome/rate
    // da riga a riga), o se l'opcode non ha un outlet (procedurale),
    // ripiega sulla prima riga: un confronto piu' fine andrebbe oltre quel
    // che una singola riga di help puo' mostrare.
    juce::String combineOutletVariants (const juce::String& multilineSyntax, const juce::String& opcodeName)
    {
        auto lines = juce::StringArray::fromLines (multilineSyntax);
        lines.trim();
        lines.removeEmptyStrings();

        if (lines.isEmpty())
            return {};

        if (lines.size() == 1)
            return lines[0];

        const auto lowerName = opcodeName.toLowerCase();
        juce::StringArray outTokens;
        juce::String restOfFirstLine;

        for (int i = 0; i < lines.size(); ++i)
        {
            const auto words = juce::StringArray::fromTokens (lines[i], " ", "");
            int opcodeIdx = -1;

            for (int w = 0; w < words.size(); ++w)
            {
                if (words[w].toLowerCase() == lowerName)
                {
                    opcodeIdx = w;
                    break;
                }
            }

            // Serve un token prima del nome opcode (l'outlet) perche' la
            // combinazione abbia senso.
            if (opcodeIdx <= 0)
                return lines[0];

            outTokens.addIfNotAlreadyThere (words[0]);

            juce::String rest = words[opcodeIdx];

            for (int w = opcodeIdx + 1; w < words.size(); ++w)
                rest << " " << words[w];

            if (i == 0)
                restOfFirstLine = rest;
            else if (rest != restOfFirstLine)
                return lines[0]; // il resto della firma cambia troppo: niente combinazione
        }

        if (outTokens.size() <= 1)
            return lines[0];

        // "ares, kres" con la virgola e' ambigua (sembra un elenco di DUE
        // argomenti distinti, non due varianti alternative dello stesso
        // outlet): usiamo "/" per dire "l'uno O l'altro", come fa Csound
        // stesso per gli argomenti in ingresso di tipo "x" (xamp = a-rate O
        // k-rate). Se le varianti sono ESATTAMENTE a-rate e k-rate (nessuna
        // i-rate), le collassiamo in un unico token con prefisso "x" invece
        // di scriverle entrambe per intero (es. "xres" invece di
        // "ares/kres") - e' la stessa convenzione che Csound usa per gli
        // argomenti in ingresso, qui applicata anche all'outlet.
        juce::String rateLetters;
        juce::String suffix; // es. "res" in "ares"/"kres" - assunto uguale per ogni variante

        for (auto& token : outTokens)
        {
            if (token.isNotEmpty())
            {
                rateLetters += token.substring (0, 1).toLowerCase();

                if (suffix.isEmpty())
                    suffix = token.substring (1);
            }
        }

        if (rateLetters.length() == 2 && rateLetters.containsChar ('a') && rateLetters.containsChar ('k'))
            return "x" + suffix + " " + restOfFirstLine;

        return outTokens.joinIntoString ("/") + " " + restOfFirstLine;
    }
}

juce::String CsoundCodeEditor::formatSignatureLine (const CsoundLiveOpcodeInfo& info)
{
    // Sintetizza una riga di sintassi nello STESSO formato del Csound
    // Reference Manual (outlet[, outlet] opcode inlet, inlet[, opzionali]),
    // solo con nomi di argomento generici invece di quelli originali (che
    // csoundNewOpcodeList non riporta - solo i tipi). E' garantita
    // corretta nei tipi perche' letta direttamente dal motore collegato, e
    // copre letteralmente ogni opcode disponibile (incluse le varianti
    // polimorfiche di uno stesso opcode, ciascuna una riga).
    bool unusedFlag = false;
    const auto outNames = namedArgsFromTypes (info.outTypes, unusedFlag);

    bool hasOptionalIn = false;
    const auto inNames = namedArgsFromTypes (info.inTypes, hasOptionalIn);

    juce::String line;

    if (! outNames.isEmpty())
        line << outNames.joinIntoString (", ") << " ";

    line << info.name;

    if (! inNames.isEmpty())
    {
        if (! hasOptionalIn)
        {
            line << " " << inNames.joinIntoString (", ");
        }
        else
        {
            // Gli argomenti opzionali in Csound sono sempre in coda:
            // raggruppiamo tutti quelli da quel punto in poi in un'unica
            // parentesi quadra, come fa il manuale (es. "[, ifn, iphs]").
            juce::StringArray required, optional;

            for (int i = 0, n = 0; i < info.inTypes.length(); ++i)
            {
                const auto c = info.inTypes[i];

                if (c == '[' || c == ']')
                    continue;

                (isOptionalTypeChar (c) ? optional : required).add (inNames[n++]);
            }

            line << " ";

            if (! required.isEmpty())
                line << required.joinIntoString (", ");

            line << " [, " << optional.joinIntoString (", ") << "]";
        }
    }

    return line;
}

void CsoundCodeEditor::updateOpcodeHelp()
{
    auto& doc = getDocument();
    const auto caret = getCaretPos();
    const int line = caret.getLineNumber();

    const auto lineText = doc.getLine (line);
    const auto fullWord   = wordAroundCaret (lineText, caret.getIndexInLine());
    const auto prefixWord = wordEndingAt   (lineText, caret.getIndexInLine());

    currentSuggestions.clear();

    // In modalita' parametri la barra resta sull'opcode appena inserito e
    // non compaiono suggerimenti mentre si saltano i token.
    if (parameterMode)
    {
        if (caret.getLineNumber() != parameterLine)
            exitParameterMode();
        else
        {
            hideSuggestions();
            pushSignatureHelp (parameterOpcode);
            return;
        }
    }

    if (fullWord.isEmpty())
    {
        clearOpcodeHelp();
        return;
    }

    const auto lowerFull = fullWord.toLowerCase();

    rescanUserOpcodesIfNeeded();

    // 1) Il caret e' su (dentro o alla fine di) un opcode conosciuto -
    //    che ci sia arrivato digitando, cliccando, o con le freccie -
    //    mostra l'help completo nella barra dedicata.
    const bool isKnownOpcode = findUserOpcode (fullWord) != nullptr
                             || liveSignatureLines.find (lowerFull) != liveSignatureLines.end()
                             || CsoundOpcodeHelpData::find (fullWord) != nullptr;

    if (isKnownOpcode)
    {
        hideSuggestions();
        pushSignatureHelp (fullWord);
        return;
    }

    // 2) Nessuna corrispondenza esatta: se il caret e' alla fine della
    //    parola (non nel mezzo: non ha senso autocompletare cliccando a
    //    meta' di qualcosa che non esiste) e sono stati digitati almeno 2
    //    caratteri, suggerisci i nomi di opcode che iniziano con quel
    //    prefisso (fino a 8, ordine alfabetico).
    if (prefixWord.length() == fullWord.length() && prefixWord.length() >= 2)
        buildSuggestions (prefixWord, 12);

    if (currentSuggestions.isEmpty())
    {
        clearOpcodeHelp();
        return;
    }

    showSuggestions (prefixWord);
}

void CsoundCodeEditor::pushSignatureHelp (const juce::String& word)
{
    juce::String syntax, description, category;

    if (const auto* udo = findUserOpcode (word))
    {
        // UDO del documento corrente: firma ricostruita dalla riga
        // "opcode Nome, outtypes, intypes" (nomi di argomento generici,
        // come per gli opcode riportati solo dal motore).
        syntax = formatSignatureLine ({ udo->name, udo->outTypes, udo->inTypes });
        description = "User-defined opcode, declared at line " + juce::String (udo->line + 1) + " of this file.";
        category = "User-defined opcode";
    }
    else if (const auto* manual = CsoundOpcodeHelpData::find (word))
    {
        // Sintassi del manuale (nomi di argomento reali), descrizione
        // "refpurpose" e categoria: il caso migliore. Le varianti che
        // differiscono solo nell'outlet (ares/kres) vengono fuse in una
        // riga da combineOutletVariants(); altrimenti resta la prima.
        syntax = combineOutletVariants (manual->syntax, manual->name);
        description = manual->description;
        category = manual->category;
    }
    else
    {
        // Opcode non presente nel manuale (es. plugin di terze parti): la
        // riga di sintassi e' la firma di tipo letta dal motore (sempre
        // corretta nei tipi, ma con nomi di argomento generici) e non c'e'
        // descrizione.
        const auto it = liveSignatureLines.find (word.toLowerCase());

        if (it != liveSignatureLines.end() && ! it->second.isEmpty())
            syntax = combineOutletVariants (it->second.joinIntoString ("\n"), word);
        else
            syntax = word;
    }

    // Costrutti del linguaggio (instr/endin, opcode/endop, if/then, goto,
    // xin/xout, intestazione) non hanno una forma funzionale: restano come
    // nel manuale anche con la sintassi moderna attiva.
    static const juce::StringArray noFunctionalForm { "xin", "xout", "goto", "igoto", "kgoto", "tigoto",
                                                      "cigoto", "ckgoto", "cggoto", "cngoto", "cnkgoto", "rigoto",
                                                      "reinit", "rireturn", "timout", "loop_lt", "loop_le",
                                                      "loop_gt", "loop_ge" };

    if (modernSyntaxHelp && ! category.startsWith ("Orchestra Syntax") && ! noFunctionalForm.contains (word))
        syntax = toModernSyntax (syntax, word);

    helpBarHasContent = true;

    if (onOpcodeHelpChanged)
        onOpcodeHelpChanged (syntax, description, category);
}

void CsoundCodeEditor::setModernSyntaxHelp (bool shouldUseModernSyntax)
{
    if (modernSyntaxHelp == shouldUseModernSyntax)
        return;

    modernSyntaxHelp = shouldUseModernSyntax;
    updateOpcodeHelp();
}

juce::String CsoundCodeEditor::toModernSyntax (const juce::String& manualLine, const juce::String& opcodeName)
{
    // "outs opcode ins" -> "outs:T = opcode(ins)" (Csound 7: assegnazione
    // funzionale con annotazione di tipo sulle variabili di uscita).
    // Righe gia' in forma di assegnazione o senza il nome dell'opcode
    // come token a se' (instr, if/then, "0dbfs = iarg"...) restano come sono.
    const auto line = manualLine.trim();

    if (line.isEmpty() || line.containsChar ('=') || line.containsChar ('('))
        return manualLine;

    const auto words = juce::StringArray::fromTokens (line, " ", "");
    int opcodeIdx = -1;

    for (int w = 0; w < words.size(); ++w)
    {
        if (words[w].equalsIgnoreCase (opcodeName))
        {
            opcodeIdx = w;
            break;
        }
    }

    if (opcodeIdx < 0)
        return manualLine;

    juce::String outs, ins;

    for (int w = 0; w < opcodeIdx; ++w)
        outs << (w > 0 ? " " : "") << words[w];

    for (int w = opcodeIdx + 1; w < words.size(); ++w)
        ins << (w > opcodeIdx + 1 ? " " : "") << words[w];

    // Annotazione di tipo per ogni variabile di uscita: "ares" -> "ares:a",
    // "gkres" -> "gkres:k", "kval[]" -> "kval:k[]", "xres" (a oppure k)
    // -> "xres:a/k". Token non riconoscibili (es. "[, a2 [...]]") restano
    // invariati.
    auto annotate = [] (const juce::String& token) -> juce::String
    {
        auto t = token.trim();
        bool isArray = false;

        if (t.endsWith ("[]"))
        {
            isArray = true;
            t = t.dropLastCharacters (2);
        }

        if (t.isEmpty() || ! t.containsOnly ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"))
            return token;

        auto rate = t.startsWith ("g") && t.length() > 1 ? t[1] : t[0];
        juce::String rateText;

        switch (rate)
        {
            case 'a': case 'k': case 'i': case 'S': case 'f': case 'w':
                rateText = juce::String::charToString (rate);
                break;
            case 'x':
                rateText = "a/k";
                break;
            default:
                return token;
        }

        return t + ":" + rateText + (isArray ? "[]" : "");
    };

    juce::String result;

    if (outs.isNotEmpty())
    {
        juce::StringArray outTokens;
        outTokens.addTokens (outs, ",", "");

        for (int i = 0; i < outTokens.size(); ++i)
            result << (i > 0 ? ", " : "") << annotate (outTokens[i]);

        result << " = ";
    }

    result << words[opcodeIdx] << "(" << ins.trim() << ")";
    return result;
}

void CsoundCodeEditor::rescanUserOpcodesIfNeeded()
{
    if (! userOpcodesDirty)
        return;

    userOpcodesDirty = false;
    userOpcodes.clearQuick();

    auto& doc = getDocument();
    const int numLines = doc.getNumLines();

    for (int i = 0; i < numLines; ++i)
    {
        const auto trimmed = stripCommentAndTrim (doc.getLine (i));

        if (! trimmed.startsWith ("opcode"))
            continue;

        // "opcode Nome, outtypes, intypes" - outtypes/intypes possono
        // essere 0 (nessun argomento) o mancare del tutto in codice
        // ancora incompleto.
        auto rest = trimmed.substring (6).trim();

        if (rest.isEmpty() || ! (juce::CharacterFunctions::isLetterOrDigit (rest[0]) || rest[0] == '_'))
            continue;

        juce::StringArray parts;
        parts.addTokens (rest, ",", "");
        parts.trim();

        UserOpcode udo;
        udo.name = parts[0];
        udo.outTypes = parts.size() > 1 && parts[1] != "0" ? parts[1] : juce::String();
        udo.inTypes  = parts.size() > 2 && parts[2] != "0" ? parts[2] : juce::String();
        udo.line = i;

        if (udo.name.isNotEmpty() && findUserOpcode (udo.name) == nullptr)
            userOpcodes.add (udo);
    }
}

const CsoundCodeEditor::UserOpcode* CsoundCodeEditor::findUserOpcode (const juce::String& word) const
{
    for (auto& udo : userOpcodes)
        if (udo.name == word)
            return &udo;

    return nullptr;
}

void CsoundCodeEditor::clearOpcodeHelp()
{
    hideSuggestions();

    if (helpBarHasContent)
    {
        helpBarHasContent = false;

        if (onOpcodeHelpChanged)
            onOpcodeHelpChanged ({}, {}, {});
    }
}

void CsoundCodeEditor::showSuggestions (const juce::String& prefix)
{
    suggestionPopup.suggestionPrefix = prefix;
    suggestionPopup.rows.clear();
    suggestionPopup.widestRow = 0;

    for (auto& s : currentSuggestions)
    {
        const auto row = makeSuggestionRow (s.name, s.syntax);
        suggestionPopup.rows.add (row);
        suggestionPopup.widestRow = juce::jmax (suggestionPopup.widestRow, row.length());
    }

    suggestionPopup.selectedIndex = 0; // ogni nuovo filtro riparte dal primo

    positionSuggestionPopup();
    pushSignatureHelpForSelectedSuggestion();
}

void CsoundCodeEditor::pushSignatureHelpForSelectedSuggestion()
{
    // Mentre si naviga la lista dei suggerimenti (freccie, hover del
    // mouse, o semplicemente apparendo con il primo elemento già
    // selezionato) la barra di help deve seguire il suggerimento
    // evidenziato, non restare legata all'ultima parola scritta per
    // intero nel testo - altrimenti durante la digitazione di un prefisso
    // (es. "osc") la barra resterebbe vuota finche' non si accetta un
    // suggerimento, invece di anticipare gia' la firma di quello
    // evidenziato (esattamente come fa CsoundQt).
    if (suggestionPopup.selectedIndex >= 0 && suggestionPopup.selectedIndex < currentSuggestions.size())
        pushSignatureHelp (currentSuggestions.getReference (suggestionPopup.selectedIndex).name);
}

bool CsoundCodeEditor::forceShowSuggestions()
{
    // Invocazione manuale (tasto Esc quando il popup e' chiuso): stessa
    // logica di filtro usata durante la digitazione, ma senza il minimo di
    // 2 caratteri (qui l'utente ha chiesto esplicitamente l'elenco, quindi
    // anche un solo carattere e' un prefisso valido).
    auto& doc = getDocument();
    const auto caret = getCaretPos();
    const auto lineText = doc.getLine (caret.getLineNumber());
    const auto prefixWord = wordEndingAt (lineText, caret.getIndexInLine());

    if (prefixWord.isEmpty())
        return false;

    rescanUserOpcodesIfNeeded();
    buildSuggestions (prefixWord, 12);

    if (currentSuggestions.isEmpty())
        return false;

    showSuggestions (prefixWord);
    return true;
}

//==============================================================================
// Suggerimenti per VARIANTE di sintassi (stile CsoundQt) ------------------
juce::StringArray CsoundCodeEditor::getSyntaxVariants (const juce::String& name)
{
    juce::StringArray lines;

    if (const auto* udo = findUserOpcode (name))
    {
        lines.add (formatSignatureLine ({ udo->name, udo->outTypes, udo->inTypes }));
        return lines;
    }

    if (const auto* manual = CsoundOpcodeHelpData::find (name))
    {
        lines = juce::StringArray::fromLines (manual->syntax);
        lines.trim();
        lines.removeEmptyStrings();

        if (! lines.isEmpty())
            return lines;
    }

    const auto it = liveSignatureLines.find (name.toLowerCase());

    if (it != liveSignatureLines.end() && ! it->second.isEmpty())
        return it->second;

    lines.add (name);
    return lines;
}

void CsoundCodeEditor::buildSuggestions (const juce::String& prefixWord, int maxRows)
{
    currentSuggestions.clear();

    juce::StringArray names;

    // Prima le UDO del file (poche e "vicine" a chi scrive), poi il resto.
    for (auto& udo : userOpcodes)
        if (udo.name.startsWithIgnoreCase (prefixWord) && ! udo.name.equalsIgnoreCase (prefixWord))
            names.addIfNotAlreadyThere (udo.name);

    for (auto& name : allOpcodeNames)
        if (name.startsWithIgnoreCase (prefixWord) && ! name.equalsIgnoreCase (prefixWord))
            names.addIfNotAlreadyThere (name);

    for (auto& name : names)
    {
        for (auto& syntax : getSyntaxVariants (name))
        {
            currentSuggestions.add ({ name, syntax });

            if (currentSuggestions.size() >= maxRows)
                return;
        }
    }
}

juce::String CsoundCodeEditor::makeSuggestionRow (const juce::String& name, const juce::String& syntax)
{
    // Come CsoundQt: "nome          inlet, inlet [, opz]     :rate"
    const auto words = juce::StringArray::fromTokens (syntax.trim(), " ", "");
    int opcodeIdx = -1;

    for (int w = 0; w < words.size(); ++w)
        if (words[w].equalsIgnoreCase (name)) { opcodeIdx = w; break; }

    juce::String outs, ins;

    if (opcodeIdx >= 0)
    {
        for (int w = 0; w < opcodeIdx; ++w)       outs << (w > 0 ? " " : "") << words[w];
        for (int w = opcodeIdx + 1; w < words.size(); ++w) ins << (w > opcodeIdx + 1 ? " " : "") << words[w];
    }

    juce::String row = name.length() < 14 ? name.paddedRight (' ', 14) : name + " ";

    if (ins.isNotEmpty())
        row << (ins.length() <= 30 ? ins : ins.substring (0, 29) + juce::String::charToString (0x2026));

    if (outs.isNotEmpty())
    {
        if (row.length() < 46)
            row = row.paddedRight (' ', 46);

        const auto rate = outs.startsWith ("g") && outs.length() > 1 ? outs.substring (1, 2) : outs.substring (0, 1);
        row << " :" << (rate == "f" ? juce::String ("pvs") : rate);
    }

    return row;
}

void CsoundCodeEditor::enterParameterMode (int line)
{
    parameterMode = true;
    parameterLine = line;
}

void CsoundCodeEditor::exitParameterMode()
{
    if (! parameterMode)
        return;

    parameterMode = false;
    parameterLine = -1;
    parameterOpcode.clear();
}

bool CsoundCodeEditor::selectParameterToken (bool forward)
{
    // Token = sequenza di caratteri senza separatori; una stringa "..." e'
    // un token unico. Parte dal caret (o dalla selezione corrente).
    auto& doc = getDocument();
    const int line = getCaretPos().getLineNumber();

    if (line != parameterLine)
        return false;

    const auto text = doc.getLine (line).trimEnd();
    const int len = text.length();

    auto isSep = [] (juce::juce_wchar c)
    {
        return c == ' ' || c == '\t' || c == ',' || c == '(' || c == ')' || c == '[' || c == ']' || c == '\\' || c == '=';
    };

    auto tokenEndFrom = [&] (int start) -> int
    {
        if (start < len && text[start] == '"')
        {
            int e = start + 1;
            while (e < len && text[e] != '"') ++e;
            return juce::jmin (len, e + 1);
        }

        int e = start;
        while (e < len && ! isSep (text[e])) ++e;
        return e;
    };

    int start = -1;

    if (forward)
    {
        int i = juce::jmax (getSelectionStart().getIndexInLine(), getSelectionEnd().getIndexInLine());

        // fine del token corrente, poi salta i separatori
        if (i < len && ! isSep (text[i]) && i > 0 && ! isSep (text[i - 1]))
            i = tokenEndFrom (i);

        while (i < len && isSep (text[i])) ++i;

        if (i >= len)
            return false;

        start = i;
    }
    else
    {
        int i = juce::jmin (getSelectionStart().getIndexInLine(), getSelectionEnd().getIndexInLine()) - 1;

        while (i >= 0 && isSep (text[i])) --i;

        if (i < 0)
            return false;

        if (text[i] == '"')
        {
            int s = i - 1;
            while (s >= 0 && text[s] != '"') --s;
            start = juce::jmax (0, s);
        }
        else
        {
            int s = i;
            while (s > 0 && ! isSep (text[s - 1])) --s;
            start = s;
        }
    }

    const int end = tokenEndFrom (start);

    if (end <= start)
        return false;

    selectingParameter = true;
    selectRegion (juce::CodeDocument::Position (doc, line, start), juce::CodeDocument::Position (doc, line, end));
    selectingParameter = false;
    return true;
}

void CsoundCodeEditor::mouseDown (const juce::MouseEvent& e)
{
    exitParameterMode(); // come CsoundQt: un click esce dalla modalita' parametri
    CodeView::mouseDown (e);
}

void CsoundCodeEditor::moveSuggestionSelection (int delta)
{
    const int count = currentSuggestions.size();

    if (count == 0)
        return;

    suggestionPopup.selectedIndex = ((suggestionPopup.selectedIndex + delta) % count + count) % count;
    suggestionPopup.repaint();
    pushSignatureHelpForSelectedSuggestion();
}

void CsoundCodeEditor::hideSuggestions()
{
    currentSuggestions.clear();
    suggestionPopup.setVisible (false);
}

void CsoundCodeEditor::positionSuggestionPopup()
{
    const auto size = suggestionPopup.computeSize();
    const auto caretRect = getCaretRectangle();

    int x = caretRect.getX();
    int y = caretRect.getBottom() + 4;

    x = juce::jlimit (0, juce::jmax (0, getWidth() - size.x), x);

    // Se non c'e' spazio sotto il caret, mostra il popup sopra invece che
    // tagliato fuori dal bordo inferiore dell'editor.
    if (y + size.y > getHeight())
        y = juce::jmax (0, caretRect.getY() - size.y - 4);

    suggestionPopup.setBounds (x, y, size.x, size.y);
    suggestionPopup.toFront (false);
    suggestionPopup.setVisible (true);
}

void CsoundCodeEditor::acceptSuggestion (Suggestion suggestion)
{
    // Come CsoundQt: la parola parziale viene sostituita dall'INTERA
    // sintassi ("ares oscil xamp, xcps [, ifn, iphs]", o la forma
    // funzionale se e' attiva la sintassi moderna). Se sulla riga c'e' gia'
    // del testo prima della parola (es. "aout = " o un outlet scritto a
    // mano) si inserisce dal nome dell'opcode in poi, senza gli outlet.
    const auto caret = getCaretPos();
    const int line = caret.getLineNumber();
    const auto lineText = codeDocument.getLine (line);
    const auto word = wordEndingAt (lineText, caret.getIndexInLine());

    if (! suggestion.name.startsWithIgnoreCase (word))
        return;

    const int wordStartIndex = caret.getIndexInLine() - word.length();
    const bool textBefore = lineText.substring (0, wordStartIndex).trim().isNotEmpty();

    juce::String syntax = suggestion.syntax.trim();

    if (modernSyntaxHelp)
        syntax = toModernSyntax (syntax, suggestion.name).trim();

    juce::String toInsert = syntax;

    if (textBefore)
    {
        if (syntax.contains (" = "))
            toInsert = syntax.fromFirstOccurrenceOf (" = ", false, false).trim();
        else
        {
            // dal nome dell'opcode (come token intero) in poi
            const auto words = juce::StringArray::fromTokens (syntax, " ", "");
            juce::String rest;

            for (int w = 0, found = 0; w < words.size(); ++w)
            {
                if (! found && words[w].equalsIgnoreCase (suggestion.name))
                    found = 1;

                if (found)
                    rest << (rest.isEmpty() ? "" : " ") << words[w];
            }

            if (rest.isNotEmpty())
                toInsert = rest;
        }
    }

    hideSuggestions();

    // Modalita' parametri (attivata PRIMA dell'inserimento, cosi' la barra
    // di help resta su questo opcode e non ricompaiono suggerimenti per
    // l'ultimo token inserito): il primo token della parte inserita viene
    // selezionato, Tab passa ai successivi.
    const bool withParameters = toInsert.containsAnyOf (" ,(");

    if (withParameters)
    {
        parameterOpcode = suggestion.name;
        enterParameterMode (line);
    }

    // Sostituisce la parola parziale con la sintassi (un solo passo di undo).
    selectRegion (juce::CodeDocument::Position (codeDocument, line, wordStartIndex), caret);
    insertTextAtCaret (toInsert);

    if (withParameters)
    {
        moveCaretTo (juce::CodeDocument::Position (codeDocument, line, wordStartIndex), false);
        selectParameterToken (true); // dal caret all'inizio del primo token: lo seleziona
        pushSignatureHelp (suggestion.name);
    }
}

//==============================================================================
CsoundCodeEditor::SuggestionPopup::SuggestionPopup()
{
    // Deve poter ricevere hover/click (per la navigazione col mouse), ma
    // senza MAI diventare focalizzabile da tastiera: niente
    // setWantsKeyboardFocus(true), niente grabKeyboardFocus da nessuna
    // parte in questa classe.
    setInterceptsMouseClicks (true, false);
}

juce::Point<int> CsoundCodeEditor::SuggestionPopup::computeSize() const
{
    // Larghezza in base alla riga piu' lunga (font monospaziato 13 px:
    // ~7.8 px per carattere), con un minimo.
    const int width = juce::jmax (240, 16 + juce::roundToInt ((float) widestRow * 7.8f));

    return { width, 8 + rows.size() * lineHeight };
}

int CsoundCodeEditor::SuggestionPopup::rowIndexAtY (int y) const
{
    const auto index = (y - 6) / lineHeight;
    return (index >= 0 && index < rows.size()) ? index : -1;
}

void CsoundCodeEditor::SuggestionPopup::mouseMove (const juce::MouseEvent& event)
{
    const auto index = rowIndexAtY (event.position.getY());

    if (index >= 0 && onHoverIndex)
        onHoverIndex (index);
}

void CsoundCodeEditor::SuggestionPopup::mouseDown (const juce::MouseEvent& event)
{
    const auto index = rowIndexAtY (event.position.getY());

    if (index >= 0 && onClickIndex)
        onClickIndex (index);
}

void CsoundCodeEditor::SuggestionPopup::mouseExit (const juce::MouseEvent&)
{
    // Lasciamo l'ultima riga evidenziata com'e' (tipicamente quella
    // raggiunta per ultima da tastiera o mouse): uscire col mouse non deve
    // "deselezionare" tutto, altrimenti Invio/Tab non avrebbero piu' un
    // suggerimento ovvio da accettare.
}

void CsoundCodeEditor::SuggestionPopup::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Tono caldo (quasi "post-it"), volutamente diverso dal bianco
    // dell'editor sottostante, cosi' il popup si distingue a colpo
    // d'occhio senza bisogno di un'ombra.
    g.setColour (juce::Colour (0xfffdf6e3));
    g.fillRect (bounds);

    g.setColour (juce::Colour (0xffd7c89a));
    g.drawRect (bounds.reduced (0.5f), 1.0f);

    auto area = getLocalBounds().reduced (8, 6);

    const auto monoFont = juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));

    for (int i = 0; i < rows.size(); ++i)
    {
        auto lineArea = area.removeFromTop (lineHeight);

        if (i == selectedIndex)
        {
            g.setColour (juce::Colour (0xffe8d9a8));
            g.fillRect (lineArea.expanded (4, 0));
        }

        const auto& name = rows[i];

        g.setFont (monoFont);
        g.setColour (juce::Colour (0xff5b4636));
        g.drawText (suggestionPrefix, lineArea, juce::Justification::centredLeft, false);

        // Font::getStringWidth/-Float non sono disponibili in questa
        // versione di JUCE: la via raccomandata per misurare il testo e'
        // GlyphArrangement.
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (monoFont, suggestionPrefix, 0.0f, 0.0f);
        const auto prefixWidth = (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth());
        auto remainderArea = lineArea.withTrimmedLeft (prefixWidth);

        g.setColour (juce::Colour (0xff8a7a55));
        g.drawText (name.substring (suggestionPrefix.length()), remainderArea, juce::Justification::centredLeft, false);
    }
}

//==============================================================================
// Cerca / Sostituisci - vedi il commento su setSearchQuery in CsoundCodeEditor.h.
void CsoundCodeEditor::setSearchQuery (const juce::String& query, bool matchCase, bool wholeWord)
{
    searchQuery = query;
    searchMatchCase = matchCase;
    searchWholeWord = wholeWord;
    currentSearchMatch = -1;

    refreshSearchMatches();

    // "Cerca mentre scrivi": porta subito in vista l'occorrenza scelta.
    if (currentSearchMatch >= 0)
        selectSearchMatch (currentSearchMatch);
}

void CsoundCodeEditor::clearSearch()
{
    searchQuery.clear();
    searchMatches.clear();
    currentSearchMatch = -1;
    repaint();
    notifySearchResults();
}

void CsoundCodeEditor::refreshSearchMatches()
{
    searchMatches.clear();

    if (searchQuery.isEmpty())
    {
        currentSearchMatch = -1;
        repaint();
        notifySearchResults();
        return;
    }

    // Il testo come array di caratteri (indici = posizioni di
    // CodeDocument): juce::String e' UTF-8, l'accesso per indice sarebbe
    // lineare a ogni chiamata.
    std::vector<juce::juce_wchar> text, query;
    {
        const auto content = codeDocument.getAllContent();
        for (auto p = content.getCharPointer(); ! p.isEmpty(); ++p)
            text.push_back (*p);
        for (auto p = searchQuery.getCharPointer(); ! p.isEmpty(); ++p)
            query.push_back (*p);
    }

    auto fold = [this] (juce::juce_wchar c)
    {
        return searchMatchCase ? c : juce::CharacterFunctions::toLowerCase (c);
    };

    auto isWordChar = [] (juce::juce_wchar c)
    {
        return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
    };

    const int n = (int) text.size(), m = (int) query.size();
    int line = 0, lineStart = 0, scanned = 0;

    for (int i = 0; i + m <= n; )
    {
        int k = 0;
        while (k < m && fold (text[(size_t) (i + k)]) == fold (query[(size_t) k]))
            ++k;

        bool found = (k == m);

        if (found && searchWholeWord)
            found = (i == 0 || ! isWordChar (text[(size_t) (i - 1)]))
                 && (i + m == n || ! isWordChar (text[(size_t) (i + m)]));

        if (! found)
        {
            ++i;
            continue;
        }

        for (; scanned < i; ++scanned)
            if (text[(size_t) scanned] == '\n')
            {
                ++line;
                lineStart = scanned + 1;
            }

        searchMatches.push_back ({ i, i + m, line, i - lineStart });
        i += m; // occorrenze non sovrapposte
    }

    // Occorrenza corrente: la prima che inizia alla selezione/caret o dopo,
    // cosi' "Next" e la digitazione nel campo proseguono da dove si e'.
    currentSearchMatch = -1;

    if (! searchMatches.empty())
    {
        const auto selection = getHighlightedRegion();
        const int anchor = selection.isEmpty() ? getCaretPos().getPosition() : selection.getStart();

        currentSearchMatch = 0;
        for (int i = 0; i < (int) searchMatches.size(); ++i)
            if (searchMatches[(size_t) i].start >= anchor)
            {
                currentSearchMatch = i;
                break;
            }
    }

    repaint();
    notifySearchResults();
}

void CsoundCodeEditor::selectSearchMatch (int index)
{
    if (index < 0 || index >= (int) searchMatches.size())
        return;

    currentSearchMatch = index;
    const auto& m = searchMatches[(size_t) index];

    // selectRegion sposta anche il caret, e quindi la vista, sull'occorrenza.
    selectRegion (juce::CodeDocument::Position (codeDocument, m.start),
                  juce::CodeDocument::Position (codeDocument, m.end));
    repaint();
    notifySearchResults();
}

void CsoundCodeEditor::selectNextSearchMatch()
{
    if (searchMatches.empty())
        return;

    // Se la corrente e' gia' selezionata si passa alla successiva,
    // altrimenti (l'utente ha spostato il caret) si seleziona quella scelta
    // dal caret.
    const auto selection = getHighlightedRegion();
    const auto& cur = searchMatches[(size_t) juce::jmax (0, currentSearchMatch)];
    const bool onCurrent = selection.getStart() == cur.start && selection.getEnd() == cur.end;

    selectSearchMatch (onCurrent ? (currentSearchMatch + 1) % (int) searchMatches.size()
                                 : juce::jmax (0, currentSearchMatch));
}

void CsoundCodeEditor::selectPreviousSearchMatch()
{
    if (searchMatches.empty())
        return;

    const int count = (int) searchMatches.size();
    selectSearchMatch ((juce::jmax (0, currentSearchMatch) - 1 + count) % count);
}

bool CsoundCodeEditor::replaceCurrentSearchMatch (const juce::String& replacement)
{
    if (currentSearchMatch < 0 || currentSearchMatch >= (int) searchMatches.size())
        return false;

    const auto m = searchMatches[(size_t) currentSearchMatch];
    const int resumeFrom = m.start + replacement.length();

    // Una transazione dedicata = un solo passo di undo.
    codeDocument.newTransaction();
    codeDocument.replaceSection (m.start, m.end, replacement);
    codeDocument.newTransaction();

    // Le occorrenze sono gia' state ricalcolate dal listener: si passa alla
    // prima DOPO il testo appena inserito (evita di risostituire dentro la
    // sostituzione stessa, es. "a" -> "aa").
    for (int i = 0; i < (int) searchMatches.size(); ++i)
        if (searchMatches[(size_t) i].start >= resumeFrom)
        {
            selectSearchMatch (i);
            return true;
        }

    if (! searchMatches.empty())
        selectSearchMatch (0);
    else
        notifySearchResults();

    return true;
}

int CsoundCodeEditor::replaceAllSearchMatches (const juce::String& replacement)
{
    if (searchMatches.empty())
        return 0;

    const auto matches = searchMatches; // copia: il listener e' sospeso, ma per sicurezza
    const int count = (int) matches.size();

    suspendSearchRefresh = true;
    codeDocument.newTransaction();

    // Dall'ultima alla prima: gli indici delle precedenti restano validi.
    for (int i = count - 1; i >= 0; --i)
        codeDocument.replaceSection (matches[(size_t) i].start, matches[(size_t) i].end, replacement);

    codeDocument.newTransaction();
    suspendSearchRefresh = false;

    refreshSearchMatches();
    return count;
}
