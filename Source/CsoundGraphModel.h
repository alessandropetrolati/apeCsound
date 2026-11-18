#pragma once
/*
    CsoundGraphModel.h — dal testo di un file Csound a scatole e cavi.

    Alessandro Petrolati — www.apesoft.it · prototipo per la vista "Grafo" di apeCsound.

    Logica pura: solo libreria standard C++17, nessuna dipendenza da JUCE o da Csound.
    Il testo resta l'unica fonte: il modello si ricostruisce ogni volta dal testo.

      una riga con un opcode      -> una scatola (Node)
      una variabile               -> un cavo (Cable) dalla riga che la SCRIVE alle righe che la LEGGONO
      p4, p5...                   -> ingressi dello strumento (i dati della nota)
      variabili globali (ga, gk)  -> "receive" in ingresso, uscita marcata globale
      if / elseif / else / while  -> cornici (Frame), non cavi
      letta prima di essere scritta -> cavo all'indietro (feedback: valore del ciclo precedente)

    Uso:
        csoundGraph::OpcodeTable table;           // facoltativa: nomi e sintassi degli opcode
        table.add ("oscili", "ares oscili xamp, xcps[, ifn, iphs]");
        auto orc = csoundGraph::parse (testoDelCsd, table);
*/
#include <algorithm>
#include <memory>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace csoundGraph
{
//==============================================================================
// Piccoli attrezzi sul testo
inline std::string trim (const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return s.substr (a, b - a);
}
inline bool isIdStart (char c) { return std::isalpha ((unsigned char) c) || c == '_'; }
inline bool isIdChar  (char c) { return std::isalnum ((unsigned char) c) || c == '_'; }
inline bool isIdent (const std::string& s)
{
    if (s.empty() || ! isIdStart (s[0])) return false;
    for (char c : s) if (! isIdChar (c)) return false;
    return true;
}

/** Divide sul separatore solo al livello piu' esterno (fuori da parentesi e stringhe). */
inline std::vector<std::string> splitTop (const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    bool inStr = false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        if (inStr) { cur += c; if (c == '\\' && i + 1 < s.size()) cur += s[++i]; else if (c == '"') inStr = false; continue; }
        if (c == '"') { inStr = true; cur += c; continue; }
        if (c == '(' || c == '[') ++depth;
        if (c == ')' || c == ']') --depth;
        if (c == sep && depth == 0) { out.push_back (trim (cur)); cur.clear(); continue; }
        cur += c;
    }
    if (! trim (cur).empty() || ! out.empty()) out.push_back (trim (cur));
    return out;
}

//==============================================================================
/** Nomi e sintassi degli opcode (in apeCsound: da csoundNewOpcodeList e dall'help). */
struct OpcodeInfo { std::vector<std::string> outs, ins; };

class OpcodeTable
{
public:
    void addName (const std::string& name) { table[name]; }

    /** syntax: riga del manuale, "ares oscili xamp, xcps[, ifn, iphs]". */
    void add (const std::string& name, const std::string& syntax)
    {
        auto& info = table[name];
        if (! info.ins.empty() || ! info.outs.empty()) return;
        std::string line = syntax.substr (0, syntax.find ('\n'));
        size_t pos = std::string::npos;
        for (size_t i = 0; i + name.size() <= line.size(); ++i)
            if (line.compare (i, name.size(), name) == 0
                && (i == 0 || ! isIdChar (line[i - 1]))
                && (i + name.size() == line.size() || ! isIdChar (line[i + name.size()])))
            { pos = i; break; }
        if (pos == std::string::npos) return;
        std::string left = trim (line.substr (0, pos)), right = trim (line.substr (pos + name.size()));
        if (left.find ('=') != std::string::npos) return;
        if (! right.empty() && right[0] == '(')
        {
            auto close = right.find (')');
            right = right.substr (1, close == std::string::npos ? std::string::npos : close - 1);
        }
        auto clean = [] (std::string t)
        {
            std::string r;
            for (char c : t) if (c != '[' && c != ']') r += c;
            std::vector<std::string> v;
            for (auto& p : splitTop (r, ',')) if (! p.empty()) v.push_back (p);
            return v;
        };
        info.outs = clean (left);
        info.ins  = clean (right);
    }

    bool has (const std::string& name) const { return table.count (name) > 0; }
    bool empty() const { return table.empty(); }
    const OpcodeInfo* info (const std::string& name) const
    {
        auto it = table.find (name);
        return it == table.end() ? nullptr : &it->second;
    }
private:
    std::unordered_map<std::string, OpcodeInfo> table;
};

//==============================================================================
// Il modello
struct VarInfo { char rate = '?'; bool global = false; bool pfield = false; };

/** a, k, i, S (stringa), f (spettro); g davanti = globale; pN = dato della nota. */
inline VarInfo classify (const std::string& name)
{
    VarInfo v;
    if (name.size() > 1 && name[0] == 'p' && std::all_of (name.begin() + 1, name.end(), [] (char c) { return std::isdigit ((unsigned char) c); }))
    { v.rate = 'i'; v.pfield = true; return v; }
    size_t k = 0;
    if (name.size() > 1 && name[0] == 'g' && std::string ("akiSfw").find (name[1]) != std::string::npos) { v.global = true; k = 1; }
    if (k < name.size() && std::string ("akiSfw").find (name[k]) != std::string::npos) v.rate = name[k];
    return v;
}

struct Out { std::string var; char rate = '?'; bool global = false; bool isArray = false; bool partial = false; };
struct In  { std::string label, text; std::vector<std::string> vars; bool isConst = false; };

enum class NodeKind { Opcode, Expr, PField, GlobalIn, Undefined, FrameHead, Jump, Label, Macro };

struct Node
{
    int id = 0;
    NodeKind kind = NodeKind::Opcode;
    int line = 0, lineEnd = 0;      // righe del file, da 1
    std::string name;               // opcode, oppure "=", "+=", "if", "while", "p4", "gaBus"...
    std::string text;               // testo da mostrare (espressione, condizione, riga)
    std::vector<Out> outs;
    std::vector<In>  ins;
    int frame = -1;                 // cornice piu' interna, -1 = nessuna
    char rate = '?';                // 'i' = solo alla nascita della nota, 'k', 'a'
    bool functional = false;        // scritto come x = opcode(...)
    bool accumulates = false;       // += oppure scrive un elemento di array
    std::string channel;            // chnget / chnset "Nome"
    bool channelWrite = false;
    std::string eventTarget;        // schedule / event -> strumento
    bool audioOut = false, audioIn = false;
};

struct Cable
{
    int from = 0, fromOut = 0, to = 0, toIn = 0;
    std::string var;
    char rate = '?';
    bool feedback = false;          // letta prima di essere scritta: ciclo precedente
};

struct Frame
{
    int id = 0;
    std::string kind;               // if, elseif, else, while, until
    std::string cond;
    int head = -1;                  // nodo FrameHead (condizione), -1 per else
    int parent = -1;
    int chain = -1;                 // id della cornice "if" a cui appartengono elseif / else
    int lineBegin = 0, lineEnd = 0;
};

