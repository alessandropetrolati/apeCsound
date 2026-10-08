#include "CsoundCodeEditor.h"
#include "CsoundOpcodePurposeData.h"
#include <cmath>

CsoundCodeEditor::CsoundCodeEditor (juce::CodeDocument& doc, juce::CodeTokeniser* tok)
    : juce::CodeEditorComponent (doc, tok),
      codeDocument (doc)
{
    setTabSize (indentSpaces, true); // Tab = 4 spazi, mai caratteri tab reali.
    codeDocument.addListener (this);

    // Autocompletamento disponibile da subito, prima ancora che il motore
    // Csound sia mai partito: seed con la tabella scritta a mano, che poi
    // setOpcodeSignatures() sostituisce/amplia con l'elenco reale.
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
            acceptSuggestion (currentSuggestions[index]);
            grabKeyboardFocus(); // il click e' andato al popup, non all'editor
        }
    };
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

    CodeEditorComponent::insertTextAtCaret ("\n" + newIndent);
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

    if (key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
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
            acceptSuggestion (currentSuggestions[suggestionPopup.selectedIndex]);
            return true;
        }
    }

    const bool handled = CodeEditorComponent::keyPressed (key);

    // Qualunque altro tasto (freccie, Home/End, Page Up/Down...) puo' aver
    // spostato il caret senza passare da codeDocumentTextInserted/Deleted:
    // ricalcoliamo l'help cosi' "portare la selezione sull'opcode" con la
    // tastiera funziona esattamente come cliccarci sopra o digitarlo.
    updateOpcodeHelp();

    return handled;
}

void CsoundCodeEditor::mouseDown (const juce::MouseEvent& event)
{
    CodeEditorComponent::mouseDown (event);
    updateOpcodeHelp();
}

void CsoundCodeEditor::mouseUp (const juce::MouseEvent& event)
{
    CodeEditorComponent::mouseUp (event);
    updateOpcodeHelp();
}

void CsoundCodeEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Lo scroll di default di juce::CodeEditorComponent passa la mano a
    // juce::ScrollBar::mouseWheelMove, che moltiplica wheel.deltaY per un
    // fattore 10 FISSO e lo applica senza alcun limite. Con un trackpad,
    // durante lo slancio di uno swipe (la fase "inertial" gestita dal
    // sistema - una sequenza di eventi con deltaY che il sistema stesso fa
    // salire e scendere da solo) il risultato e' un salto di decine o
    // centinaia di righe per singolo evento: su un file lungo diventa
    // impossibile fermarsi dove si vuole, esattamente il problema lamentato
    // (la stessa sensazione "accelerata" dell'editor di Cabbage).
    //
    // Qui scaliamo deltaY in modo lineare ma con un fattore molto piu'
    // contenuto, e soprattutto limitiamo (jlimit) il numero massimo di
    // righe per singolo evento: la velocita' percepita resta quindi
    // prevedibile e limitata, indipendentemente da quanto e' lungo il file
    // o da quanto forte e' lo swipe. wheelScrollRemainder accumula la parte
    // frazionaria (e l'eventuale eccesso tagliato dal limite) tra un evento
    // e il successivo, cosi' uno scroll lento e preciso resta fluido (non
    // si "perde" nulla sotto la soglia di una riga) e uno scroll rapido
    // viene spalmato su piu' eventi invece di saltare in un colpo solo.
    //
    // IMPORTANTE (il vero motivo per cui restava "incontrollabile" anche
    // con questo limite): senza un tetto anche su wheelScrollRemainder,
    // durante uno swipe forte su trackpad macOS (che manda MOLTI eventi in
    // rapida sequenza, ciascuno gia' con un deltaY grande per conto suo)
    // l'accumulo entrava piu' velocemente di quanto il limite per evento
    // riuscisse a scaricarlo: il "debito" di righe cresceva senza fondo e
    // continuava a scaricarsi per secondi anche DOPO che l'utente aveva
    // fermato il gesto, durante la sola fase di decelerazione inerziale -
    // la sensazione di "accelerazione fuori controllo" lamentata. Il
    // jlimit sul remainder sotto impedisce che quel debito superi mai un
    // singolo scatto (maxLinesPerEvent): lo scroll si ferma non appena si
    // ferma il gesto, non dopo.
    if (! juce::approximatelyEqual (wheel.deltaY, 0.0f))
    {
        constexpr float linesPerNotch     = 1.2f;
        constexpr int   maxLinesPerEvent  = 3;

        wheelScrollRemainder = juce::jlimit (-(float) maxLinesPerEvent, (float) maxLinesPerEvent,
                                              wheelScrollRemainder + wheel.deltaY * linesPerNotch);

        const int wholeLines = (int) wheelScrollRemainder; // troncato verso zero
        const int clampedLines = juce::jlimit (-maxLinesPerEvent, maxLinesPerEvent, wholeLines);

        wheelScrollRemainder -= (float) clampedLines;

        if (clampedLines != 0)
        {
            // Non scrollBy(-clampedLines) diretto (BUG corretto: "scroll
            // extra testo", cioe' overscroll oltre la fine del file) -
            // CodeEditorComponent::scrollToLineInternal (JUCE) limita
            // firstLineOnScreen SOLO a [0, numLines - 1], SENZA tener conto
            // di quante righe stanno a schermo (getNumLinesOnScreen()):
            // l'ULTIMA riga del file puo' quindi finire in cima
            // all'editor, lasciando sotto uno spazio vuoto mai occupato da
            // testo. Lo scroll "di serie" (passato allo ScrollBar) non
            // soffre di questo perche' la ScrollBar stessa clampa la sua
            // posizione corrente al range disponibile - qui bypassavamo
            // quel clamp chiamando scrollBy()/scrollToLine() direttamente,
            // quindi dobbiamo rifare noi lo stesso clamp "non oltre la
            // fine" prima di applicarlo.
            const int maxFirstLine = juce::jmax (0, getDocument().getNumLines() - getNumLinesOnScreen());
            const int targetFirstLine = juce::jlimit (0, maxFirstLine, getFirstLineOnScreen() - clampedLines);
            scrollToLine (targetFirstLine);
            return;
        }
    }

    CodeEditorComponent::mouseWheelMove (event, wheel);
}

