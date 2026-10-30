#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

/**
    "Jump to Definition" dell'editor (Cmd+clic, come Xcode): dato il testo e
    una posizione (riga, colonna), trova dove e' definito cio' che c'e'
    sotto il cursore. Logica pura, riga per riga, senza compilatore - vedi
    Tests/CsoundUnitTests.cpp.

      variabile locale    prima riga che la SCRIVE nello stesso instr/opcode
                          (kenv madsr ..., kenv = ..., kenv init ...,
                          kenv:k = ..., ia, kb xin)
      variabile globale   (gk..., ga..., gi..., gS..., gf...) prima riga che la
                          scrive nel file
      UDO                 la riga "opcode Nome, ..."
      strumento           "instr N" / "instr Nome" da event "i", N / schedule
                          / scoreline / righe di score "i N ..." / "Nome"
      macro               $NOME -> #define NOME
      canale              "Nome" in chnget/chnset/chn_k: la dichiarazione
                          chn_k, altrimenti il primo chnget/chnset
      etichetta           goto / kgoto / igoto / reinit ... -> "nome:"

    I commenti (";", "//" e i blocchi a stella) e le stringhe non contano come codice.
*/
namespace csoundNav
{
    struct Target
    {
        int line = -1;        // 0-based; -1 = niente da raggiungere
        int column = 0;       // inizio del nome sulla riga di destinazione
        int length = 0;
        juce::String kind;    // "variable", "global", "opcode", "instrument", "macro", "channel", "label"
        bool isValid() const noexcept { return line >= 0; }
    };

    /** Le righe del testo con commenti e contenuto delle stringhe sostituiti
        da spazi (le colonne restano le stesse). keepStrings: le stringhe
        restano (servono per canali e strumenti con nome). */
    inline juce::StringArray stripComments (const juce::StringArray& lines, bool keepStrings)
    {
        juce::StringArray out;
        bool inBlock = false;
        for (const auto& raw : lines)
        {
            juce::String line;
            bool inString = false;
            const int n = raw.length();
            for (int i = 0; i < n; ++i)
            {
                const auto c = raw[i];
                if (inBlock)
                {
                    if (c == '*' && i + 1 < n && raw[i + 1] == '/') { inBlock = false; line << "  "; ++i; }
                    else line << " ";
                    continue;
                }
                if (inString)
                {
                    if (c == '"') { inString = false; line << '"'; }
                    else line << (keepStrings ? juce::String::charToString (c) : juce::String (" "));
                    continue;
                }
                if (c == '"') { inString = true; line << '"'; continue; }
                if (c == ';') break;
                if (c == '/' && i + 1 < n && raw[i + 1] == '/') break;
                if (c == '/' && i + 1 < n && raw[i + 1] == '*') { inBlock = true; line << "  "; ++i; continue; }
                line << juce::String::charToString (c);
            }
            out.add (line);
        }
        return out;
    }