struct Graph
{
    enum Kind { Header, Instr, Udo };
    Kind kind = Instr;
    std::vector<std::string> names;         // "1", "Voce"...  (instr 1, 2 -> due nomi)
    std::string udoOutTypes, udoInTypes;
    std::vector<std::string> udoArgs;       // Csound 7: opcode Nome(a:k, b:i) - "a:k", "b:i"
    int lineBegin = 0, lineEnd = 0;
    std::vector<Node>  nodes;
    std::vector<Cable> cables;
    std::vector<Frame> frames;
    std::set<int> pfields;
    std::vector<std::string> unknownOpcodes, undefinedVars, warnings;
    std::string title() const
    {
        if (kind == Header) return "header";
        std::string t = kind == Udo ? "opcode " : "instr ";
        for (size_t i = 0; i < names.size(); ++i) t += (i ? ", " : "") + names[i];
        return t;
    }
};

/** Collegamento fra blocchi: bus globale, canale, evento. */
struct Link { int from = -1, to = -1; std::string kind, name; char rate = '?'; };

struct Orchestra
{
    std::map<std::string, std::string> settings;    // sr, ksmps, nchnls, 0dbfs
    std::vector<Graph> blocks;                      // blocks[0] = header
    std::vector<Link> links;
    std::map<std::string, int> scoreNotes;          // strumento -> righe "i" nello score
    /** Una riga dello score: "i" (nota) o "f" (tabella), con i p-field gia'
        risolti per "." (come la riga prima) e "+" (p2 = inizio + durata della
        nota precedente). line = riga del file. */
    struct ScoreLine { char kind = 'i'; std::string instr; std::vector<std::string> p; int line = 0; };
    std::vector<ScoreLine> scoreLines;
    std::vector<std::string> hostChannels;          // canali letti e mai scritti nel file: parametri dell'host
    std::vector<std::string> warnings;
    int codeLines = 0, unparsed = 0;
};

//==============================================================================
namespace detail
{
struct LLine { int first = 0, last = 0; std::string text; };

/** Toglie i commenti, unisce le righe con "\" finale e le stringhe {{ }} su piu' righe.
    Csound 7: anche le righe con parentesi aperte ("x = (\n 1 +\n 2\n)") e
    quelle che finiscono con un operatore ("if a ||\n b then"). */
inline std::vector<LLine> logicalLines (const std::string& src, int firstLine)
{
    std::vector<LLine> out;
    LLine cur; cur.first = firstLine;
    int line = firstLine;
    int depth = 0, joined = 0;            // parentesi aperte nella riga logica; righe gia' unite
    enum { Normal, Str, Brace, Block } st = Normal;
    auto emit = [&] { cur.last = line; out.push_back (cur); cur = LLine(); cur.first = line + 1; depth = 0; joined = 0; };
    auto endsWithOperator = [] (const std::string& t)
    {
        if (t.empty()) return false;
        const char c = t.back();
        if (t.size() >= 2 && (t.compare (t.size() - 2, 2, "||") == 0 || t.compare (t.size() - 2, 2, "&&") == 0)) return true;
        return c == '+' || c == '*' || c == '/' || c == ',' || c == '?' || c == '(' || c == '[';
    };
    for (size_t i = 0; i < src.size(); ++i)
    {
        const char c = src[i], n = i + 1 < src.size() ? src[i + 1] : 0;
        if (c == '\r') continue;
        if (c == '\n')
        {
            if (st == Str) st = Normal;
            if (st == Brace || st == Block) { cur.text += ' '; ++line; continue; }
            std::string t = cur.text;
            while (! t.empty() && (t.back() == ' ' || t.back() == '\t')) t.pop_back();
            if (! t.empty() && t.back() == '\\') { t.pop_back(); cur.text = t + ' '; ++line; continue; }
            // parentesi ancora aperte o operatore in fondo: la riga continua sotto
            // (al massimo 60 righe: una parentesi dimenticata non si mangia il file)
            if ((depth > 0 || endsWithOperator (t)) && joined < 60) { cur.text = t + ' '; ++line; ++joined; continue; }
            emit(); ++line; continue;
        }
        switch (st)
        {
            case Str:   cur.text += c; if (c == '\\' && n && n != '\n') { cur.text += n; ++i; } else if (c == '"') st = Normal; break;
            case Brace: if (c == '}' && n == '}') { cur.text += '"'; ++i; st = Normal; } else cur.text += (c == '"' ? '\'' : c); break;
            case Block: if (c == '*' && n == '/') { ++i; st = Normal; } break;
            case Normal:
                if (c == '(' || c == '[') ++depth;
                if ((c == ')' || c == ']') && depth > 0) --depth;
                if (c == '"') { st = Str; cur.text += c; }
                else if (c == '{' && n == '{') { st = Brace; cur.text += '"'; ++i; }
                else if (c == '/' && n == '*') { st = Block; ++i; }
                else if (c == ';' || (c == '/' && n == '/'))
                {
                    while (i + 1 < src.size() && src[i + 1] != '\n') ++i;
                }
                else cur.text += (c == '\t' ? ' ' : c);
                break;
        }
    }
    if (! trim (cur.text).empty()) emit();
    return out;
}

inline std::string firstWord (const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && (isIdChar (s[i]) || s[i] == '#')) ++i;
    return s.substr (0, i);
}
inline std::string lastWord (const std::string& s)
{
    size_t b = s.size();
    while (b > 0 && isIdChar (s[b - 1])) --b;
    return s.substr (b);
}

inline bool isConstName (const std::string& id)
{
    static const std::set<std::string> c { "sr", "kr", "ksmps", "nchnls", "nchnls_i", "A4", "then", "ithen", "kthen", "do", "true", "false", "this" };
    return c.count (id) > 0;
}

/** Variabili lette in un'espressione (gli identificatori seguiti da "(" sono funzioni). */
inline void exprVars (const std::string& e, std::vector<std::string>& vars, std::vector<std::string>* funcs = nullptr)
{
    for (size_t i = 0; i < e.size();)
    {
        const char c = e[i];
        if (c == '"') { ++i; while (i < e.size() && e[i] != '"') { if (e[i] == '\\') ++i; ++i; } ++i; continue; }
        if (c == '$') { ++i; while (i < e.size() && isIdChar (e[i])) ++i; if (i < e.size() && e[i] == '.') ++i; continue; }
        if (std::isdigit ((unsigned char) c) || (c == '.' && i + 1 < e.size() && std::isdigit ((unsigned char) e[i + 1])))
        {
            ++i;
            while (i < e.size() && (isIdChar (e[i]) || e[i] == '.'
                   || ((e[i] == '+' || e[i] == '-') && (e[i - 1] == 'e' || e[i - 1] == 'E') && std::isdigit ((unsigned char) e[i - 2])))) ++i;
            continue;
        }
        if (isIdStart (c))
        {
            size_t j = i;
            while (j < e.size() && isIdChar (e[j])) ++j;
            std::string id = e.substr (i, j - i);
            size_t k = j;
            if (k + 1 < e.size() && e[k] == ':' && isIdStart (e[k + 1]))        // opcode:k(...)
            {
                size_t t = k + 1;
                while (t < e.size() && (isIdChar (e[t]) || e[t] == '[' || e[t] == ']')) ++t;
                if (t < e.size() && e[t] == '(') { j = t; k = t; }
            }
            while (k < e.size() && e[k] == ' ') ++k;
            const bool call = k < e.size() && e[k] == '(';
            const bool afterColon = i > 0 && e[i - 1] == ':' && ! (i > 1 && e[i - 2] == ' ');
            const bool member = i > 0 && e[i - 1] == '.' && (i == 1 || isIdChar (e[i - 2]) || e[i - 2] == ']' || e[i - 2] == ' ');   // var0[i].real: "real" e' un campo
            if (call) { if (funcs) funcs->push_back (id); }
            else if (member) {}
            else if (! isConstName (id) && ! (afterColon && id.size() <= 2)
                     && std::find (vars.begin(), vars.end(), id) == vars.end())
                vars.push_back (id);
            i = j; continue;
        }
        ++i;
    }
}

