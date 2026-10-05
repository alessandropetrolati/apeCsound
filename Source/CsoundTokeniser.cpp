#include "CsoundTokeniser.h"

CsoundTokeniser::CsoundTokeniser()
{
    keywords.addArray ({
        "instr", "endin", "opcode", "endop",
        "if", "then", "elseif", "else", "endif", "until", "do", "od", "while",
        "goto", "igoto", "kgoto", "cggoto", "cigoto", "ckgoto",
        "reinit", "rireturn", "return", "timout", "tigoto",
        "setksmps", "xin", "xout"
    });

    // Sottoinsieme molto ampio (ma non esaustivo) degli opcode standard di
    // Csound, raggruppati per categoria per facilitare la manutenzione.
    opcodes.addArray ({
        "ATSadd", "ATSaddnz", "ATSbufread", "ATScross", "ATSinfo", "ATSinterpread", "ATSpartialtap", "ATSread", "ATSreadnz", "ATSsinnoi", "K35_hpf", "K35_lpf", "MixerClear", "MixerGetLevel", "MixerReceive", "MixerSend", "MixerSetLevel", "MixerSetLevel_i", "OSCbundle", "OSCcount", "OSCinit", "OSCinitM", "OSClisten", "OSCraw", "OSCsend", "OSCsend_lo", "S", "a", "abs", "active", "adsr", "adsyn", "adsynt", "adsynt2", "aftouch", "allpole", "alpass", "alwayson", "ampdb", "ampdbfs", "ampmidi", "ampmidicurve", "ampmidid", "apoleparams", "arduinoRead", "arduinoReadF", "arduinoStart", "arduinoStop", "areson", "aresonk", "atone", "atonek", "atonex", "autocorr", "babo", "balance", "balance2", "bamboo", "barmodel", "bbcutm", "bbcuts", "betarand", "bexprnd", "bformdec1", "bformdec2", "bformenc1", "binit", "biquad", "biquada", "birnd", "bob", "bpf", "bpfcos", "bqrez", "butbp", "butbr", "buthp", "butlp", "butterbp", "butterbr", "butterhp", "butterlp", "button", "buzz", "c2r", "cabasa", "cauchy", "cauchyi", "cbrt", "ceil", "cell", "cent", "centroid", "ceps", "cepsinv", "cggoto", "chanctrl", "changed", "changed2", "chani", "chano", "chebyshevpoly", "checkbox", "chn_S", "chn_a", "chn_k", "chnclear", "chnexport", "chnget", "chngeta", "chngeti", "chngetk", "chngetks", "chngets", "chnmix", "chnparams", "chnset", "chnseta", "chnseti", "chnsetk", "chnsetks", "chnsets", "cigoto", "cingoto", "ckgoto", "clear", "clfilt", "clip", "clockoff", "clockon", "cmp", "cmplxprod", "cngoto", "cnkgoto", "cntCreate", "cntCycles", "cntDelete", "cntDelete_i", "cntRead", "cntReset", "cntState", "comb", "combinv", "compilecsd", "compileorc", "compilestr", "compress", "compress2", "connect", "control", "convle", "convolve", "copya2ftab", "copyf2array", "cos", "cosh", "cosinv", "cosseg", "cossegb", "cossegr", "count", "count_i", "cps2pch", "cpsmidi", "cpsmidib", "cpsmidinn", "cpsoct", "cpspch", "cpstmid", "cpstun", "cpstuni", "cpsxpch", "cpuprc", "cross2", "crossfm", "crossfmi", "crossfmpm", "crossfmpmi", "crosspm", "crosspmi", "crunch", "ctlchn", "ctrl14", "ctrl21", "ctrl7", "ctrlinit", "ctrlpreset", "ctrlprint", "ctrlprintpresets", "ctrlsave", "ctrlselect", "cuserrnd", "dam", "date", "dates", "db", "dbamp", "dbfsamp", "dcblock", "dcblock2", "dconv", "dct", "dctinv", "deinterleave", "delay", "delay1", "delayk", "delayr", "delayw", "deltap", "deltap3", "deltapi", "deltapn", "deltapx", "deltapxw", "denorm", "diff", "diode_ladder", "directory", "diskgrain", "diskin", "diskin2", "dispfft", "display", "distort", "distort1", "divz", "doppler", "dot", "downsamp", "dripwater", "dumpk", "dumpk2", "dumpk3", "dumpk4", "duserrnd", "dust", "dust2", "elapsedcycles", "elapsedtime", "endin", "endop", "envlpx", "envlpxr", "ephasor", "eqfil", "evalstr", "event", "event_i", "eventcycles", "eventtime", "exciter", "exitnow", "exp", "expcurve", "expon", "exprand", "exprandi", "expseg", "expsega", "expsegb", "expsegba", "expsegr", "fareylen", "fareyleni", "fft", "fftinv", "ficlose", "filebit", "filelen", "filenchnls", "filepeak", "filescal", "filesr", "filevalid", "fillarray", "filter2", "fin", "fini", "fink", "fiopen", "flanger", "flashtxt", "flooper", "flooper2", "floor", "fmanal", "fmax", "fmb3", "fmbell", "fmin", "fmmetal", "fmod", "fmpercfl", "fmrhode", "fmvoice", "fmwurlie", "fof", "fof2", "fofilter", "fog", "fold", "follow", "follow2", "foscil", "foscili", "fout", "fouti", "foutir", "foutk", "fprintks", "fprints", "frac", "fractalnoise", "framebuffer", "freeverb", "ftaudio", "ftchnls", "ftconv", "ftcps", "ftexists", "ftfree", "ftgen", "ftgenonce", "ftgentmp", "ftlen", "ftload", "ftloadk", "ftlptim", "ftmorf", "ftom", "ftprint", "ftresize", "ftresizei", "ftsamplebank", "ftsave", "ftsavek", "ftset", "ftslice", "ftslicei", "ftsr", "gain", "gainslider", "gauss", "gaussi", "gausstrig", "gbuzz", "genarray", "genarray_i", "gendy", "gendyc", "gendyx", "getcfg", "getcol", "getftargs", "getrow", "getseed", "gogobel", "goto", "grain", "grain2", "grain3", "granule", "gtadsr", "gtf", "guiro", "harmon", "harmon2", "harmon3", "harmon4", "hilbert", "hilbert2", "hrtfearly", "hrtfmove", "hrtfmove2", "hrtfmove", "hrtfstat", "hsboscil", "hvs1", "hvs2", "hvs3", "hypot", "i", "igoto", "ihold", "in", "in32", "inch", "inh", "init", "initc14", "initc21", "initc7", "inleta", "inletf", "inletk", "inletkid", "inletv", "ino", "inq", "inrg", "ins", "insglobal", "insremot", "instr", "int", "integ", "interleave", "interp", "invalue", "inx", "inz", "jitter", "jitter2", "jspline", "k", "kgoto", "lag", "lagud", "lastcycle", "lenarray", "lfo", "lfsr", "limit", "limit1", "lincos", "line", "linen", "linenr", "lineto", "linlin", "linrand", "linseg", "linsegb", "linsegr", "liveconv", "locsend", "locsig", "log", "log10", "log2", "logbtwo", "logcurve", "loop_ge", "loop_gt", "loop_le", "loop_lt", "loopseg", "loopsegp", "looptseg", "loopxseg", "lorenz", "loscil", "loscil3", "loscil3phs", "loscilphs", "loscilx", "lowpass2", "lowres", "lowresx", "lpcanal", "lpcfilter", "lpf18", "lpform", "lpfreson", "lphasor", "lpinterp", "lposcil", "lposcil3", "lposcila", "lposcilsa", "lposcilsa2", "lpread", "lpreson", "lpshold", "lpsholdp", "lpslot", "lufs", "mac", "maca", "madsr", "mags", "mandel", "mandol", "maparray", "maparray_i", "marimba", "massign", "max", "max_k", "maxabs", "maxabsaccum", "maxaccum", "maxalloc", "maxarray", "mclock", "mdelay", "median", "mediank", "metro", "metro2", "metrobpm", "mfb", "midglobal", "midiarp", "midic14", "midic21", "midic7", "midichannelaftertouch", "midichn", "midicontrolchange", "midictrl", "mididefault", "midifilestatus", "midiin", "midinoteoff", "midinoteoncps", "midinoteonkey", "midinoteonoct", "midinoteonpch", "midion", "midion2", "midiout", "midiout_i", "midipgm", "midipitchbend", "midipolyaftertouch", "midiprogramchange", "miditempo", "midremot", "min", "minabs", "minabsaccum", "minaccum", "minarray", "mincer", "mirror", "mode", "modmatrix", "monitor", "moog", "moogladder", "moogladder2", "moogvcf", "moogvcf2", "moscil", "mp3bitrate", "mp3in", "mp3len", "mp3nchnls", "mp3scal", "mp3sr", "mpulse", "mrtmsg", "ms2st", "mtof", "mton", "multitap", "mute", "mvchpf", "mvclpf1", "mvclpf2", "mvclpf3", "mvclpf4", "mvmfilter", "mxadsr", "nchnls_hw", "nestedap", "nlalp", "nlfilt", "nlfilt2", "noise", "noteoff", "noteon", "noteondur", "noteondur2", "notnum", "nreverb", "nrpn", "nsamp", "nstance", "nstrnum", "nstrstr", "ntof", "ntom", "ntrpol", "nxtpow2", "octave", "octcps", "octmidi", "octmidib", "octmidinn", "octpch", "olabuffer", "opcode", "oscbnk", "oscil", "oscil1", "oscil1i", "oscil3", "oscili", "oscilikt", "osciliktp", "oscilikts", "osciln", "oscils", "oscilx", "out", "out32", "outall", "outc", "outch", "outh", "outiat", "outic", "outic14", "outipat", "outipb", "outipc", "outkat", "outkc", "outkc14", "outkpat", "outkpb", "outkpc", "outleta", "outletf", "outletk", "outletkid", "outletv", "outo", "outq", "outq1", "outq2", "outq3", "outq4", "outrg", "outs", "outs1", "outs2", "outvalue", "outx", "outz", "p", "pan", "pan2", "pareq", "part2txt", "partials", "partikkel", "partikkelget", "partikkelset", "partikkelsync", "passign", "paulstretch", "pcauchy", "pchbend", "pchmidi", "pchmidib", "pchmidinn", "pchoct", "pchtom", "pconvolve", "pcount", "pdclip", "pdhalf", "pdhalfy", "peak", "pgmassign", "pgmchn", "phaser1", "phaser2", "phasor", "phasorbnk", "phs", "pindex", "pinker", "pinkish", "pitch", "pitchac", "pitchamdf", "planet", "platerev", "plltrack", "pluck", "poisson", "pol2rect", "polyaft", "polynomial", "port", "portk", "poscil", "poscil3", "pow", "powershape", "powoftwo", "pows", "prealloc", "prepiano", "print", "print_type", "printarray", "printf", "printf_i", "printk", "printk2", "printks", "printks2", "println", "prints", "printsk", "product", "pset", "ptablew", "ptrack", "puts", "pvadd", "pvbufread", "pvcross", "pvinterp", "pvoc", "pvread", "pvs2array", "pvs2tab", "pvsadsyn", "pvsanal", "pvsarp", "pvsbandp", "pvsbandr", "pvsbandwidth", "pvsbin", "pvsblur", "pvsbuffer", "pvsbufread", "pvsbufread2", "pvscale", "pvscent", "pvsceps", "pvscfs", "pvscross", "pvsdemix", "pvsdiskin", "pvsdisp", "pvsenvftw", "pvsfilter", "pvsfread", "pvsfreeze", "pvsfromarray", "pvsftr", "pvsftw", "pvsfwrite", "pvsgain", "pvsgendy", "pvshift", "pvsifd", "pvsin", "pvsinfo", "pvsinit", "pvslock", "pvslpc", "pvsmaska", "pvsmix", "pvsmooth", "pvsmorph", "pvsosc", "pvsout", "pvspitch", "pvstanal", "pvstencil", "pvstrace", "pvsvoc", "pvswarp", "pvsynth", "pwd", "qinf", "qnan", "r2c", "rand", "randc", "randh", "randi", "random", "randomh", "randomi", "rbjeq", "readclock", "readf", "readfi", "readk", "readk2", "readk3", "readk4", "readks", "readscore", "readscratch", "rect2pol", "reinit", "release", "remoteport", "remove", "repluck", "reshapearray", "reson", "resonbnk", "resonk", "resonr", "resonx", "resonxk", "resony", "resonz", "resyn", "return", "reverb", "reverb2", "reverbsc", "rewindscore", "rezzy", "rfft", "rifft", "rigoto", "rireturn", "rms", "rnd", "rnd31", "rndseed", "round", "rspline", "rtclock", "s16b14", "s32b14", "samphold", "sandpaper", "sc_lag", "sc_lagud", "sc_phasor", "sc_trig", "scale", "scale2", "scalearray", "scanhammer", "scanmap", "scans", "scansmap", "scantable", "scanu", "scanu2", "schedkwhen", "schedkwhennamed", "schedule", "schedulek", "schedwhen", "scoreline", "scoreline_i", "seed", "sekere", "select", "semitone", "sense", "sensekey", "seqtime", "seqtime2", "sequ", "sequstate", "serialBegin", "serialEnd", "serialFlush", "serialPrint", "serialRead", "serialWrite", "serialWrite_i", "setcol", "setctrl", "setksmps", "setrow", "setscorepos", "sfilist", "sfinstr", "sfinstr3", "sfinstr3m", "sfinstrm", "sfload", "sflooper", "sfpassign", "sfplay", "sfplay3", "sfplay3m", "sfplaym", "sfplist", "sfpreset", "shaker", "shiftin", "shiftout", "signum", "sin", "sinh", "sininv", "sinsyn", "skf", "sleighbells", "slicearray", "slicearray_i", "slider16", "slider16f", "slider16table", "slider16tablef", "slider32", "slider32f", "slider32table", "slider32tablef", "slider64", "slider64f", "slider64table", "slider64tablef", "slider8", "slider8f", "slider8table", "slider8tablef", "sliderKawai", "sndloop", "sndwarp", "sndwarpst", "sockrecv", "sockrecvs", "socksend", "socksends", "sorta", "sortd", "soundin", "space", "spat3d", "spat3di", "spat3dt", "spdist", "spf", "splitrig", "sprintf", "sprintfk", "spsend", "sqrt", "squinewave", "st2ms", "statevar", "sterrain", "stix", "strcat", "strcatk", "strchar", "strchark", "strcmp", "strcmpk", "strcpy", "strcpyk", "strecv", "streson", "strfromurl", "strget", "strindex", "strindexk", "string2array", "strlen", "strlenk", "strlower", "strlowerk", "strrindex", "strrindexk", "strset", "strstrip", "strsub", "strsubk", "strtod", "strtodk", "strtol", "strtolk", "strupper", "strupperk", "stsend", "subinstr", "subinstrinit", "sum", "sumarray", "svfilter", "svn", "syncgrain", "syncloop", "syncphasor", "system", "system_i", "tab", "tab2array", "tab2pvs", "tab_i", "tabifd", "table", "table3", "table3kt", "tablecopy", "tablefilter", "tablefilteri", "tablegpw", "tablei", "tableicopy", "tableigpw", "tableikt", "tableimix", "tablekt", "tablemix", "tableng", "tablera", "tableseg", "tableshuffle", "tableshufflei", "tablew", "tablewa", "tablewkt", "tablexkt", "tablexseg", "tabmorph", "tabmorpha", "tabmorphak", "tabmorphi", "tabplay", "tabrec", "tabsum", "tabw", "tabw_i", "tambourine", "tan", "tanh", "taninv", "taninv2", "tbvcf", "tempest", "tempo", "temposcal", "tempoval", "tigoto", "timedseq", "timeinstk", "timeinsts", "timek", "times", "timout", "tival", "tlineto", "tone", "tonek", "tonex", "tradsyn", "trandom", "transeg", "transegb", "transegr", "trcross", "trfilter", "trhighest", "trigExpseg", "trigLinseg", "trigexpseg", "trigger", "trighold", "triglinseg", "trigphasor", "trigseq", "trim", "trim_i", "trirand", "trlowest", "trmix", "trscale", "trshift", "trsplit", "turnoff", "turnoff2", "turnoff2_i", "turnoff3", "turnon", "tvconv", "unirand", "unwrap", "upsamp", "urandom", "urd", "vactrol", "vadd", "vadd_i", "vaddv", "vaddv_i", "vaget", "valpass", "vaset", "vbap", "vbapg", "vbapgmove", "vbaplsinit", "vbapmove", "vbapz", "vbapzmove", "vcella", "vclpf", "vco", "vco2", "vco2ft", "vco2ift", "vco2init", "vcomb", "vcopy", "vcopy_i", "vdel_k", "vdelay", "vdelay3", "vdelayk", "vdelayx", "vdelayxq", "vdelayxs", "vdelayxw", "vdelayxwq", "vdelayxws", "vdivv", "vdivv_i", "vecdelay", "veloc", "vexp", "vexp_i", "vexpseg", "vexpv", "vexpv_i", "vibes", "vibr", "vibrato", "vincr", "vlimit", "vlinseg", "vlowres", "vmap", "vmirror", "vmult", "vmult_i", "vmultv", "vmultv_i", "voice", "vosim", "vphaseseg", "vport", "vpow", "vpow_i", "vpowv", "vpowv_i", "vps", "vpvoc", "vrandh", "vrandi", "vsubv", "vsubv_i", "vtaba", "vtabi", "vtabk", "vtable1k", "vtablea", "vtablei", "vtablek", "vtablewa", "vtablewi", "vtablewk", "vtabwa", "vtabwi", "vtabwk", "vwrap", "waveset", "weibull", "wgbow", "wgbowedbar", "wgbrass", "wgclar", "wgflute", "wgpluck", "wgpluck2", "wguide1", "wguide2", "window", "wrap", "writescratch", "wterrain", "wterrain2", "xadsr", "xin", "xout", "xtratim", "xyscale", "zacl", "zakinit", "zamod", "zar", "zarg", "zaw", "zawm", "zdf_1pole", "zdf_1pole_mode", "zdf_2pole", "zdf_2pole_mode", "zdf_ladder", "zfilter2", "zir", "ziw", "ziwm", "zkcl", "zkmod", "zkr", "zkw", "zkwm"
    });
}

