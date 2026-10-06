#pragma once

#include <JuceHeader.h>

/**
    Mini-database di help inline per gli opcode Csound piu' comuni: sintassi
    (trascritta dai file sorgente XML del Csound Reference Manual ufficiale,
    github.com/csound/manual, lo stesso materiale da cui attinge CsoundQt)
    + una riga di descrizione. Usata da CsoundCodeEditor per riempire la
    barra di help fissa quando il caret si trova su un opcode conosciuto.

    A differenza di una prima versione di questa tabella, qui i nomi degli
    argomenti nella sintassi sono quelli REALI usati dal manuale (xamp,
    kcps, ifn, iphs...) e non segnaposto generici (a1, k2, i3) - e' quello
    che rende l'help "identico a CsoundQT", come richiesto.

    Non e' esaustiva (Csound ha oltre 1700 opcode): contiene gli opcode piu'
    frequenti in sintesi/MIDI/filtri/envelope/random/tabelle/I-O, ciascuno
    verificato contro il proprio file sorgente XML (non contro l'indice
    HTML del manuale, che per la sua dimensione viene troncato dal fetch).
    Per gli opcode non presenti qui, CsoundCodeEditor sintetizza comunque
    una riga di sintassi a partire dai tipi realmente riportati dal motore
    Csound (csoundNewOpcodeList), ma con nomi di argomento generici.
    Estendere questa tabella (un opcode alla volta, dal relativo file
    opcodes/<nome>.xml nel repository del manuale) e' il modo naturale per
    ampliare la copertura "di qualita'" in futuro.
*/
struct CsoundOpcodeHelp
{
    const char* name;
    const char* syntax;      // una o piu' righe, come nel manuale
    const char* description;
};

/**
    Un opcode come riportato dal motore Csound realmente collegato
    (csoundNewOpcodeList/opcodeListEntry, vedi CSOUND API), non scritto a
    mano: name/outTypes/inTypes sono le stesse stringhe di tipo compatte
    che Csound usa internamente (es. 'a', 'k', 'i', 'S', 'x', 'o', 'z'...).
    Questa e' la fonte usata per coprire TUTTI gli opcode effettivamente
    disponibili in questa build di Csound (inclusi eventuali plugin di
    terze parti), non solo il sottoinsieme descritto a mano in
    CsoundOpcodeHelpData::table. Vedi CsoundCodeEditor per come viene
    formattata e combinata con le descrizioni scritte a mano.
*/
struct CsoundLiveOpcodeInfo
{
    juce::String name, outTypes, inTypes;
};