/** Posizione di un operatore di assegnazione al livello esterno; len = 1 ("=") o 2 ("+="). */
inline size_t findAssign (const std::string& s, int& len)
{
    int depth = 0; bool inStr = false;
    for (size_t i = 0; i < s.size(); ++i)
    {
        const char c = s[i];
        if (inStr) { if (c == '\\') ++i; else if (c == '"') inStr = false; continue; }
        if (c == '"') { inStr = true; continue; }
        if (c == '(' || c == '[') ++depth;
        if (c == ')' || c == ']') --depth;
        if (c == '?' ) return std::string::npos;       // un ternario prima di "=": non e' un'assegnazione
        if (depth != 0 || c != '=') continue;
        const char p = i ? s[i - 1] : 0, n = i + 1 < s.size() ? s[i + 1] : 0;
        if (n == '=' ) { ++i; continue; }
        if (p == '=' || p == '!' || p == '<' || p == '>') continue;
        if (p == '+' || p == '-' || p == '*' || p == '/' || p == '%' || p == '|' || p == '&') { len = 2; return i - 1; }
        len = 1; return i;
    }
    return std::string::npos;
}

/** "kArr[kI]", "aSig", "kArr[]", "x:k" -> nome, indici, array? */
/** Una variabile scritta: nome, indici ("kArr[i]"), membro di struct
    ("var0[i].real", "bus.signal"), tipo Csound 7 ("x:i", "arr:S[]",
    "bus@global:Bus"). type = il tipo dichiarato (senza "[]"), global = @global. */
inline bool parseLvalue (const std::string& t, std::string& name, std::string& index, bool& isArray,
                         std::string* type = nullptr, bool* annotatedGlobal = nullptr)
{
    size_t i = 0;
    while (i < t.size() && isIdChar (t[i])) ++i;
    name = t.substr (0, i);
    index.clear(); isArray = false;
    if (type) type->clear();
    if (annotatedGlobal) *annotatedGlobal = false;
    if (name.empty() || ! (isIdStart (name[0]) || std::isdigit ((unsigned char) name[0]))) return false;
    std::string rest = trim (t.substr (i));
    for (;;)
    {
        if (! rest.empty() && rest[0] == '[')
        {
            int d = 0; size_t j = 0;
            for (; j < rest.size(); ++j) { if (rest[j] == '[') ++d; if (rest[j] == ']' && --d == 0) break; }
            if (j >= rest.size()) return false;
            index += rest.substr (1, j - 1) + " ";
            isArray = true;
            rest = trim (rest.substr (j + 1));
            continue;
        }
        if (rest.size() > 1 && rest[0] == '.' && isIdStart (rest[1]))      // membro di una struct: scrittura parziale
        {
            size_t j = 1;
            while (j < rest.size() && isIdChar (rest[j])) ++j;
            index += "." + rest.substr (1, j - 1) + " ";
            isArray = true;
            rest = trim (rest.substr (j));
            continue;
        }
        break;
    }
    if (rest.rfind ("@global", 0) == 0) { rest = trim (rest.substr (7)); if (annotatedGlobal) *annotatedGlobal = true; }
    if (! rest.empty() && rest[0] == ':')
    {
        rest = trim (rest.substr (1));
        std::string ty;
        while (! rest.empty() && (isIdChar (rest[0]) || rest[0] == '[' || rest[0] == ']')) { if (isIdChar (rest[0])) ty += rest[0]; rest.erase (0, 1); }
        if (type) *type = ty;
    }
    return trim (rest).empty();
}

/** Il tempo di un tipo Csound 7 dichiarato ("i", "k", "a", "S", "f", "InstrDef", "Bus"...). */
inline char rateOfType (const std::string& ty)
{
    if (ty.size() == 1 && std::string ("aikSfwb").find (ty[0]) != std::string::npos) return ty[0] == 'b' ? 'k' : ty[0];
    return ty.empty() ? '?' : 'i';      // struct e tipi di sistema: valori come i-time
}

inline std::string unquote (std::string s)
{
    s = trim (s);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr (1, s.size() - 2);
    return s;
}

using Writers = std::map<std::string, std::vector<std::pair<int, int>>>;

inline void mergeWriters (Writers& into, const Writers& other)
{
    for (auto& kv : other)
        for (auto& w : kv.second)
            if (std::find (into[kv.first].begin(), into[kv.first].end(), w) == into[kv.first].end())
                into[kv.first].push_back (w);
}

struct Builder
{
    Graph& g;
    const OpcodeTable& table;
    const std::set<std::string>& udos;
    Writers cur;
    struct Ctx { Writers before, acc; bool hasElse = false; bool loop = false; int frame = -1; int chain = -1; bool sw = false; std::string swExpr; };
    std::vector<Ctx> stack;
    int frame = -1;
    struct Pending { int node, in; std::string var; };
    std::vector<Pending> pending;
    std::map<std::string, int> special;      // p4, gaBus, non definita -> nodo
    std::map<std::string, std::pair<char, bool>> declared;   // Csound 7: "x:i" -> ('i', globale?)

    /** Un'uscita da un lvalue, con il tipo dichiarato (anche in una riga precedente). */
    Out makeOut (const std::string& lvalue, std::string& name, std::string& index, bool& arr)
    {
        std::string ty; bool glob = false;
        parseLvalue (lvalue, name, index, arr, &ty, &glob);
        Out o; o.var = name;
        auto vi = classify (name);
        o.rate = vi.rate; o.global = vi.global;
        if (! ty.empty() || glob)
        {
            const char r = ty.empty() ? (vi.rate == '?' ? 'i' : vi.rate) : rateOfType (ty);
            declared[name] = { r, glob || vi.global };
        }
        if (auto it = declared.find (name); it != declared.end() && o.rate == '?') { o.rate = it->second.first; o.global = o.global || it->second.second; }
        else if (auto it2 = declared.find (name); it2 != declared.end()) o.global = o.global || it2->second.second;
        o.isArray = arr;
        return o;
    }

    Builder (Graph& gg, const OpcodeTable& t, const std::set<std::string>& u) : g (gg), table (t), udos (u) {}

    bool known (const std::string& n) const { return table.has (n) || udos.count (n) > 0; }

    Node& add (NodeKind k, const LLine& l, const std::string& name)
    {
        Node n; n.id = (int) g.nodes.size(); n.kind = k; n.line = l.first; n.lineEnd = l.last; n.name = name; n.frame = frame;
        g.nodes.push_back (n);
        return g.nodes.back();
    }