void CsoundCodeEditor::focusLost (juce::Component::FocusChangeType cause)
{
    CodeEditorComponent::focusLost (cause);

    // Un click fuori dall'editor (o fuori dalla finestra del plugin) fa
    // perdere il focus da tastiera: e' il segnale piu' affidabile per
    // "l'utente e' andato altrove", quindi chiudiamo il popup di
    // autocompletamento. La barra di help fissa invece resta com'e': non
    // e' un popup "flottante" che infastidisce, puo' restare a mostrare
    // l'ultimo opcode selezionato.
    hideSuggestions();
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
    if (dragHighlightLine < 0)
        return;

    const auto y = (dragHighlightLine - getFirstLineOnScreen()) * getLineHeight();

    g.setColour (juce::Colour (0xff17a2b8).withAlpha (0.25f));
    g.fillRect (0, y, getWidth(), getLineHeight());
}

void CsoundCodeEditor::resized()
{
    CodeEditorComponent::resized();

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

    // Se per qualche motivo il motore non ha ancora riportato nulla (non
    // dovrebbe succedere dopo csoundEngineStarted, ma meglio non restare
    // con l'autocompletamento vuoto), ripiega sulla tabella scritta a mano.
    if (allOpcodeNames.isEmpty())
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
            names.add (typeCharPrefix (types[i]) + juce::String (i + 1));

            if (isOptionalTypeChar (types[i]))
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

            for (int i = 0; i < info.inTypes.length(); ++i)
                (isOptionalTypeChar (info.inTypes[i]) ? optional : required).add (inNames[i]);

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

    if (fullWord.isEmpty())
    {
        clearOpcodeHelp();
        return;
    }

    const auto lowerFull = fullWord.toLowerCase();

    // 1) Il caret e' su (dentro o alla fine di) un opcode conosciuto -
    //    che ci sia arrivato digitando, cliccando, o con le freccie -
    //    mostra l'help completo nella barra dedicata.
    const bool isKnownOpcode = liveSignatureLines.find (lowerFull) != liveSignatureLines.end()
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
    {
        for (auto& name : allOpcodeNames)
        {
            if (name.startsWithIgnoreCase (prefixWord) && ! name.equalsIgnoreCase (prefixWord))
            {
                currentSuggestions.add (name);

                if (currentSuggestions.size() >= 8)
                    break;
            }
        }
    }

    if (currentSuggestions.isEmpty())
    {
        clearOpcodeHelp();
        return;
    }

    showSuggestions (prefixWord);
}

