#include "CsoundCodeEditor.h"

CsoundCodeEditor::CsoundCodeEditor (juce::CodeDocument& doc, juce::CodeTokeniser* tok)
    : juce::CodeEditorComponent (doc, tok)
{
    setTabSize (indentSpaces, true); // Tab = 4 spazi, mai caratteri tab reali.
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

    // Chiamata esplicita alla base class: e' il vero inserimento di testo,
    // non deve ripassare dal nostro override (che gestisce l'auto-dedent).
    CodeEditorComponent::insertTextAtCaret ("\n" + newIndent);
}

void CsoundCodeEditor::insertTextAtCaret (const juce::String& textToInsert)
{
    CodeEditorComponent::insertTextAtCaret (textToInsert);

    // Auto-dedent "live": se l'utente ha appena completato, a inizio riga,
    // una keyword di chiusura blocco (endin/endif/od/endop/else/elseif),
    // riallinea subito l'indentazione della riga - stesso comportamento
    // di Xcode/VS Code quando si digita una parentesi di chiusura.
    if (textToInsert.isEmpty() || textToInsert.containsChar ('\n'))
        return;

    const auto caret = getCaretPos();
    const int line = caret.getLineNumber();
    const auto trimmed = stripCommentAndTrim (getDocument().getLine (line));
    const auto lower = trimmed.toLowerCase();

    static const juce::StringArray dedentWholeLine { "endin", "endif", "od", "endop", "else" };

    if (dedentWholeLine.contains (lower) || lower.startsWith ("elseif "))
        reindentLine (line);
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

    doc.replaceSection (lineStartOffset, contentStartOffset, newIndent);

    const juce::CodeDocument::Position newCaretPos (doc, lineStartOffset + newIndent.length() + caretOffsetFromContent);
    moveCaretTo (newCaretPos, false);
}