    int specialNode (NodeKind k, const std::string& var, int line)
    {
        auto it = special.find (var);
        if (it != special.end()) return it->second;
        LLine l; l.first = l.last = line;
        const int saved = frame; frame = -1;
        Node& n = add (k, l, var);
        frame = saved;
        Out o; o.var = var; auto vi = classify (var); o.rate = vi.rate; o.global = vi.global;
        n.outs.push_back (o); n.rate = vi.rate;
        special[var] = n.id;
        return n.id;
    }

    void connect (int node)
    {
        for (int i = 0; i < (int) g.nodes[node].ins.size(); ++i)
        {
            const auto vars = g.nodes[node].ins[i].vars;
            for (auto& v : vars)
            {
                const auto vi = classify (v);
                auto it = cur.find (v);
                if (it != cur.end() && ! it->second.empty())
                {
                    for (auto& w : it->second)
                        if (w.first != node)
                            g.cables.push_back ({ w.first, w.second, node, i, v, vi.rate, false });
                }
                else if (vi.pfield)
                {
                    g.pfields.insert (std::stoi (v.substr (1)));
                    g.cables.push_back ({ specialNode (NodeKind::PField, v, g.lineBegin), 0, node, i, v, 'i', false });
                }
                else if (vi.global)
                    g.cables.push_back ({ specialNode (NodeKind::GlobalIn, v, g.lineBegin), 0, node, i, v, vi.rate, false });
                else
                    pending.push_back ({ node, i, v });
            }
        }
    }

    void write (int node, bool keepOld)
    {
        for (int o = 0; o < (int) g.nodes[node].outs.size(); ++o)
        {
            auto& w = cur[g.nodes[node].outs[o].var];
            if (! keepOld) w.clear();
            w.push_back ({ node, o });
        }
    }

    void finishNode (int id)
    {
        Node& n = g.nodes[id];
        auto rank = [] (char r) { return r == 'a' ? 3 : r == 'f' ? 3 : r == 'k' ? 2 : r == 'S' ? 1 : r == 'i' ? 1 : 0; };
        char best = '?';
        if (! n.outs.empty()) { for (auto& o : n.outs) if (rank (o.rate) > rank (best)) best = o.rate; }
        else for (auto& in : n.ins) for (auto& v : in.vars) { char r = classify (v).rate; if (rank (r) > rank (best)) best = r; }
        if (best == 'S' || (best == '?' && n.outs.empty())) best = n.outs.empty() ? 'k' : 'i';
        if (best == 'f') best = 'k';
        n.rate = best;
    }

    void makeIns (Node& n, const std::vector<std::string>& args)
    {
        const OpcodeInfo* info = table.info (n.name);
        for (size_t a = 0; a < args.size(); ++a)
        {
            In in; in.text = args[a];
            if (info && a < info->ins.size()) in.label = info->ins[a];
            exprVars (args[a], in.vars);
            in.isConst = in.vars.empty();
            n.ins.push_back (in);
        }
    }

    void annotate (Node& n, const std::vector<std::string>& args)
    {
        static const std::set<std::string> chR { "chnget", "chngetk", "chngeta", "chngeti", "chngets", "invalue", "chani" };
        static const std::set<std::string> chW { "chnset", "chnsetk", "chnseta", "chnseti", "chnsets", "outvalue", "chano", "chnmix" };
        static const std::set<std::string> aOut { "out", "outs", "outc", "outch", "outq", "outh", "outo", "outx", "out32", "outall", "outs1", "outs2", "outrg", "fout" };
        static const std::set<std::string> aIn  { "in", "ins", "inch", "inq", "inh", "ino", "inx", "in32", "inrg", "diskin", "diskin2", "soundin", "mp3in" };
        const auto& o = n.name;
        if (chR.count (o) || chW.count (o))
        {
            for (auto& a : args) if (! a.empty() && a[0] == '"') { n.channel = unquote (a); break; }
            n.channelWrite = chW.count (o) > 0;
        }
        n.audioOut = aOut.count (o) > 0;
        n.audioIn  = aIn.count (o) > 0;
        int idx = -1;
        if (o == "schedule" || o == "schedulek" || o == "alwayson" || o == "turnoff2" || o == "turnoff2_i" || o == "turnoff3" || o == "subinstr" || o == "nstance") idx = 0;
        else if (o == "event" || o == "event_i" || o == "massign" || o == "pgmassign") idx = 1;
        // schedkwhen[named] ktrigger, kmintim, kmaxnum, kinsnum|Sname, ...; schedwhen ktrigger, kinsnum, ...
        else if (o == "schedkwhen" || o == "schedkwhennamed" || o == "schedwhen") idx = (o == "schedwhen" ? 1 : 3);
        if (idx >= 0 && idx < (int) args.size())
        {
            std::string t = args[idx];
            auto q = t.find ('"');
            if (q != std::string::npos) { auto q2 = t.find ('"', q + 1); t = t.substr (q + 1, q2 == std::string::npos ? std::string::npos : q2 - q - 1); }
            n.eventTarget = trim (t);
        }
        if (o == "scoreline" || o == "scoreline_i")
            for (auto& a : args)
            {
                auto p = a.find ("i ");
                if (p == std::string::npos) p = a.find ("i\\\"");
                if (p != std::string::npos)
                {
                    size_t j = p + 1;
                    while (j < a.size() && ! isIdChar (a[j])) ++j;
                    size_t k = j;
                    while (k < a.size() && (isIdChar (a[k]))) ++k;
                    if (k > j) { n.eventTarget = a.substr (j, k - j); break; }
                }
            }
    }

    int addOpcode (const LLine& l, const std::string& name, const std::vector<std::string>& outs, const std::vector<std::string>& args, bool functional)
    {
        Node& n = add (NodeKind::Opcode, l, name);
        n.functional = functional;
        bool partial = false;
        for (auto& ot : outs)
        {
            std::string nm, index; bool arr = false;
            Out o = makeOut (ot, nm, index, arr);
            o.partial = arr && ! trim (index).empty();
            if (o.partial) { partial = true; In in; in.text = "[" + trim (index) + "]"; exprVars (index, in.vars); in.isConst = in.vars.empty(); n.ins.push_back (in); }
            n.outs.push_back (o);
        }
        makeIns (n, args);
        annotate (n, args);
        n.accumulates = partial;
        if (! table.empty() && ! known (name) && std::find (g.unknownOpcodes.begin(), g.unknownOpcodes.end(), name) == g.unknownOpcodes.end())
            g.unknownOpcodes.push_back (name);
        const int id = n.id;
        connect (id); write (id, partial); finishNode (id);
        bool inLoop = false;
        for (auto& c : stack) inLoop = inLoop || c.loop;
        // opcode senza stato (letture/scritture di tabelle e array, canali, conversioni):
        // in un ciclo vanno bene, non serve l'avviso
        static const std::set<std::string> stateless { "init", "vaget", "vaset", "tab", "tabw", "tab_i", "tabw_i", "table", "tablei", "table3", "tablew",
            "tableiw", "tablekt", "tableikt", "chnget", "chnset", "chngetk", "chnsetk", "chngeti", "chnseti", "chngeta", "chnseta", "chngets", "chnsets",
            "zkr", "zkw", "zar", "zaw", "zir", "ziw", "cpsmidinn", "ampdb", "dbamp", "mtof", "ftom", "cpspch", "pchmidi", "octcps", "cpsoct", "ftlen",
            "lenarray", "fillarray", "strcpy", "strcpyk", "strcat", "strcatk", "sprintf", "sprintfk", "strlen", "strlenk", "strsub", "strsubk", "strindex",
            "strindexk", "strtod", "strtodk", "strtol", "strtolk", "event", "schedulek", "schedkwhen", "turnoff2", "ftgenonce", "maxarray", "minarray",
            "sumarray", "scalearray", "copyf2array", "copya2ftab", "getrow", "setrow", "getcol", "setcol", "slicearray", "tab2array", "trim", "printf",
            "printks", "println" };
        if (inLoop && ! functional && ! g.nodes[id].outs.empty() && (g.nodes[id].rate == 'k' || g.nodes[id].rate == 'a') && ! stateless.count (name))
            g.warnings.push_back ("riga " + std::to_string (l.first) + ": opcode \"" + name + "\" dentro un ciclo (lo stato e' condiviso fra le iterazioni)");
        return id;
    }