bool CsoundTokeniser::isIdentifierStart (juce::juce_wchar c) noexcept
{
    return juce::CharacterFunctions::isLetter (c) || c == '_';
}

bool CsoundTokeniser::isIdentifierBody (juce::juce_wchar c) noexcept
{
    return juce::CharacterFunctions::isLetterOrDigit (c) || c == '_';
}

void CsoundTokeniser::skipToEndOfLine (juce::CodeDocument::Iterator& source)
{
    while (! source.isEOF())
        if (source.nextChar() == '\n')
            break;
}

void CsoundTokeniser::skipBlockComment (juce::CodeDocument::Iterator& source)
{
    juce::juce_wchar last = 0;

    while (! source.isEOF())
    {
        auto c = source.nextChar();

        if (last == '*' && c == '/')
            break;

        last = c;
    }
}

int CsoundTokeniser::readNextToken (juce::CodeDocument::Iterator& source)
{
    source.skipWhitespace();

    const auto firstChar = source.peekNextChar();

    if (source.isEOF() || firstChar == 0)
        return tokenType_default;

    // Commenti riga singola: ';' oppure '//'
    if (firstChar == ';')
    {
        skipToEndOfLine (source);
        return tokenType_comment;
    }

    if (firstChar == '/')
    {
        source.skip();

        if (source.peekNextChar() == '/')
        {
            skipToEndOfLine (source);
            return tokenType_comment;
        }

        if (source.peekNextChar() == '*')
        {
            source.skip();
            skipBlockComment (source);
            return tokenType_comment;
        }

        return tokenType_punctuation;
    }

    // Direttive del preprocessore Csound: #define, #include, #ifdef ecc.
    if (firstChar == '#')
    {
        skipToEndOfLine (source);
        return tokenType_preprocessor;
    }

    // Stringhe tra doppi apici
    if (firstChar == '"')
    {
        source.skip();

        while (! source.isEOF())
        {
            const auto c = source.nextChar();

            if (c == '\\' && ! source.isEOF())
            {
                source.skip();
                continue;
            }

            if (c == '"')
                break;
        }

        return tokenType_string;
    }

    // Numeri (interi, decimali, notazione esponenziale)
    if (juce::CharacterFunctions::isDigit (firstChar))
    {
        source.skip();

        while (juce::CharacterFunctions::isDigit (source.peekNextChar()))
            source.skip();

        if (source.peekNextChar() == '.')
        {
            source.skip();

            while (juce::CharacterFunctions::isDigit (source.peekNextChar()))
                source.skip();
        }

        if (source.peekNextChar() == 'e' || source.peekNextChar() == 'E')
        {
            source.skip();

            if (source.peekNextChar() == '+' || source.peekNextChar() == '-')
                source.skip();

            while (juce::CharacterFunctions::isDigit (source.peekNextChar()))
                source.skip();
        }

        return tokenType_number;
    }

    // Identificatori, keyword e opcode. Copre anche i prefissi di scope
    // globale di Csound (gi, gk, ga, gS...) perche' fanno parte del nome.
    if (isIdentifierStart (firstChar))
    {
        juce::String identifier;
        identifier += source.nextChar();

        while (isIdentifierBody (source.peekNextChar()))
            identifier += source.nextChar();

        if (keywords.contains (identifier))
            return tokenType_keyword;

        if (opcodes.contains (identifier))
            return tokenType_opcode;

        return tokenType_identifier;
    }

    // Tutto il resto: operatori, parentesi, virgole...
    source.skip();
    return tokenType_punctuation;
}