void CsoundCodeEditor::pushSignatureHelp (const juce::String& word)
{
    juce::String syntax, description;

    if (const auto* curated = CsoundOpcodeHelpData::find (word))
    {
        // Sintassi "umana" e descrizione in prosa, entrambe verificate contro
        // il file XML sorgente dell'opcode nel manuale: il caso migliore.
        // Alcuni opcode polimorfici hanno piu' righe di sintassi che
        // differiscono SOLO nel tipo dell'outlet (es. linen: "ares linen
        // xamp, irise, idur, idec" / "kres linen kamp, irise, idur, idec"):
        // unirle tutte con "|" era ridondante (si leggeva due volte quasi
        // la stessa firma), ma troncare alla prima riga perdeva
        // informazione reale (si sa solo che esiste la a-rate, non che
        // esiste anche la k-rate). combineOutletVariants() qui sotto
        // unisce i tipi di outlet in un unico token non ambiguo ("xres" se
        // sono esattamente a/k, altrimenti "ares/kres/ires" con lo slash)
        // mantenendo un'unica lista di argomenti.
        syntax = combineOutletVariants (curated->syntax, curated->name);
        description = curated->description;
    }
    else
    {
        // Nessuna sintassi "umana" per questo opcode: la riga di sintassi
        // resta la firma di tipo grezza letta dal motore (sempre corretta
        // perche' non e' scritta a mano, e per definizione copre anche le
        // varianti che io non ho trascritto a mano dal manuale). Per la
        // descrizione, se l'opcode compare nell'indice del Csound
        // Reference Manual (vedi CsoundOpcodePurposeData, trascritto dalla
        // riga "refpurpose" del manuale - lo stesso testo che CsoundQt
        // mostra come help rapido) usiamo quella riga, univoca per ogni
        // opcode; altrimenti la descrizione resta vuota.
        // formatSignatureLine include gia' il nome dell'opcode: se e'
        // polimorfico (es. oscil esiste sia a a-rate sia a k-rate) ogni
        // variante e' una riga separata - stessa combinazione per-outlet
        // usata sopra, invece di "|" ridondanti o di troncare alla prima.
        const auto it = liveSignatureLines.find (word.toLowerCase());

        if (it != liveSignatureLines.end() && ! it->second.isEmpty())
            syntax = combineOutletVariants (it->second.joinIntoString ("\n"), word);
        else
            syntax = word;

        if (const auto* purpose = CsoundOpcodePurposeDataNS::find (word))
            description = purpose;
    }

    helpBarHasContent = true;

    if (onOpcodeHelpChanged)
        onOpcodeHelpChanged (syntax, description);
}

void CsoundCodeEditor::clearOpcodeHelp()
{
    hideSuggestions();

    if (helpBarHasContent)
    {
        helpBarHasContent = false;

        if (onOpcodeHelpChanged)
            onOpcodeHelpChanged ({}, {});
    }
}

void CsoundCodeEditor::showSuggestions (const juce::String& prefix)
{
    suggestionPopup.suggestionPrefix = prefix;
    suggestionPopup.suggestionNames = currentSuggestions;
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
        pushSignatureHelp (currentSuggestions[suggestionPopup.selectedIndex]);
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

    currentSuggestions.clear();

    for (auto& name : allOpcodeNames)
    {
        if (name.startsWithIgnoreCase (prefixWord) && ! name.equalsIgnoreCase (prefixWord))
        {
            currentSuggestions.add (name);

            if (currentSuggestions.size() >= 8)
                break;
        }
    }

    if (currentSuggestions.isEmpty())
        return false;

    showSuggestions (prefixWord);
    return true;
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

void CsoundCodeEditor::acceptSuggestion (const juce::String& fullName)
{
    const auto caret = getCaretPos();
    const auto lineText = codeDocument.getLine (caret.getLineNumber());
    const auto word = wordEndingAt (lineText, caret.getIndexInLine());

    if (! fullName.startsWithIgnoreCase (word))
        return;

    insertTextAtCaret (fullName.substring (word.length()));
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
    constexpr int width = 240;

    return { width, 8 + suggestionNames.size() * lineHeight };
}

int CsoundCodeEditor::SuggestionPopup::rowIndexAtY (int y) const
{
    const auto index = (y - 6) / lineHeight;
    return (index >= 0 && index < suggestionNames.size()) ? index : -1;
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

    for (int i = 0; i < suggestionNames.size(); ++i)
    {
        auto lineArea = area.removeFromTop (lineHeight);

        if (i == selectedIndex)
        {
            g.setColour (juce::Colour (0xffe8d9a8));
            g.fillRect (lineArea.expanded (4, 0));
        }

        const auto& name = suggestionNames[i];

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