    int addHead (const LLine& l, const std::string& kind, const std::string& cond)
    {
        Node& n = add (NodeKind::FrameHead, l, kind);
        n.text = cond;
        In in; in.text = cond; exprVars (cond, in.vars); in.isConst = in.vars.empty();
        n.ins.push_back (in);
        const int id = n.id;
        connect (id); finishNode (id);
        return id;
    }

    int openFrame (const LLine& l, const std::string& kind, const std::string& cond, int head, int parent, int chain)
    {
        Frame f; f.id = (int) g.frames.size(); f.kind = kind; f.cond = cond; f.head = head; f.parent = parent;
        f.chain = chain < 0 ? f.id : chain; f.lineBegin = l.first; f.lineEnd = l.last;
        g.frames.push_back (f);
        return f.id;
    }

    /** Una riga logica dentro un blocco. Torna false se non riconosciuta. */
    bool statement (LLine l)
    {
        std::string s = trim (l.text);
        if (s.empty()) return true;

        // etichetta "nome:" (da sola o davanti a un'istruzione)
        {
            size_t i = 0;
            while (i < s.size() && isIdChar (s[i])) ++i;
            if (i > 0 && i < s.size() && s[i] == ':' && (i + 1 == s.size() || std::isspace ((unsigned char) s[i + 1])) && isIdStart (s[0]))
            {
                add (NodeKind::Label, l, s.substr (0, i));
                s = trim (s.substr (i + 1));
                if (s.empty()) return true;
            }
        }

        const std::string w = firstWord (s);
        if (w == "struct") return true;     // Csound 7: struct Punto x:i, y:i (una dichiarazione di tipo, nessun calcolo)
        if (w == "#define" || w == "#include" || w == "#ifdef" || w == "#ifndef" || w == "#end" || w == "#endif" || w == "#else" || w == "#undef" || w == "#includestr")
            return true;

        if (s[0] == '$')
        {
            Node& n = add (NodeKind::Macro, l, s.substr (0, s.find_first_of (" (.")));
            n.text = s;
            In in; in.text = s; exprVars (s, in.vars); n.ins.push_back (in);
            const int id = n.id; connect (id); finishNode (id);
            return true;
        }

        if (w == "if" || w == "elseif")
        {
            std::string rest = trim (s.substr (w.size()));
            const std::string lw = lastWord (rest);
            if (lw == "then" || lw == "ithen" || lw == "kthen")
            {
                const std::string cond = trim (rest.substr (0, rest.size() - lw.size()));
                if (w == "if")
                {
                    const int head = addHead (l, "if", cond);
                    Ctx c; c.before = cur; c.frame = frame;
                    const int f = openFrame (l, "if", cond, head, frame, -1);
                    c.chain = f; stack.push_back (c); frame = f;
                }
                else
                {
                    if (stack.empty() || stack.back().loop || stack.back().sw) return false;
                    auto& c = stack.back();
                    g.frames[frame].lineEnd = l.first - 1;
                    mergeWriters (c.acc, cur); cur = c.before; frame = c.frame;
                    const int head = addHead (l, "elseif", cond);
                    frame = openFrame (l, "elseif", cond, head, c.frame, c.chain);
                }
                return true;
            }
            // if COND goto etichetta
            auto words = splitTop (rest, ' ');
            std::vector<std::string> ws;
            for (auto& x : words) if (! x.empty()) ws.push_back (x);
            if (ws.size() >= 2)
            {
                const std::string& jmp = ws[ws.size() - 2];
                if (jmp == "goto" || jmp == "kgoto" || jmp == "igoto")
                {
                    auto p = rest.rfind (jmp);
                    Node& n = add (NodeKind::Jump, l, "if " + jmp);
                    n.text = trim (rest.substr (0, p)) + " -> " + ws.back();
                    In in; in.text = trim (rest.substr (0, p)); exprVars (in.text, in.vars); n.ins.push_back (in);
                    const int id = n.id; connect (id); finishNode (id);
                    return true;
                }
            }
            return false;
        }
        if (w == "else" && trim (s.substr (4)).empty())
        {
            if (stack.empty() || stack.back().loop || stack.back().sw) return false;
            auto& c = stack.back();
            g.frames[frame].lineEnd = l.first - 1;
            mergeWriters (c.acc, cur); cur = c.before; c.hasElse = true; frame = c.frame;
            frame = openFrame (l, "else", "", -1, c.frame, c.chain);
            return true;
        }
        if (w == "endif" || w == "fi")
        {
            if (stack.empty() || stack.back().loop || stack.back().sw) return false;
            auto c = stack.back(); stack.pop_back();
            g.frames[frame].lineEnd = l.last;
            mergeWriters (c.acc, cur);
            if (! c.hasElse) mergeWriters (c.acc, c.before);
            cur = c.acc; frame = c.frame;
            return true;
        }
        if ((w == "while" || w == "until") && lastWord (s) == "do")
        {
            const std::string cond = trim (s.substr (w.size(), s.size() - w.size() - 2));
            Ctx c; c.before = cur; c.loop = true; c.frame = frame;
            const int f = openFrame (l, w, cond, -1, frame, -1);
            stack.push_back (c); frame = f;
            g.frames[f].head = addHead (l, w, cond);
            return true;
        }
        // Csound 7:  for x in arr do / for x, i in arr do / for i in [0 ... 9] do
        if (w == "for" && lastWord (s) == "do")
        {
            const std::string body = trim (s.substr (3, s.size() - 3 - 2));
            const auto inPos = body.find (" in ");
            std::string vars = inPos == std::string::npos ? std::string() : trim (body.substr (0, inPos));
            const std::string over = inPos == std::string::npos ? body : trim (body.substr (inPos + 4));
            Ctx c; c.before = cur; c.loop = true; c.frame = frame;
            const int f = openFrame (l, "for", body, -1, frame, -1);
            stack.push_back (c); frame = f;
            const int head = addHead (l, "for", over);
            g.frames[f].head = head;
            for (auto& v : splitTop (vars, ','))
            {
                std::string nm, ix; bool arr;
                Out o = makeOut (trim (v), nm, ix, arr);
                if (o.rate == '?') o.rate = 'i';
                if (! nm.empty()) g.nodes[head].outs.push_back (o);
            }
            write (head, false);
            return true;
        }
        // Csound 7:  switch x / case 1, 2 / default / endsw  (come una catena if / elseif / else)
        if (w == "switch")
        {
            Ctx c; c.before = cur; c.frame = frame; c.sw = true; c.swExpr = trim (s.substr (6));
            stack.push_back (c);
            addHead (l, "switch", c.swExpr);
            return true;
        }
        if (w == "case" || w == "default")
        {
            if (stack.empty() || ! stack.back().sw) return false;
            auto& c = stack.back();
            const std::string cond = w == "default" ? std::string() : c.swExpr + " == " + trim (s.substr (4));
            if (c.chain >= 0)
            {
                g.frames[frame].lineEnd = l.first - 1;
                mergeWriters (c.acc, cur); cur = c.before;
            }
            if (w == "default") c.hasElse = true;
            const int head = w == "default" ? -1 : addHead (l, "case", cond);
            frame = openFrame (l, w == "default" ? "else" : (c.chain < 0 ? "if" : "elseif"), cond, head, c.frame, c.chain);
            if (c.chain < 0) { c.chain = frame; g.frames[frame].chain = frame; }
            return true;
        }
        if (w == "endsw")
        {
            if (stack.empty() || ! stack.back().sw) return false;
            auto c = stack.back(); stack.pop_back();
            if (c.chain >= 0)
            {
                g.frames[frame].lineEnd = l.last;
                mergeWriters (c.acc, cur);
                if (! c.hasElse) mergeWriters (c.acc, c.before);
                cur = c.acc;
            }
            frame = c.frame;
            return true;
        }
        if (w == "od" || w == "enduntil")
        {
            if (stack.empty() || ! stack.back().loop) return false;
            auto c = stack.back(); stack.pop_back();
            g.frames[frame].lineEnd = l.last;
            mergeWriters (cur, c.before); frame = c.frame;
            return true;
        }
        if (w == "goto" || w == "kgoto" || w == "igoto" || w == "rigoto" || w == "tigoto" || w == "reinit"
            || w == "cggoto" || w == "cigoto" || w == "ckgoto" || w == "cngoto" || w == "timout"
            || w == "loop_lt" || w == "loop_le" || w == "loop_gt" || w == "loop_ge"
            || w == "looplt" || w == "loople" || w == "loopgt" || w == "loopge")
        {
            Node& n = add (NodeKind::Jump, l, w);
            n.text = trim (s.substr (w.size()));
            auto args = splitTop (n.text, ',');
            const bool loopOp = w.rfind ("loop", 0) == 0;
            for (size_t a = 0; a + 1 < args.size() || (a < args.size() && ! (w == "goto" || w == "kgoto" || w == "igoto" || w == "reinit" || w == "rigoto" || w == "tigoto")); ++a)
            {
                if (a + 1 == args.size()) break;      // l'ultimo argomento e' l'etichetta
                In in; in.text = args[a]; exprVars (args[a], in.vars); in.isConst = in.vars.empty(); n.ins.push_back (in);
            }
            const int id = n.id;
            if (loopOp && ! args.empty() && isIdent (args[0]))
            {
                Out o; o.var = args[0]; o.rate = classify (args[0]).rate; g.nodes[id].outs.push_back (o); g.nodes[id].accumulates = true;
            }
            connect (id); if (loopOp) write (id, true); finishNode (id);
            return true;
        }

        // assegnazione:  a = espr   a += espr   kArr[i] = espr   a1, a2 = f(...)
        int alen = 0;
        const size_t ap = findAssign (s, alen);
        if (ap != std::string::npos)
        {
            const auto lhs = splitTop (s.substr (0, ap), ',');
            const std::string rhs = trim (s.substr (ap + alen));
            bool ok = ! lhs.empty();
            for (auto& t : lhs) { std::string nm, ix; bool arr; ok = ok && parseLvalue (t, nm, ix, arr) && (isIdent (nm) || nm == "0dbfs"); }
            if (ok)
            {
                // x = opcode(args): una scatola con il nome dell'opcode
                size_t i = 0, nameEnd = 0;
                while (i < rhs.size() && isIdChar (rhs[i])) ++i;
                nameEnd = i;
                if (i + 1 < rhs.size() && rhs[i] == ':' && isIdStart (rhs[i + 1]))
                {
                    size_t t = i + 1;
                    while (t < rhs.size() && (isIdChar (rhs[t]) || rhs[t] == '[' || rhs[t] == ']')) ++t;
                    if (t < rhs.size() && rhs[t] == '(') i = t;
                }
                if (alen == 1 && i > 0 && i < rhs.size() && rhs[i] == '(' && rhs.back() == ')' && isIdStart (rhs[0]))
                {
                    int d = 0; size_t j = i;
                    for (; j < rhs.size(); ++j) { if (rhs[j] == '(') ++d; if (rhs[j] == ')' && --d == 0) break; }
                    if (j == rhs.size() - 1)
                    {
                        addOpcode (l, rhs.substr (0, nameEnd), lhs, splitTop (rhs.substr (i + 1, j - i - 1), ','), true);
                        return true;
                    }
                }
                Node& n = add (NodeKind::Expr, l, s.substr (ap, alen));
                n.text = rhs;
                bool partial = false;
                std::vector<std::string> reads;
                for (auto& t : lhs)
                {
                    std::string nm, ix; bool arr;
                    Out o = makeOut (t, nm, ix, arr);
                    o.partial = arr && ! trim (ix).empty();
                    n.outs.push_back (o);
                    if (o.partial) { partial = true; exprVars (ix, reads); }
                    if (alen == 2 && std::find (reads.begin(), reads.end(), nm) == reads.end()) reads.push_back (nm);
                }
                exprVars (rhs, reads);
                for (auto& v : reads) { In in; in.label = v; in.text = v; in.vars = { v }; n.ins.push_back (in); }
                n.accumulates = partial || alen == 2;
                const int id = n.id;
                connect (id); write (id, partial); finishNode (id);
                return true;
            }
        }

        // forma classica:  uscite opcode argomenti
        std::vector<std::string> L;
        size_t pos = 0;
        auto skipWs = [&] { while (pos < s.size() && std::isspace ((unsigned char) s[pos])) ++pos; };
        auto readLv = [&] () -> std::string
        {
            const size_t a = pos;
            while (pos < s.size() && isIdChar (s[pos])) ++pos;
            if (pos == a) return {};
            while (pos < s.size() && s[pos] == '[')
            {
                int d = 0;
                for (; pos < s.size(); ++pos) { if (s[pos] == '[') ++d; if (s[pos] == ']' && --d == 0) { ++pos; break; } }
            }
            if (s.compare (pos, 7, "@global") == 0) pos += 7;          // Csound 7: nome@global:i
            if (pos + 1 < s.size() && s[pos] == ':' && isIdStart (s[pos + 1])) { ++pos; while (pos < s.size() && (isIdChar (s[pos]) || s[pos] == '[' || s[pos] == ']')) ++pos; }
            return s.substr (a, pos - a);
        };
        size_t afterList = 0;
        for (;;)
        {
            skipWs();
            const std::string lv = readLv();
            if (lv.empty()) break;
            L.push_back (lv);
            afterList = pos;
            skipWs();
            if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
            break;
        }
        if (L.empty()) return false;
        pos = afterList; skipWs();
        const size_t nStart = pos;
        while (pos < s.size() && isIdChar (s[pos])) ++pos;
        const std::string N = s.substr (nStart, pos - nStart);
        const std::string afterN = trim (s.substr (pos));
        const std::string T1 = L[0];
        bool nIsOpcode;
        if (L.size() > 1) nIsOpcode = isIdent (N);
        else if (! isIdent (N)) nIsOpcode = false;
        // "chnset limit(k, 0, 1), \"X\"": il primo e' l'opcode, il secondo una funzione nei suoi argomenti
        else if (known (N) && known (T1) && ! afterN.empty() && afterN[0] == '(') nIsOpcode = false;
        else if (known (N) && known (T1)) nIsOpcode = ! (N == "a" || N == "k" || N == "i" || N == "p");
        else if (known (N)) nIsOpcode = true;
        else if (known (T1)) nIsOpcode = false;
        else nIsOpcode = ! afterN.empty() && afterN[0] != ',' && std::string ("+-*/%<>=!&|?:)").find (afterN[0]) == std::string::npos;

        if (nIsOpcode)
        {
            if (! afterN.empty() && afterN[0] == '(' && afterN.back() == ')' && L.size() == 1 && ! known (N)) return false;
            addOpcode (l, N, L, splitTop (afterN, ','), false);
            return true;
        }
        if (! isIdent (T1)) return false;
        std::string rest = trim (s.substr (T1.size()));
        if (! rest.empty() && rest[0] == '(' && rest.back() == ')')
        {
            int d = 0; size_t j = 0;
            for (; j < rest.size(); ++j) { if (rest[j] == '(') ++d; if (rest[j] == ')' && --d == 0) break; }
            if (j == rest.size() - 1) rest = rest.substr (1, rest.size() - 2);
        }
        addOpcode (l, T1, {}, splitTop (rest, ','), false);
        return true;
    }