juce::CodeEditorComponent::ColourScheme CsoundTokeniser::getDefaultColourScheme()
{
    struct TokenColour { const char* name; juce::uint32 colour; };

    // Palette per sfondo CHIARO (l'editor ora ha uno sfondo bianco, vedi
    // CsoundLookAndFeel): colori scuri e ben contrastati, sullo stile
    // Xcode/VS Code "light". L'ordine deve corrispondere ai valori
    // dell'enum TokenType.
    static const TokenColour types[] =
    {
        { "Default",      0xff1f2933 }, // testo generale: grigio-blu scuro
        { "Comment",      0xff177500 }, // verde scuro
        { "Keyword",      0xffaa0d91 }, // viola/magenta (instr, if, endin...)
        { "Opcode",       0xff0b5d8c }, // blu petrolio (poscil, outs...)
        { "Identifier",   0xff1f2933 }, // come il default
        { "Number",       0xff1c00cf }, // blu
        { "String",       0xffc41a16 }, // rosso
        { "Preprocessor", 0xff643820 }, // marrone
        { "Punctuation",  0xff4d4d4d }  // grigio medio
    };

    juce::CodeEditorComponent::ColourScheme scheme;

    for (auto& t : types)
        scheme.set (t.name, juce::Colour (t.colour));

    return scheme;
}