    inline bool isIdentStart (juce::juce_wchar c) { return juce::CharacterFunctions::isLetter (c) || c == '_'; }
    inline bool isIdentChar  (juce::juce_wchar c) { return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_'; }

    /** La parola (identificatore o numero) alla colonna col: [start, end). */
    inline bool wordAt (const juce::String& line, int col, int& start, int& end)
    {
        const int n = line.length();
        if (n == 0) return false;
        col = juce::jlimit (0, n, col);
        auto isWord = [] (juce::juce_wchar c) { return isIdentChar (c) || c == '.'; };
        if (col == n || ! isWord (line[col]))
        {
            if (col > 0 && isWord (line[col - 1])) --col;
            else return false;
        }
        start = col; end = col;
        while (start > 0 && isWord (line[start - 1])) --start;
        while (end < n && isWord (line[end])) ++end;
        // un punto e' parte di un numero (10.1), non di un nome
        auto word = line.substring (start, end);
        if (! juce::CharacterFunctions::isDigit (word[0]) && word.containsChar ('.'))
        {
            start = col; end = col;
            while (start > 0 && isIdentChar (line[start - 1])) --start;
            while (end < n && isIdentChar (line[end])) ++end;
        }
        return end > start;
    }

    /** Le variabili SCRITTE da una riga di codice (senza commenti): gli
        identificatori a sinistra di "=" / "init" / di un opcode. isOpcode
        dice se una parola e' un opcode (del manuale, del motore o un UDO). */
    inline juce::StringArray outputsOfLine (const juce::String& codeLine,
                                            const std::function<bool (const juce::String&)>& isOpcode)
    {
        juce::StringArray outs;
        auto line = codeLine.trim();
        // etichetta "nome:" in testa
        {
            int p = 0;
            while (p < line.length() && isIdentChar (line[p])) ++p;
            if (p > 0 && p < line.length() && line[p] == ':' && (p + 1 >= line.length() || line[p + 1] == ' ' || line[p + 1] == '\t'))
                line = line.substring (p + 1).trim();
        }
        if (line.isEmpty())
            return outs;

        static const juce::StringArray keywords { "if", "elseif", "else", "endif", "then", "while", "until", "do", "od",
                                                  "instr", "endin", "opcode", "endop", "goto", "igoto", "kgoto",
                                                  "return", "rireturn", "fi", "switch", "case", "default", "endsw" };

        // lista di uscite "a, b:k, c[]"
        auto parseOutputList = [] (const juce::String& lhs, juce::StringArray& into) -> bool
        {
            juce::StringArray parts;
            parts.addTokens (lhs, ",", "");
            for (auto p : parts)
            {
                p = p.trim();
                if (p.containsChar ('[')) p = p.upToFirstOccurrenceOf ("[", false, false).trim();
                if (p.containsChar (':')) p = p.upToFirstOccurrenceOf (":", false, false).trim();
                if (p.isEmpty() || ! isIdentStart (p[0])) return false;
                for (auto c : p) if (! isIdentChar (c)) return false;
                into.add (p);
            }
            return ! into.isEmpty();
        };

        // 1) assegnazione "lhs = rhs" (non ==, <=, >=, !=, +=...)
        for (int i = 0; i < line.length(); ++i)
        {
            const auto c = line[i];
            if (c == '"' || c == '(') break;
            if (c == '=')
            {
                const auto prev = i > 0 ? line[i - 1] : juce::juce_wchar (' ');
                const auto next = i + 1 < line.length() ? line[i + 1] : juce::juce_wchar (' ');
                if (next == '=' || prev == '=' || prev == '<' || prev == '>' || prev == '!'
                    || prev == '+' || prev == '-' || prev == '*' || prev == '/')
                    break;
                juce::StringArray lhs;
                if (parseOutputList (line.substring (0, i), lhs))
                {
                    const auto first = lhs[0];
                    if (! keywords.contains (first))
                        return lhs;
                }
                return outs;
            }
        }

        // 2) "uscite opcode argomenti": la prima parola dopo la lista e' un opcode
        int p = 0;
        juce::String lhsText;
        while (p < line.length())
        {
            // un identificatore (con :tipo / [] facoltativi)
            const int s = p;
            if (! isIdentStart (line[p])) return outs;
            while (p < line.length() && (isIdentChar (line[p]) || line[p] == ':' || line[p] == '[' || line[p] == ']')) ++p;
            lhsText << line.substring (s, p);
            while (p < line.length() && (line[p] == ' ' || line[p] == '\t')) ++p;
            if (p < line.length() && line[p] == ',')
            {
                lhsText << ",";
                ++p;
                while (p < line.length() && (line[p] == ' ' || line[p] == '\t')) ++p;
                continue;
            }
            break;
        }
        // la parola successiva
        const int s = p;
        while (p < line.length() && isIdentChar (line[p])) ++p;
        const auto opcode = line.substring (s, p);   // anche opcode:k (il tipo viene dopo)
        if (opcode.isEmpty())
            return outs;
        juce::StringArray lhs;
        if (! parseOutputList (lhsText, lhs) || keywords.contains (lhs[0]))
            return outs;
        if (opcode == "init" || (isOpcode && isOpcode (opcode)))
            return lhs;
        return outs;
    }

    /** Righe di inizio/fine del blocco instr/opcode che contiene 'line'
        (-1, -1 se e' nell'header globale). */
    inline void enclosingBlock (const juce::StringArray& code, int line, int& from, int& to)
    {
        from = -1; to = -1;
        for (int i = juce::jmin (line, code.size() - 1); i >= 0; --i)
        {
            const auto t = code[i].trim();
            if (t.startsWith ("endin") || t.startsWith ("endop"))
            {
                if (i == line) continue;
                return;
            }
            if ((t.startsWith ("instr") || t.startsWith ("opcode")) && (t.length() == 5 || t.length() == 6 || ! isIdentChar (t[t.startsWith ("instr") ? 5 : 6])))
            {
                from = i;
                break;
            }
        }
        if (from < 0) return;
        to = code.size() - 1;
        for (int i = from + 1; i < code.size(); ++i)
        {
            const auto t = code[i].trim();
            if (t.startsWith ("endin") || t.startsWith ("endop")) { to = i; break; }
        }
    }

    /** Colonna del nome 'word' come parola intera nella riga (-1 se assente). */
    inline int findWord (const juce::String& line, const juce::String& word, int fromCol = 0)
    {
        for (int at = line.indexOf (fromCol, word); at >= 0; at = line.indexOf (at + 1, word))
        {
            const bool leftOk  = at == 0 || ! isIdentChar (line[at - 1]);
            const bool rightOk = at + word.length() >= line.length() || ! isIdentChar (line[at + word.length()]);
            if (leftOk && rightOk) return at;
        }
        return -1;
    }

    /** Riga "instr ..." che dichiara il numero o il nome dato. */
    inline Target findInstrument (const juce::StringArray& code, const juce::String& numberOrName)
    {
        Target t;
        const bool numeric = juce::CharacterFunctions::isDigit (numberOrName[0]);
        const auto key = numeric ? juce::String (numberOrName.getIntValue()) : numberOrName;
        for (int i = 0; i < code.size(); ++i)
        {
            const auto& l = code[i];
            const auto trimmed = l.trimStart();
            if (! trimmed.startsWith ("instr") || (trimmed.length() > 5 && isIdentChar (trimmed[5])))
                continue;
            const int base = l.length() - trimmed.length() + 5;
            juce::StringArray names;
            names.addTokens (l.substring (base), ", \t", "");
            names.removeEmptyStrings();
            for (const auto& n : names)
                if (n == key || (numeric && n.getIntValue() == key.getIntValue() && juce::CharacterFunctions::isDigit (n[0])))
                {
                    t.line = i; t.column = findWord (l, n, base); t.length = n.length(); t.kind = "instrument";
                    if (t.column < 0) t.column = base;
                    return t;
                }
        }
        return t;
    }

    /** Dove e' definito cio' che sta alla posizione (line, col) di text. */
    inline Target findDefinition (const juce::String& text, int line, int col,
                                  const std::function<bool (const juce::String&)>& isOpcode)
    {
        Target none;
        juce::StringArray raw;
        raw.addLines (text);
        if (! juce::isPositiveAndBelow (line, raw.size()))
            return none;

        const auto code    = stripComments (raw, false);   // niente stringhe: per variabili/opcode
        const auto withStr = stripComments (raw, true);    // con le stringhe: canali, strumenti con nome
        const auto& here = withStr[line];
        if (col > here.length())
            return none;   // dentro un commento (tolto)

        // --- stringa sotto il cursore: canale o strumento con nome --------
        {
            int open = -1;
            bool inStr = false;
            for (int i = 0; i < here.length() && i < col + 1; ++i)
                if (here[i] == '"') { inStr = ! inStr; open = inStr ? i : -1; }
            if (inStr && open >= 0 && col > open)
            {
                const int close = here.indexOfChar (open + 1, '"');
                if (close > col || close < 0)
                {
                    const auto name = here.substring (open + 1, close < 0 ? here.length() : close).trim();
                    if (name.isEmpty()) return none;
                    // strumento con nome?
                    auto instr = findInstrument (code, name.trimCharactersAtStart ("-"));
                    if (instr.isValid()) return instr;
                    // canale: chn_k, poi il primo chnget/chnset
                    const auto quoted = "\"" + name + "\"";
                    for (int pass = 0; pass < 2; ++pass)
                        for (int i = 0; i < withStr.size(); ++i)
                        {
                            const auto& l = withStr[i];
                            const int at = l.indexOf (quoted);
                            if (at < 0) continue;
                            const bool decl = findWord (l, "chn_k") >= 0 || findWord (l, "chn_S") >= 0 || findWord (l, "chn_a") >= 0;
                            const bool use  = l.contains ("chnget") || l.contains ("chnset") || l.contains ("chnmix") || decl;
                            if ((pass == 0 && decl) || (pass == 1 && use && i != line))
                            {
                                Target t; t.line = i; t.column = at + 1; t.length = name.length(); t.kind = "channel";
                                return t;
                            }
                        }
                    return none;
                }
            }
        }

        // --- parola sotto il cursore -------------------------------------
        int s = 0, e = 0;
        if (! wordAt (code[line], col, s, e))
            return none;
        const auto word = code[line].substring (s, e);

        // macro $NOME
        if (s > 0 && code[line][s - 1] == '$')
        {
            for (int i = 0; i < raw.size(); ++i)
            {
                const auto t = raw[i].trimStart();
                if (t.startsWith ("#define"))
                {
                    const int at = findWord (raw[i], word);
                    if (at >= 0) { Target r; r.line = i; r.column = at; r.length = word.length(); r.kind = "macro"; return r; }
                }
            }
            return none;
        }

        // numero: strumento, se la riga lancia eventi (o e' una riga di score)
        if (juce::CharacterFunctions::isDigit (word[0]))
        {
            const auto l = code[line].trim();
            const bool eventLine = l.startsWithChar ('i') || withStr[line].contains ("event") || withStr[line].contains ("schedule")
                                   || withStr[line].contains ("schedkwhen") || withStr[line].contains ("scoreline")
                                   || withStr[line].contains ("turnoff2") || withStr[line].contains ("massign")
                                   || withStr[line].contains ("alwayson");
            if (eventLine)
                return findInstrument (code, word);
            return none;
        }

        // "i10" in una riga di score
        if (word.length() > 1 && word[0] == 'i' && juce::CharacterFunctions::isDigit (word[1]) && s == code[line].length() - code[line].trimStart().length())
            return findInstrument (code, word.substring (1));

        // UDO: "opcode Nome, uscite, ingressi"
        for (int i = 0; i < code.size(); ++i)
        {
            const auto t = code[i].trimStart();
            if (! t.startsWith ("opcode") || t.length() <= 6 || isIdentChar (t[6]))
                continue;
            const auto name = t.substring (6).trimStart().upToFirstOccurrenceOf (",", false, false).trim();
            if (name == word && i != line)
            {
                Target r; r.line = i; r.column = juce::jmax (0, findWord (code[i], word)); r.length = word.length(); r.kind = "opcode";
                return r;
            }
        }

        // etichetta di un salto
        {
            static const juce::StringArray jumps { "goto", "igoto", "kgoto", "tigoto", "reinit", "cggoto", "cigoto", "ckgoto",
                                                   "cngoto", "timout", "rigoto", "loop_lt", "loop_le", "loop_gt", "loop_ge",
                                                   "looplt", "loople", "loopgt", "loopge" };
            bool isJumpTarget = false;
            for (const auto& j : jumps)
            {
                const int at = findWord (code[line], j);
                if (at >= 0 && at < s) { isJumpTarget = true; break; }
            }
            if (isJumpTarget)
            {
                int from, to; enclosingBlock (code, line, from, to);
                const int a = from < 0 ? 0 : from, b = to < 0 ? code.size() - 1 : to;
                for (int i = a; i <= b; ++i)
                {
                    const auto t = code[i].trimStart();
                    if (t.startsWith (word + ":"))
                    {
                        Target r; r.line = i; r.column = code[i].length() - t.length(); r.length = word.length(); r.kind = "label";
                        return r;
                    }
                }
            }
        }

        // strumento con nome scritto senza virgolette (raro): "instr Nome"
        // non e' una variabile - salta oltre.

        // variabile: prima riga che la scrive, nel blocco (locale) o nel file
        const bool global = word.length() > 1 && word[0] == 'g' && juce::String ("ikaSfw").containsChar (word[1]);
        auto searchRange = [&] (int a, int b) -> Target
        {
            for (int i = a; i <= b; ++i)
            {
                const auto outs = outputsOfLine (code[i], isOpcode);
                if (outs.contains (word))
                {
                    Target r; r.line = i; r.column = juce::jmax (0, findWord (code[i], word)); r.length = word.length();
                    r.kind = global ? "global" : "variable";
                    return r;
                }
            }
            return {};
        };

        if (! global)
        {
            int from, to; enclosingBlock (code, line, from, to);
            if (from >= 0)
            {
                auto r = searchRange (from, to);
                if (r.isValid()) return r;
            }
        }
        auto r = searchRange (0, code.size() - 1);   // globale (o locale fuori dai blocchi)
        if (r.isValid()) return r;

        // un nome di strumento usato come parola (nstrnum Nome?) o niente
        return findInstrument (code, word);
    }
} // namespace csoundNav