    void finish()
    {
        for (auto& p : pending)
        {
            auto it = cur.find (p.var);
            bool done = false;
            if (it != cur.end())
            {
                for (auto& w : it->second)
                {
                    if (w.first != p.node) g.cables.push_back ({ w.first, w.second, p.node, p.in, p.var, classify (p.var).rate, true });
                    done = true;            // se legge se stessa: accumulo, nessun cavo
                }
            }
            if (! done)
            {
                g.cables.push_back ({ specialNode (NodeKind::Undefined, p.var, g.lineBegin), 0, p.node, p.in, p.var, classify (p.var).rate, false });
                if (std::find (g.undefinedVars.begin(), g.undefinedVars.end(), p.var) == g.undefinedVars.end()) g.undefinedVars.push_back (p.var);
            }
        }
        if (! stack.empty()) g.warnings.push_back ("cornici non chiuse: " + std::to_string (stack.size()));
    }
};
} // namespace detail

//==============================================================================
/** Dal testo (file .csd intero, oppure solo l'orchestra) al modello. */
inline Orchestra parse (const std::string& text, const OpcodeTable& table = OpcodeTable())
{
    using namespace detail;
    Orchestra orc;

    std::string src = text;
    int firstLine = 1;
    const auto a = text.find ("<CsInstruments>");
    if (a != std::string::npos)
    {
        const auto b = text.find ("</CsInstruments>", a);
        const size_t start = a + 15;
        src = text.substr (start, b == std::string::npos ? std::string::npos : b - start);
        firstLine = 1 + (int) std::count (text.begin(), text.begin() + (long) start, '\n');
    }
    const auto lines = logicalLines (src, firstLine);

    // primo passo: i nomi degli opcode definiti dall'utente
    std::set<std::string> udos;
    for (auto& l : lines)
    {
        const std::string s = trim (l.text);
        if (firstWord (s) == "opcode")
        {
            auto parts = splitTop (trim (s.substr (6)), ',');
            if (! parts.empty()) { std::string n = parts[0]; n = n.substr (0, n.find ('(')); udos.insert (trim (n)); }
        }
    }

    orc.blocks.emplace_back();
    orc.blocks[0].kind = Graph::Header;
    orc.blocks[0].lineBegin = firstLine;
    std::vector<std::vector<LLine>> blockLines (1);
    int current = 0;
    // #define NOME(A) # corpo # : il corpo (anche su piu' righe, e anche con il
    // primo # sulla riga dopo) non e' codice da disegnare
    bool inMacro = false, waitingBody = false;
    for (auto& l : lines)
    {
        const std::string s = trim (l.text);
        if (s.empty()) continue;
        const int hashes = (int) std::count (s.begin(), s.end(), '#');
        if (inMacro) { if (hashes % 2 == 1) inMacro = false; continue; }
        if (waitingBody)
        {
            waitingBody = false;
            if (s[0] == '#') { if (hashes == 1) inMacro = true; continue; }
        }
        const std::string w = firstWord (s);
        if (w == "#define")
        {
            const int body = hashes - 1;          // senza il # di #define
            if (body == 0) waitingBody = true;
            else if (body == 1) inMacro = true;
            continue;
        }
        ++orc.codeLines;
        if (w == "instr" && current == 0)
        {
            orc.blocks.emplace_back(); blockLines.emplace_back();
            current = (int) orc.blocks.size() - 1;
            auto& g = orc.blocks[(size_t) current];
            g.kind = Graph::Instr; g.lineBegin = l.first;
            for (auto& n : splitTop (trim (s.substr (5)), ',')) if (! n.empty()) g.names.push_back (n[0] == '+' ? n.substr (1) : n);
            continue;
        }
        if (w == "opcode" && current == 0)
        {
            orc.blocks.emplace_back(); blockLines.emplace_back();
            current = (int) orc.blocks.size() - 1;
            auto& g = orc.blocks[(size_t) current];
            g.kind = Graph::Udo; g.lineBegin = l.first;
            auto parts = splitTop (trim (s.substr (6)), ',');
            const std::string head = trim (s.substr (6));
            if (const auto par = head.find ('('); par != std::string::npos && (parts.empty() || parts[0].find ('(') != std::string::npos))
            {
                // Csound 7:  opcode Nome(a:k, b:i):a  - argomenti con nome e tipo
                g.names.push_back (trim (head.substr (0, par)));
                int d = 0; size_t e = par;
                for (; e < head.size(); ++e) { if (head[e] == '(') ++d; if (head[e] == ')' && --d == 0) break; }
                g.udoArgs = splitTop (head.substr (par + 1, e > par ? e - par - 1 : 0), ',');
                const auto colon = head.find (':', e);
                g.udoInTypes.clear();
                for (auto& a : g.udoArgs) { const auto c = a.find (':'); g.udoInTypes += c == std::string::npos ? std::string ("?") : trim (a.substr (c + 1)); }
                g.udoOutTypes = colon == std::string::npos ? std::string ("0") : trim (head.substr (colon + 1));
                continue;
            }
            if (! parts.empty()) g.names.push_back (parts[0]);
            if (parts.size() > 1) g.udoOutTypes = parts[1];
            if (parts.size() > 2) g.udoInTypes = parts[2];
            continue;
        }
        if ((w == "endin" || w == "endop") && current != 0)
        {
            orc.blocks[(size_t) current].lineEnd = l.last;
            current = 0;
            continue;
        }
        blockLines[(size_t) current].push_back (l);
    }

    for (size_t b = 0; b < orc.blocks.size(); ++b)
    {
        auto& g = orc.blocks[b];
        Builder bld (g, table, udos);
        if (g.kind == Graph::Udo && ! g.udoArgs.empty())
        {
            // gli argomenti con nome: un "xin" implicito sulla riga dell'opcode
            LLine hl; hl.first = hl.last = g.lineBegin;
            bld.addOpcode (hl, "xin", g.udoArgs, {}, false);
        }
        for (auto& l : blockLines[b])
        {
            if (b == 0)
            {
                // sr = 44100, ksmps = 32, nchnls = 2, 0dbfs = 1
                const std::string s = trim (l.text);
                const auto eq = s.find ('=');
                if (eq != std::string::npos)
                {
                    const std::string k = trim (s.substr (0, eq));
                    if (k == "sr" || k == "kr" || k == "ksmps" || k == "nchnls" || k == "nchnls_i" || k == "0dbfs" || k == "A4")
                    { orc.settings[k] = trim (s.substr (eq + 1)); continue; }
                }
            }
            if (! bld.statement (l))
            {
                ++orc.unparsed;
                orc.warnings.push_back ("riga " + std::to_string (l.first) + " non riconosciuta: " + trim (l.text));
            }
        }
        bld.finish();
        if (g.lineEnd == 0 && ! g.nodes.empty()) g.lineEnd = g.nodes.back().lineEnd;
    }

    // collegamenti fra blocchi: bus globali, canali, eventi
    auto blockOf = [&] (const std::string& name) -> int
    {
        for (size_t b = 1; b < orc.blocks.size(); ++b)
            if (orc.blocks[b].kind == Graph::Instr)
                for (auto& n : orc.blocks[b].names) if (n == name) return (int) b;
        return -1;
    };
    std::map<std::string, std::set<int>> gW, gR, cW, cR;
    for (size_t b = 0; b < orc.blocks.size(); ++b)
        for (auto& n : orc.blocks[b].nodes)
        {
            if (n.kind == NodeKind::GlobalIn) gR[n.name].insert ((int) b);
            else for (auto& o : n.outs) if (o.global && ! (b == 0 && (n.name == "init" || n.kind == NodeKind::Expr))) gW[o.var].insert ((int) b);
            if (! n.channel.empty()) (n.channelWrite ? cW : cR)[n.channel].insert ((int) b);
            if (! n.eventTarget.empty())
            {
                const int t = blockOf (n.eventTarget);
                if (t >= 0) orc.links.push_back ({ (int) b, t, "evento", n.name, 'i' });
            }
        }
    for (auto& kv : gR)
        for (int r : kv.second)
            for (int w : gW[kv.first])
                if (w != r) orc.links.push_back ({ w, r, "bus", kv.first, classify (kv.first).rate });
    for (auto& kv : cR)
    {
        if (cW[kv.first].empty()) orc.hostChannels.push_back (kv.first);
        for (int r : kv.second)
            for (int w : cW[kv.first])
                if (w != r) orc.links.push_back ({ w, r, "canale", kv.first, '?' });
    }

    // score: quante note per strumento
    const auto sa = text.find ("<CsScore>");
    if (sa != std::string::npos)
    {
        const auto sb = text.find ("</CsScore>", sa);
        const std::string sco = text.substr (sa + 9, sb == std::string::npos ? std::string::npos : sb - sa - 9);
        const int scoFirst = 1 + (int) std::count (text.begin(), text.begin() + (long) sa, '\n');
        std::vector<std::string> prevI;
        for (auto& l : logicalLines (sco, scoFirst))
        {
            const std::string s = trim (l.text);
            // p-field: parole separate da spazi (stringhe "..." e espressioni [...] intere)
            if (s.size() > 1 && (s[0] == 'i' || s[0] == 'f') && ! isIdChar (s[1]))
            {
                Orchestra::ScoreLine sl;
                sl.kind = s[0];
                sl.line = l.first;
                std::string cur;
                int depth = 0; bool str = false;
                for (size_t i = 1; i <= s.size(); ++i)
                {
                    const char c = i < s.size() ? s[i] : ' ';
                    if (c == '"') str = ! str;
                    if (! str && c == '[') ++depth;
                    if (! str && c == ']') --depth;
                    if (! str && depth <= 0 && (c == ' ' || c == '\t' || c == ','))
                    {
                        if (! cur.empty()) { sl.p.push_back (cur); cur.clear(); }
                        continue;
                    }
                    cur += c;
                }
                if (! sl.p.empty())
                {
                    if (sl.kind == 'i')
                    {
                        for (size_t k = 0; k < sl.p.size(); ++k)
                        {
                            if (sl.p[k] == "." && k < prevI.size()) sl.p[k] = prevI[k];
                            else if (sl.p[k] == "+" && k == 1 && prevI.size() > 2)
                            {
                                char* e1 = nullptr; char* e2 = nullptr;
                                const double st = std::strtod (prevI[1].c_str(), &e1), du = std::strtod (prevI[2].c_str(), &e2);
                                if (e1 != prevI[1].c_str() && e2 != prevI[2].c_str())
                                {
                                    std::ostringstream os; os << (st + std::abs (du)); sl.p[k] = os.str();
                                }
                            }
                        }
                        auto instrOf = [] (std::string nm)
                        {
                            if (nm.size() > 1 && nm.front() == '"') return nm.substr (1, nm.size() - 2);
                            if (! nm.empty() && std::isdigit ((unsigned char) nm[0])) return nm.substr (0, nm.find ('.'));   // 1.2: istanza 2 dello strumento 1
                            return nm;
                        };
                        sl.instr = instrOf (sl.p[0]);
                        for (size_t k = sl.p.size(); k < prevI.size() && k >= 3 && sl.instr == instrOf (prevI[0]); ++k)
                            sl.p.push_back (prevI[k]);      // p-field mancanti: come la riga prima (stesso strumento)
                        prevI = sl.p;
                    }
                    orc.scoreLines.push_back (sl);
                }
            }
            if (s.size() > 1 && s[0] == 'i' && ! isIdChar (s[1]))
            {
                std::string rest = trim (s.substr (1));
                size_t j = 0;
                if (! rest.empty() && rest[0] == '"') { j = rest.find ('"', 1); rest = rest.substr (1, j == std::string::npos ? std::string::npos : j - 1); }
                else { while (j < rest.size() && (std::isdigit ((unsigned char) rest[j]) || isIdChar (rest[j]))) ++j; rest = rest.substr (0, j); }
                if (! rest.empty()) ++orc.scoreNotes[rest];
            }
        }
    }
    return orc;
}
} // namespace csoundGraph