namespace CsoundOpcodeHelpData
{
    // Tabella statica, ordine alfabetico. Ogni riga e' stata verificata
    // contro il file sorgente XML dell'opcode nel Csound Reference Manual
    // (github.com/csound/manual/tree/master/opcodes).
    static const CsoundOpcodeHelp table[] =
    {
        { "ampmidi",    "iamp ampmidi iscal [, ifn]",
          "Velocity of the current MIDI event, scaled to the range 0-iscal (optionally through function table ifn)." },
        { "butterhp",   "ares butterhp asig, kfreq [, iskip]\nares butterhp asig, afreq [, iskip]",
          "Second-order high-pass Butterworth filter, with an almost flat passband." },
        { "butterlp",   "ares butterlp asig, kfreq [, iskip]\nares butterlp asig, afreq [, iskip]",
          "Second-order low-pass Butterworth filter, with an almost flat passband." },
        { "chnget",     "kval chnget Sname\nival chnget Sname\naval chnget Sname\nSval chnget Sname",
          "Reads a value from the named software bus channel Sname." },
        { "chnset",     "chnset kval, Sname\nchnset ival, Sname\nchnset aval, Sname\nchnset Sval, Sname",
          "Writes a value to the named software bus channel Sname." },
        { "cpsmidi",    "icps cpsmidi",
          "Note number of the current MIDI event, expressed in cycles-per-second." },
        { "delay",      "ares delay asig, idlt [, iskip]",
          "Delays an audio signal by idlt seconds, with no feedback." },
        { "diskin2",    "a1[, a2 [...]] diskin2 ifilcod [, kpitch] [, iskiptim] [, iwrap] [, iformat] [, iwsize] [, ibufsize] [, iskipinit]",
          "Streams audio from a soundfile on disk, with pitch-shifting (kpitch) and a choice of interpolation quality (iwsize)." },
        { "expseg",     "ares expseg ia, idur1, ib [, idur2] [, ic] [...]\nkres expseg ia, idur1, ib [, idur2] [, ic] [...]",
          "Traces exponential segments between the given points; values cannot cross or equal zero." },
        { "ftgen",      "gir ftgen ifn, itime, isize, igen, iarga [, iargb] [...]",
          "Generates a function table from the orchestra; equivalent to a score f statement." },
        { "inch",       "ain1[, ain2 [...]] inch kchan1[, kchan2 [...]]",
          "Reads audio from numbered channels of an external input/stream (e.g. the audio interface)." },
        { "line",       "ares line ia, idur, ib\nkres line ia, idur, ib",
          "Generates a linear ramp from ia to ib over idur seconds; does not stop once idur has elapsed." },
        { "linen",      "ares linen xamp, irise, idur, idec\nkres linen kamp, irise, idur, idec",
          "Applies a straight-line rise and decay shape to an amplitude signal." },
        { "linseg",     "ares linseg ia, idur1, ib [, idur2] [, ic] [...]\nkres linseg ia, idur1, ib [, idur2] [, ic] [...]",
          "Traces a series of straight line segments between the given points." },
        { "madsr",      "ares madsr iatt, idec, islev, irel [, idel] [, ireltim]\nkres madsr iatt, idec, islev, irel [, idel] [, ireltim]",
          "Classic ADSR envelope via linsegr, suited to MIDI notes (releases on note-off)." },
        { "massign",    "massign ichnl, insnum [, ireset]\nmassign ichnl, \"insname\" [, ireset]",
          "Assigns a Csound instrument to be triggered by incoming MIDI on channel ichnl (0 = all channels)." },
        { "mididefault","mididefault xdefault, xvalue",
          "During MIDI activation, overwrites xvalue with xdefault; leaves xvalue unchanged for score-activated notes." },
        { "moogladder", "asig moogladder ain, kcf, kres [, istor]\nasig moogladder ain, acf, kres [, istor]\nasig moogladder ain, kcf, ares [, istor]\nasig moogladder ain, acf, ares [, istor]",
          "Moog ladder low-pass filter emulation; kcf is cutoff frequency, kres is resonance (self-oscillates near 1)." },
        { "moogvcf",    "ares moogvcf asig, xfco, xres [, iscale] [, iskip]",
          "Digital emulation of the Moog diode ladder filter; xres near 1 causes self-oscillation." },
        { "notnum",     "ival notnum",
          "MIDI note number (0-127) of the current event." },
        { "oscil",      "ares oscil xamp, xcps [, ifn, iphs]\nkres oscil kamp, kcps [, ifn, iphs]",
          "Simple oscillator: reads table ifn cyclically at frequency xcps, scaled by xamp." },
        { "oscil3",     "ares oscil3 xamp, xcps [, ifn, iphs]\nkres oscil3 kamp, kcps [, ifn, iphs]",
          "Like oscili, but uses cubic interpolation for table lookup." },
        { "oscili",     "ares oscili xamp, xcps [, ifn, iphs]\nkres oscili kamp, kcps [, ifn, iphs]",
          "Simple oscillator with linear interpolation between table points." },
        { "oscilikt",   "ares oscilikt xamp, xcps, kfn [, iphs] [, istor]\nkres oscilikt kamp, kcps, kfn [, iphs] [, istor]",
          "Linearly interpolated oscillator that allows changing the function table kfn at k-rate (waveform morphing)." },
        { "outs",       "outs asig1, asig2",
          "Writes a stereo pair of audio signals to the output (channel count must match nchnls)." },
        { "pan2",       "a1, a2 pan2 asig, xp [, imode]",
          "Distributes a mono signal across two channels; xp=0 is hard left, 1 is hard right; imode selects the panning law." },
        { "phasor",     "ares phasor xcps [, iphs]\nkres phasor kcps [, iphs]",
          "Produces a normalized moving phase value (0 to <1), useful as a table index to build custom oscillators." },
        { "poscil",     "ares poscil aamp, acps [, ifn, iphs]\nares poscil aamp, kcps [, ifn, iphs]\nares poscil kamp, acps [, ifn, iphs]\nares poscil kamp, kcps [, ifn, iphs]\nires poscil kamp, kcps [, ifn, iphs]\nkres poscil kamp, kcps [, ifn, iphs]",
          "High precision oscillator using floating-point table indexing; like oscili but more accurate, especially at low frequencies." },
        { "poscil3",    "ares poscil3 aamp, acps [, ifn, iphs]\nares poscil3 aamp, kcps [, ifn, iphs]\nares poscil3 kamp, acps [, ifn, iphs]\nares poscil3 kamp, kcps [, ifn, iphs]\nires poscil3 kamp, kcps [, ifn, iphs]\nkres poscil3 kamp, kcps [, ifn, iphs]",
          "High precision oscillator with cubic interpolation (like poscil, but smoother)." },
        { "printk",     "printk itime, kval [, ispace] [, inamed]",
          "Prints the value of kval to the console every itime seconds (0 = every k-cycle)." },
        { "rand",       "ares rand xamp [, iseed] [, isel] [, ioffset]\nkres rand xamp [, iseed] [, isel] [, ioffset]",
          "Generates a uniform random number series between -xamp and +xamp." },
        { "randh",      "ares randh xamp, xcps [, iseed] [, isize] [, ioffset]\nkres randh kamp, kcps [, iseed] [, isize] [, ioffset]",
          "Like rand, but holds each new random value for 1/xcps seconds (band-limited noise)." },
        { "randi",      "ares randi xamp, xcps [, iseed] [, isize] [, ioffset]\nkres randi kamp, kcps [, iseed] [, isize] [, ioffset]",
          "Like randh, but linearly interpolates between successive random values." },
        { "random",     "ares random kmin, kmax\nires random imin, imax\nkres random kmin, kmax",
          "Generates a uniform pseudo-random number series between min and max." },
        { "release",    "kflag release",
          "Returns 1 while the current note is in its release stage (after note-off, with xtratim extending it), 0 otherwise." },
        { "reson",      "ares reson asig, xcf, xbw [, iscl] [, iskip]",
          "Second-order resonant bandpass filter; xcf is center frequency, xbw is bandwidth in Hz." },
        { "resonz",     "ares resonz asig, xcf, xbw [, iscl] [, iskip]",
          "Two-zero resonant bandpass filter; more constant-gain than reson as the center frequency is swept." },
        { "reverb",     "ares reverb asig, krvt [, iskip]",
          "'Natural reverberation' reverb unit; krvt is the time, in seconds, to decay 60dB." },
        { "rezzy",      "ares rezzy asig, xfco, xres [, imode] [, iskip]",
          "A resonant low-pass filter (or high-pass if imode is non-zero)." },
        { "schedkwhen", "schedkwhen ktrigger, kmintim, kmaxnum, kinsnum, kwhen, kdur [, ip4] [...]\nschedkwhen ktrigger, kmintim, kmaxnum, \"insname\", kwhen, kdur [, ip4] [...]",
          "Triggers a new score event at k-rate when ktrigger is non-zero, limited by minimum interval and max instances." },
        { "schedule",   "schedule insnum, iwhen, idur [, ip4] [...]\nschedule \"insname\", iwhen, idur [, ip4] [...]",
          "Schedules a new score event (like a score i statement) from inside the orchestra." },
        { "seed",       "seed ival",
          "Sets the global seed for the x-class noise generators; ival = 0 seeds from the system clock." },
        { "soundin",    "ar1[, ar2 [...]] soundin ifilcod [, iskptim] [, iformat] [, iskipinit] [, ibufsize]",
          "Reads audio data directly from a soundfile; the number of output arguments must match the file's channel count." },
        { "table",      "ares table andx, ifn [, ixmode] [, ixoff] [, iwrap]\nires table indx, ifn [, ixmode] [, ixoff] [, iwrap]\nkres table kndx, ifn [, ixmode] [, ixoff] [, iwrap]",
          "Reads a function table by direct (non-interpolating) indexing." },
        { "tablei",     "ares tablei andx, ifn [, ixmode] [, ixoff] [, iwrap]\nires tablei indx, ifn [, ixmode] [, ixoff] [, iwrap]\nkres tablei kndx, ifn [, ixmode] [, ixoff] [, iwrap]",
          "Like table, but with linear interpolation between adjacent entries." },
        { "tablew",     "tablew asig, andx, ifn [, ixmode] [, ixoff] [, iwgmode]\ntablew isig, indx, ifn [, ixmode] [, ixoff] [, iwgmode]\ntablew ksig, kndx, ifn [, ixmode] [, ixoff] [, iwgmode]",
          "Writes a value into an existing function table at the given index (always runs at k-rate)." },
        { "timout",     "timout istrt, idur, label",
          "Branches to label during the time window [istrt, istrt+idur), measured from the start of the note." },
        { "tone",       "ares tone asig, khp [, iskip]",
          "First-order recursive low-pass filter; khp is the half-power point, in Hz." },
        { "tonek",      "kres tonek ksig, khp [, iskip]",
          "Like tone, but for a k-rate signal, output at control-rate." },
        { "transeg",    "ares transeg ia, idur, itype, ib [, idur2] [, itype2] [, ic] [...]\nkres transeg ia, idur, itype, ib [, idur2] [, itype2] [, ic] [...]",
          "User-definable envelope: itype=0 gives a straight line, non-zero gives a concave/convex exponential-style curve." },
        { "turnoff",    "turnoff\nturnoff inst\nturnoff knst",
          "Turns off the currently running instrument (no args), or another running instance by its handle." },
        { "vco",        "ares vco xamp, xcps, iwave, kpw [, ifn] [, imaxd] [, ileak] [, inyx] [, iphs] [, iskip]",
          "Band-limited, analog-modeled oscillator; iwave selects sawtooth (1), square/PWM (2) or triangle/ramp (3)." },
        { "vco2",       "ares vco2 kamp, kcps [, imode] [, kpw] [, kphs] [, inyx]",
          "Band-limited oscillator using pre-calculated tables of minimum-phase impulses; imode selects the waveform." },
        { "vdelay",     "ares vdelay asig, adel, imaxdel [, iskip]",
          "Interpolating variable-time delay; adel is the current delay time, in milliseconds." },
        { "vdelayx",    "aout vdelayx ain, adl, imd, iws [, ist]",
          "High quality variable delay with adjustable interpolation window size iws; adl is the delay time, in seconds." },
        { "veloc",      "ival veloc [ilow] [, ihigh]",
          "MIDI velocity (0-127) of the current event, optionally remapped to the ilow-ihigh range." },
        { "xin",        "xinarg1 [, xinarg2] [...] xin",
          "Inside a user-defined opcode (UDO), copies the caller's arguments into local variables." },
        { "xout",       "xout xoutarg1 [, xoutarg2] [...]",
          "Inside a user-defined opcode (UDO), copies local variables back out as the opcode's output arguments." },
    };

    inline const CsoundOpcodeHelp* find (const juce::String& opcodeName)
    {
        const auto lower = opcodeName.trim().toLowerCase();

        if (lower.isEmpty())
            return nullptr;

        for (auto& entry : table)
            if (lower == juce::String (entry.name))
                return &entry;

        return nullptr;
    }
}
