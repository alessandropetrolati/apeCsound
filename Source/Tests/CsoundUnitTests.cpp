#include "CsoundUnitTests.h"

#include "../CsoundParameterGroups.h"
#include "../CsoundCabbageHelpers.h"
#include "../CsoundLayoutHelpers.h"
#include "../CsoundSyntaxHelpers.h"
#include "../CsoundMonitorLayout.h"
#include "../CsoundChannelDiagnostics.h"

#ifndef CSOUND_TESTS_CORE_ONLY
 #include "../PluginProcessor.h"
#endif

namespace csoundTests
{

static constexpr const char* kCategory = "apeCsound";

//==============================================================================
struct ParameterGroupsTests final : public juce::UnitTest
{
    ParameterGroupsTests() : juce::UnitTest ("CsoundParameterGroups::parse", kCategory) {}

    void runTest() override
    {
        using namespace CsoundParameterGroups;

        beginTest ("@group names with spaces (ButtonTest: \"Event lines\")");
        {
            const auto p = parse ("instr 1\n"
                                  "; @group Event lines\n"
                                  "kb chnget \"Bell\"\n"
                                  "kp chnget \"Pad\"\n"
                                  "; @group Events from the code\n"
                                  "kc chnget \"Chord\"\n"
                                  "endin\n");
            expectEquals (p.groupNames[0], juce::String ("Event lines"));
            expectEquals (p.groupNames[1], juce::String ("Events from the code"));
            expectEquals (p.channels.at ("Bell").group, 0);
            expectEquals (p.channels.at ("Pad").group, 0);
            expectEquals (p.channels.at ("Chord").group, 1);
        }

        beginTest ("@group assigns the following chnget lines, in order");
        {
            const auto p = parse ("instr 1\n"
                                  "; @group Filter\n"
                                  "kcf chnget \"cutoff\"\n"
                                  "kq  chnget \"res\"\n"
                                  "; @group Amp\n"
                                  "ka  chnget \"amp\"\n"
                                  "endin\n");
            expectEquals (p.groupNames.size(), 2);
            expectEquals (p.groupNames[0], juce::String ("Filter"));
            expectEquals (p.groupNames[1], juce::String ("Amp"));
            expectEquals (p.channels.at ("cutoff").group, 0);
            expectEquals (p.channels.at ("res").group, 0);
            expectEquals (p.channels.at ("amp").group, 1);
            expect (p.channels.at ("cutoff").order < p.channels.at ("res").order);
            expect (p.channels.at ("res").order < p.channels.at ("amp").order);
        }

        beginTest ("no @group -> no groups, channels still ordered");
        {
            const auto p = parse ("ka chnget \"a\"\nkb chnget \"b\"\n");
            expect (! p.hasGroups());
            expectEquals ((int) p.channels.size(), 2);
            expect (p.channels.at ("a").order < p.channels.at ("b").order);
        }

        beginTest ("block comments and ; comments are ignored");
        {
            const auto p = parse ("/* ; @group Hidden\n kx chnget \"x\" */\n"
                                  "ky chnget \"y\" ; @group NotAGroup\n");
            expect (! p.hasGroups());
            expectEquals ((int) p.channels.count ("x"), 0);
            expectEquals ((int) p.channels.count ("y"), 1);
        }

        beginTest ("; @xy \"X\" \"Y\" -> one pad");
        {
            const auto p = parse ("; @xy \"cut\" \"res\"\nkc chnget \"cut\"\nkr chnget \"res\"\n");
            expectEquals ((int) p.xyPads.size(), 1);
            expectEquals (p.xyPads[0].x, juce::String ("cut"));
            expectEquals (p.xyPads[0].y, juce::String ("res"));
        }

        beginTest ("Cabbage xypad widget (outside the orchestra) -> pad");
        {
            const auto p = parse ("<Cabbage>\n"
                                  "xypad bounds(0,0,100,100), channel(\"Az\",\"Elev\"), rangeX(-180,180,0), rangeY(-40,90,0)\n"
                                  "</Cabbage>\n<CsoundSynthesizer>\n<CsInstruments>\n"
                                  "kaz chnget \"Az\"\nkel chnget \"Elev\"\n"
                                  "</CsInstruments>\n</CsoundSynthesizer>\n");
            expectEquals ((int) p.xyPads.size(), 1);
            expectEquals (p.xyPads[0].x, juce::String ("Az"));
            expectEquals (p.xyPads[0].y, juce::String ("Elev"));
        }

        beginTest ("CsoundQt BSBController objectName/objectName2 -> pad, only inside the element");
        {
            const auto p = parse ("<CsoundSynthesizer><CsInstruments>\nkx chnget \"px\"\nky chnget \"py\"\nkz chnget \"other\"\n"
                                  "</CsInstruments></CsoundSynthesizer>\n"
                                  "<bsbPanel>\n"
                                  "<bsbObject type=\"BSBController\"><objectName>px</objectName><objectName2>py</objectName2></bsbObject>\n"
                                  "<bsbObject type=\"BSBKnob\"><objectName>other</objectName></bsbObject>\n"
                                  "</bsbPanel>\n");
            expectEquals ((int) p.xyPads.size(), 1);
            expectEquals (p.xyPads[0].x, juce::String ("px"));
            expectEquals (p.xyPads[0].y, juce::String ("py"));
        }

        beginTest ("Cabbage groupbox { } -> group named after text(), @group in code wins");
        {
            const auto p = parse ("<Cabbage>\n"
                                  "groupbox bounds(0,0,200,100), text(\"Tone\") {\n"
                                  "rslider bounds(0,20,50,50), channel(\"cf\"), range(0,1,0.5)\n"
                                  "rslider bounds(60,20,50,50), channel(\"q\"), range(0,1,0.5)\n"
                                  "}\n"
                                  "rslider bounds(0,120,50,50), channel(\"vol\"), range(0,1,0.5)\n"
                                  "</Cabbage>\n<CsoundSynthesizer><CsInstruments>\n"
                                  "kc chnget \"cf\"\nkq chnget \"q\"\n"
                                  "; @group Mix\nkv chnget \"vol\"\n"   // @group applies to every chnget that follows
                                  "</CsInstruments></CsoundSynthesizer>\n");
            expect (p.hasGroups());
            expect (p.groupNames.contains ("Tone"));
            expect (p.groupNames.contains ("Mix"));
            expectEquals (p.groupNames[p.channels.at ("cf").group], juce::String ("Tone"));
            expectEquals (p.groupNames[p.channels.at ("q").group],  juce::String ("Tone"));
            expectEquals (p.groupNames[p.channels.at ("vol").group], juce::String ("Mix"));
        }

        beginTest ("Cabbage groupbox with the brace on the next line");
        {
            const auto p = parse ("<Cabbage>\n"
                                  "groupbox bounds(0,0,200,100), text(\"Env\")\n{\n"
                                  "rslider bounds(0,20,50,50), channel(\"att\"), range(0,1,0.5)\n"
                                  "}\n</Cabbage>\n<CsoundSynthesizer><CsInstruments>\nka chnget \"att\"\n</CsInstruments></CsoundSynthesizer>\n");
            expect (p.hasGroups());
            expectEquals (p.groupNames[p.channels.at ("att").group], juce::String ("Env"));
        }

        beginTest ("chnset channels get the group of their @group section (Monitor cards inside groups)");
        {
            const auto p = parse ("instr 1\n"
                                  "; @group Filter\n"
                                  "kcf chnget \"cutoff\"\n"
                                  "chnset kcf, \"CutoffOut\"\n"
                                  "chnset limit(kdb, -60, 0), \"Level\"   ; commas inside the expression\n"
                                  "chnset(kx, \"Func\")\n"
                                  "chnset:k(ky, \"FuncK\")\n"
                                  "chnset \"text\", \"Status\"            ; string value first\n"
                                  "chnsetks Sx, \"NotThisOne\"\n"
                                  "endin\n"
                                  "instr 2\n"
                                  "chnset k1, \"Ungrouped\"\n"
                                  "chnset k1, Sname                       ; name built at run time\n"
                                  "endin\n");
            expectEquals ((int) p.written.size(), 6);
            for (const char* n : { "CutoffOut", "Level", "Func", "FuncK", "Status" })
            {
                expect (p.written.count (n) == 1, n);
                if (p.written.count (n)) expectEquals (p.written.at (n).group, 0, n);
            }
            expectEquals (p.written.at ("Ungrouped").group, -1);
            expectEquals ((int) p.written.count ("NotThisOne"), 0);
            // order is shared with chnget: CutoffOut comes right after cutoff
            expect (p.channels.at ("cutoff").order < p.written.at ("CutoffOut").order);
            expect (p.written.at ("CutoffOut").order < p.written.at ("Level").order);
        }

        beginTest ("Cabbage groupbox also groups a meter written with chnset");
        {
            const auto p = parse ("<Cabbage>\n"
                                  "groupbox bounds(0,0,200,100), text(\"Meters\") {\n"
                                  "vmeter bounds(0,20,20,60), channel(\"vu\")\n"
                                  "}\n</Cabbage>\n<CsoundSynthesizer><CsInstruments>\nchnset krms, \"vu\"\n</CsInstruments></CsoundSynthesizer>\n");
            expect (p.written.count ("vu") == 1);
            if (p.written.count ("vu")) expectEquals (p.groupNames[p.written.at ("vu").group], juce::String ("Meters"));
        }

        beginTest ("parseNameList: quoted and bare names");
        {
            const auto a = parseNameList ("\"a b\" \"c\"");
            expectEquals (a.size(), 2);
            expectEquals (a[0], juce::String ("a b"));
            const auto b = parseNameList ("x, y z");
            expectEquals (b.size(), 3);
        }
    }
};

//==============================================================================
struct CabbageHelpersTests final : public juce::UnitTest
{
    CabbageHelpersTests() : juce::UnitTest ("Cabbage import helpers", kCategory) {}

    void runTest() override
    {
        using namespace cabbageImport;

        beginTest ("latched(): momentary buttons");
        {
            bool latched = true;
            expect (cabbageMatchLatched ("button channel(\"Go\"), latched(0)", latched));
            expect (! latched);
            expect (cabbageMatchLatched ("button channel(\"Go\"), latched( 1 )", latched));
            expect (latched);
            expect (! cabbageMatchLatched ("button channel(\"Go\")", latched), "absent: Cabbage default (latched)");
        }

        beginTest ("trigger code and CsoundQt event buttons");
        {
            expectEquals (csoundVariableSuffix ("Play"), juce::String ("Play"));
            expectEquals (csoundVariableSuffix ("my-btn 2"), juce::String ("my_btn_2"));
            expectEquals (csoundVariableSuffix ("1shot"), juce::String ("_1shot"));

            const auto code = csoundTriggerCode ("Play");
            expect (code.contains ("kPlay chnget \"Play\""));
            expect (code.contains ("trigger:k(kPlay, 0.5, 0) == 1"));
            expect (code.contains ("event \"i\""));

            expect (csoundEventButtonsInstrument ({}).isEmpty());
            const auto instr = csoundEventButtonsInstrument ({ { "Event1", "i1 0 10" }, { "Go", "i \"Name\" 0 2" } });
            expect (instr.contains ("instr apeEventButtons"));
            expect (instr.contains ("alwayson \"apeEventButtons\""));
            expect (instr.contains ("trigger:k(chnget:k(\"Event1\"), 0.5, 0) == 1"));
            expect (instr.contains ("scoreline {{i1 0 10}}, 1"));
            expect (instr.contains ("scoreline {{i \"Name\" 0 2}}, 1"), "quotes kept inside {{ }}");

            const auto held = csoundSlotEventsOrc ({ { "Pad", "i 14.1 0 -1", "i -14.1 0 0" } }, 5);
            expect (held.contains ("trigger:k(chnget:k(\"Pad\"), 0.5, 0) == 1"), "ON on the rising edge");
            expect (held.contains ("scoreline {{i 14.1 0 -1}}, 1"));
            expect (held.contains ("trigger:k(chnget:k(\"Pad\"), 0.5, 1) == 1"), "OFF on the falling edge");
            expect (held.contains ("scoreline {{i -14.1 0 0}}, 1"));
            const auto offOnly = csoundSlotEventsOrc ({ { "X", "", "i 2 0 1" } }, 6);
            expect (! offOnly.contains ("0.5, 0) == 1"), "no ON line: no rising-edge block");
            expect (offOnly.contains ("0.5, 1) == 1"));

            const auto orc = csoundSlotEventsOrc ({ { "Play", "i 10 0 1", {} } }, 3);
            expect (orc.startsWith ("gkApeSlotEvents init 3"));
            expect (orc.contains ("if gkApeSlotEvents != 3 then"), "an older instance turns itself off");
            expect (orc.contains ("trigger:k(chnget:k(\"Play\"), 0.5, 0) == 1"));
            expect (orc.contains ("scoreline {{i 10 0 1}}, 1"));
            expect (orc.contains ("schedule \"apeSlotEvents\", 0, -1"));
            expectEquals (csoundSlotEventsOrc ({}, 4).trim(), juce::String ("gkApeSlotEvents init 4"), "no events: only stops the old one");
            expectEquals (csoundScoreLiteral ("i 1 0 1}}"), juce::String ("\"i 1 0 1}}\""));

            expectEquals (csoundQtTurnOffLine ("i1 0 -1"), juce::String ("i -1 0 -1"));
            expectEquals (csoundQtTurnOffLine ("i 10.1 0 -1 440"), juce::String ("i -10.1 0 -1 440"));
            expectEquals (csoundQtTurnOffLine ("i \"Pad\" 0 -1"), juce::String ("i \"-Pad\" 0 -1"));
            expect (csoundQtTurnOffLine ("i1 0 3").isEmpty(), "positive duration: no OFF");
            expect (csoundQtTurnOffLine ("").isEmpty());
            expectEquals (channelNameFromLabel ("Play note"), juce::String ("Play_note"));
            expectEquals (channelNameFromLabel ("  >> Go! "), juce::String ("Go"));
            expectEquals (channelNameFromLabel ("1st"), juce::String ("_1st"));
            expect (channelNameFromLabel ("!!!").isEmpty());
        }

        beginTest ("leading identifier");
        {
            expectEquals (cabbageLeadingIdentifier ("rslider bounds(0,0,1,1), channel(\"a\")"), juce::String ("rslider"));
            expectEquals (cabbageLeadingIdentifier ("   checkbox channel(\"a\")"), juce::String ("checkbox"));
            expectEquals (cabbageLeadingIdentifier ("; rslider commented"), juce::String());
            expectEquals (cabbageLeadingIdentifier (""), juce::String());
        }

        beginTest ("single channel and channel pair");
        {
            juce::String ch;
            expect (cabbageMatchSingleChannel ("rslider channel(\"Cutoff\"), range(0,1,0.5)", ch));
            expectEquals (ch, juce::String ("Cutoff"));
            expect (cabbageMatchSingleChannel ("rslider channel( \"Spaced\" )", ch));
            expectEquals (ch, juce::String ("Spaced"));

            juce::String a, b;
            expect (cabbageMatchChannelPair ("xypad channel(\"x\",\"y\")", a, b));
            expectEquals (a, juce::String ("x"));
            expectEquals (b, juce::String ("y"));
            expect (! cabbageMatchChannelPair ("rslider channel(\"x\")", a, b));
        }

        beginTest ("range(min, max, default, skew, step)");
        {
            juce::String contents;
            expect (cabbageMatchRangeContents ("rslider channel(\"a\"), range(50, 1000, 200, 0.5, 1)", contents));
            double v[5] = { 0, 1, 0, 1, 0.001 };
            double defaultB = 0;
            expect (cabbageParseFiveFieldRange (contents, v, defaultB));
            expectWithinAbsoluteError (v[0], 50.0, 1e-9);
            expectWithinAbsoluteError (v[1], 1000.0, 1e-9);
            expectWithinAbsoluteError (v[2], 200.0, 1e-9);
            expectWithinAbsoluteError (v[3], 0.5, 1e-9);
            expectWithinAbsoluteError (v[4], 1.0, 1e-9);
        }

        beginTest ("range with 3 fields keeps skew/step defaults; 'a:b' default of a vrange");
        {
            double v[5] = { 0, 1, 0, 1, 0.001 };
            double defaultB = 0;
            expect (cabbageParseFiveFieldRange ("0, 1, 0.7", v, defaultB));
            expectWithinAbsoluteError (v[2], 0.7, 1e-9);
            expectWithinAbsoluteError (v[3], 1.0, 1e-9);
            expectWithinAbsoluteError (v[4], 0.001, 1e-9);
            expect (cabbageParseFiveFieldRange ("0, 100, 20:80", v, defaultB));
            expectWithinAbsoluteError (v[2], 20.0, 1e-9);
            expectWithinAbsoluteError (defaultB, 80.0, 1e-9);
        }

        beginTest ("value(), min(), max()");
        {
            double value = -1;
            expect (cabbageMatchValueNumber ("checkbox channel(\"a\"), value(1)", value));
            expectWithinAbsoluteError (value, 1.0, 1e-9);
            expect (! cabbageMatchValueNumber ("checkbox channel(\"a\")", value));
            double mn = 0, mx = 0;
            expect (cabbageMatchMinNumber ("encoder channel(\"e\"), min(-2), max(7.5)", mn));
            expect (cabbageMatchMaxNumber ("encoder channel(\"e\"), min(-2), max(7.5)", mx));
            expectWithinAbsoluteError (mn, -2.0, 1e-9);
            expectWithinAbsoluteError (mx, 7.5, 1e-9);
        }

        beginTest ("combobox text(...) / items(...)");
        {
            juce::StringArray items;
            expect (cabbageMatchItems ("combobox channel(\"m\"), text(\"Saw\", \"Square\",\"Tri\"), value(2)", items));
            expectEquals (items.size(), 3);
            expectEquals (items[1], juce::String ("Square"));
            items.clear();
            expect (cabbageMatchItems ("combobox channel(\"m\"), items(\"A\", \"B\")", items));
            expectEquals (items.size(), 2);
        }
    }
};

//==============================================================================
struct LayoutHelpersTests final : public juce::UnitTest
{
    LayoutHelpersTests() : juce::UnitTest ("Card field wrapping", kCategory) {}

    void runTest() override
    {
        using namespace csoundLayout;

        beginTest ("columns never exceed the number of fields, never below 1");
        {
            expectEquals (wrappedFieldColumns (10000, { 80, 80, 80 }, 8), 3);
            expectEquals (wrappedFieldColumns (10, { 80, 80, 80 }, 8), 1);
            expectEquals (wrappedFieldColumns (0, { 80 }, 8), 1);
        }

        beginTest ("rows * columns always covers every field (the bug: last field vanished)");
        {
            const std::initializer_list<int> five { 86, 86, 86, 86, 86 };
            for (int width = 50; width <= 700; ++width)
            {
                const int cols = wrappedFieldColumns (width, five, 10);
                const int rows = wrappedFieldRows (width, five, 10);
                expect (rows * cols >= 5, "width " + juce::String (width));
                expect ((rows - 1) * cols < 5, "no empty trailing row at width " + juce::String (width));
                // the columns really fit
                expect (cols == 1 || cols * 86 + (cols - 1) * 10 <= width, "overflow at width " + juce::String (width));
            }
        }

        beginTest ("exact fits");
        {
            expectEquals (wrappedFieldColumns (86 * 2 + 10, { 86, 86, 86, 86, 86 }, 10), 2);
            expectEquals (wrappedFieldColumns (86 * 2 + 9,  { 86, 86, 86, 86, 86 }, 10), 1);
            expectEquals (wrappedFieldRows (86 * 2 + 10, { 86, 86, 86, 86, 86 }, 10), 3);
        }
    }
};

//==============================================================================
struct SyntaxHelpersTests final : public juce::UnitTest
{
    SyntaxHelpersTests() : juce::UnitTest ("Syntax helpers", kCategory) {}

    void runTest() override
    {
        using namespace csoundSyntax;

        beginTest ("toModernSyntax: outputs get type annotations, inputs go in parentheses");
        {
            expectEquals (toModernSyntax ("ares oscili xamp, xcps [, ifn]", "oscili"),
                          juce::String ("ares:a = oscili(xamp, xcps [, ifn])"));
            expectEquals (toModernSyntax ("kcps, kamp ptrack asig, ihopsize", "ptrack"),
                          juce::String ("kcps:k, kamp:k = ptrack(asig, ihopsize)"));
            expectEquals (toModernSyntax ("gkres oscil kamp, kcps", "oscil"),
                          juce::String ("gkres:k = oscil(kamp, kcps)"));
            expectEquals (toModernSyntax ("kArr[] fillarray i1, i2", "fillarray"),
                          juce::String ("kArr:k[] = fillarray(i1, i2)"));
            expectEquals (toModernSyntax ("xres diff xsig", "diff"),
                          juce::String ("xres:a/k = diff(xsig)"));
        }

        beginTest ("toModernSyntax: no outputs, already functional, or opcode missing -> unchanged / no '='");
        {
            expectEquals (toModernSyntax ("outch kchan1, asig1", "outch"), juce::String ("outch(kchan1, asig1)"));
            expectEquals (toModernSyntax ("ares = oscili(1, 2)", "oscili"), juce::String ("ares = oscili(1, 2)"));
            expectEquals (toModernSyntax ("instr 1", "oscili"), juce::String ("instr 1"));
            expectEquals (toModernSyntax ("", "oscili"), juce::String());
        }

        beginTest ("channelNameInLine: literal under the caret, else the first; nothing without chn*");
        {
            const juce::String line = "kcf chnget \"cutoff\" ; chnset \"notme\"";
            expectEquals (channelNameInLine (line, 0), juce::String ("cutoff"));
            expectEquals (channelNameInLine (line, 13), juce::String ("cutoff"));
            expectEquals (channelNameInLine ("chnset kval, \"a\" ; x", 5), juce::String ("a"));
            expectEquals (channelNameInLine ("kx = chnget:k(\"xx\") + chnget:k(\"yy\")", 32), juce::String ("yy"));
            expectEquals (channelNameInLine ("kx = chnget:k(\"xx\") + chnget:k(\"yy\")", 0), juce::String ("xx"));
            expectEquals (channelNameInLine ("asig oscili 1, 440 ; \"quoted\"", 3), juce::String());
            expectEquals (channelNameInLine ("Smychnget = \"no\"", 3), juce::String ());
        }
    }
};

//==============================================================================
struct MonitorLayoutTests final : public juce::UnitTest
{
    MonitorLayoutTests() : juce::UnitTest ("Monitor attributes and grouping", kCategory) {}

    void runTest() override
    {
        using namespace csoundMonitor;

        beginTest ("parseAttributes: only spark; 1.0.5 attributes are ignored");
        {
            expect (parseAttributes ("spark").spark);
            expect (parseAttributes ("db SPARK, int;hidden").spark);
            expect (! parseAttributes ("").spark);
            expect (! parseAttributes ("db int hidden group:X led note").spark);
        }

        beginTest ("splitIndexedName");
        {
            juce::String base; int index = 0;
            expect (splitIndexedName ("Band_7", base, index));  expectEquals (base, juce::String ("Band")); expectEquals (index, 7);
            expect (splitIndexedName ("LFO2", base, index));    expectEquals (base, juce::String ("LFO"));  expectEquals (index, 2);
            expect (splitIndexedName ("Late 12", base, index)); expectEquals (base, juce::String ("Late")); expectEquals (index, 12);
            expect (splitIndexedName ("F1", base, index));      expectEquals (base, juce::String ("F"));
            expect (! splitIndexedName ("Cutoff", base, index));
            expect (! splitIndexedName ("OutRMS_dB", base, index));
            expect (! splitIndexedName ("7", base, index));
            expect (! splitIndexedName ("_1", base, index));
        }

        beginTest ("buildCards: name groups of >= 2, singles stay single, order of first appearance, strings never grouped");
        {
            std::vector<ChannelInfo> ch {
                { "Level", false, false },
                { "Band_2", false, false },
                { "Status", true, false },
                { "Band_1", false, false },
                { "Only1", false, false },
                { "Band_3", false, false },
                { "Name1", true, false }, { "Name2", true, false },
            };
            const auto cards = buildCards (ch);
            expectEquals ((int) cards.size(), 6);
            expectEquals (cards[0].title, juce::String ("Level"));  expect (! cards[0].multi);
            expectEquals (cards[1].title, juce::String ("Band"));   expect (cards[1].multi);
            expectEquals ((int) cards[1].channels.size(), 3);
            expectEquals (cards[1].channels[0], 3);  // Band_1 first, sorted by index
            expectEquals (cards[1].channels[1], 1);
            expectEquals (cards[1].channels[2], 5);
            expectEquals (cards[2].title, juce::String ("Status"));
            expectEquals (cards[3].title, juce::String ("Only1"));  expect (! cards[3].multi);
            expectEquals (cards[4].title, juce::String ("Name1"));
            expectEquals (cards[5].title, juce::String ("Name2"));
        }

        beginTest ("buildCards: audio channels group only with audio channels");
        {
            std::vector<ChannelInfo> ch {
                { "Out_1", false, true },
                { "Out_2", false, true },
                { "Out_3", false, false },
                { "Out_4", false, false },
            };
            const auto cards = buildCards (ch);
            expectEquals ((int) cards.size(), 2);
            expectEquals (cards[0].title, juce::String ("Out"));
            expect (cards[0].multi && cards[0].channels == std::vector<int> { 0, 1 });
            expectEquals (cards[1].title, juce::String ("Out"));
            expect (cards[1].multi && cards[1].channels == std::vector<int> { 2, 3 });
        }

        beginTest ("splitStereoName: L/R pairs by name");
        {
            juce::String base; int side = 0;
            expect (splitStereoName ("Out_L", base, side));  expectEquals (base, juce::String ("Out"));  expectEquals (side, 1);
            expect (splitStereoName ("Out_R", base, side));  expectEquals (side, 2);
            expect (splitStereoName ("Peak-left", base, side)); expectEquals (base, juce::String ("Peak")); expectEquals (side, 1);
            expect (splitStereoName ("Meter R", base, side)); expectEquals (base, juce::String ("Meter")); expectEquals (side, 2);
            expect (splitStereoName ("OutL", base, side));   expectEquals (base, juce::String ("Out"));  expectEquals (side, 1);
            expect (splitStereoName ("MixRight", base, side)); expectEquals (base, juce::String ("Mix")); expectEquals (side, 2);
            expect (! splitStereoName ("Level", base, side), "a lowercase l is part of the word");
            expect (! splitStereoName ("Cutoff", base, side));
            expect (! splitStereoName ("L", base, side), "no base");
            expect (! splitStereoName ("CPU", base, side));
        }

        beginTest ("buildCards: L/R pairs become one card, apart from indexed names");
        {
            std::vector<ChannelInfo> ch {
                { "Out_L", false, true },
                { "Out_R", false, true },
                { "Out1",  false, true },
                { "Out2",  false, true },
                { "PanL",  false, false },   // alone: no R, a single card
            };
            const auto cards = buildCards (ch);
            expectEquals ((int) cards.size(), 3);
            expectEquals (cards[0].title, juce::String ("Out"));
            expect (cards[0].multi && cards[0].channels == std::vector<int> { 0, 1 });
            expectEquals (cards[1].title, juce::String ("Out"));
            expect (cards[1].multi && cards[1].channels == std::vector<int> { 2, 3 });
            expectEquals (cards[2].title, juce::String ("PanL"));
            expect (! cards[2].multi);
        }

        beginTest ("ChangeTracker: stuck after 2 s without a change");
        {
            ChangeTracker t;
            for (int i = 0; i < 39; ++i) t.observe (0.0);
            expect (! t.looksStuck(), "not enough readings yet");
            t.observe (0.0);
            expect (t.looksStuck());
            t.observe (0.5);
            expect (! t.looksStuck(), "one change clears it for good");
            t.observe (0.0);
            expect (! t.looksStuck());
            ChangeTracker k;
            for (int i = 0; i < 50; ++i) k.observe (7.0);
            expect (k.looksStuck(), "any constant value, not only 0");
        }

        beginTest ("chnKDeclaration: range rounded outwards, default inside");
        {
            expectEquals (chnKDeclaration ("Drift", 7.25, 2.0137, 19.8812, false),
                          juce::String ("chn_k \"Drift\", 2, 2, 7.25, 2.013, 19.882"));
            expectEquals (chnKDeclaration ("Pitch", 50.0, 110.0, 880.0, true),
                          juce::String ("chn_k \"Pitch\", 2, 2, 110, 110, 880, 0, 0, 0, 0, \"spark\""), "default clamped into the range");
            expectEquals (chnKDeclaration ("Const", 3.0, 3.0, 3.0, false),
                          juce::String ("chn_k \"Const\", 2, 2, 3, 3, 4"), "an empty range gets a width");
        }

        beginTest ("describeKind: one sentence per kind");
        {
            for (auto k : { Kind::text, Kind::lamp, Kind::audio, Kind::spark, Kind::steps, Kind::bipolar, Kind::range, Kind::adaptive })
                expect (describeKind (k, -1, 1).isNotEmpty());
            expect (describeKind (Kind::range, 0, 1).contains ("0..1"));
            for (auto k : { Kind::text, Kind::lamp, Kind::audio, Kind::spark, Kind::steps, Kind::bipolar, Kind::range, Kind::adaptive })
                expect (shortKind (k, -100.125, 880.5).length() <= 28, "fits the menu header");
        }

        beginTest ("cabbageIdentChannels: only inside <Cabbage>, unique");
        {
            const auto names = cabbageIdentChannels ("<Cabbage>\nrslider channel(\"Fb\"), identChannel(\"FbID\")\n"
                                                    "checkbox channel(\"x\") identChannel( \"xID\" )\nlabel identChannel(\"FbID\")\n</Cabbage>\n"
                                                    "<CsInstruments>\n; identChannel(\"NotInCabbage\")\n</CsInstruments>");
            expectEquals (names.size(), 2);
            expect (names.contains ("FbID") && names.contains ("xID"));
            expect (cabbageIdentChannels ("no cabbage here").isEmpty());
        }

        beginTest ("isCabbageGuiMessage: widget messages vs real text");
        {
            expect (isCabbageGuiMessage ("visible(1)"));
            expect (isCabbageGuiMessage ("visible(1), bounds(12,34,1,50)"));
            expect (isCabbageGuiMessage ("colour( 20, 60, 70)"));
            expect (isCabbageGuiMessage ("tableNumber(1)"));
            expect (isCabbageGuiMessage ("text(C#3) bounds(0,0,10,10)"));
            expect (! isCabbageGuiMessage ("playing"));
            expect (! isCabbageGuiMessage ("bar 3  beat 2  step 05  x"));
            expect (! isCabbageGuiMessage ("C#3"));
            expect (! isCabbageGuiMessage ("f(x) = 2"));
            expect (! isCabbageGuiMessage (""));
        }

        beginTest ("barSpan: unipolar from the left, bipolar from zero");
        {
            auto b = barSpan (0.25, 0.0, 1.0);
            expectWithinAbsoluteError (b.from, 0.0f, 1e-6f); expectWithinAbsoluteError (b.to, 0.25f, 1e-6f); expect (b.zero < 0.0f);
            b = barSpan (0.5, -1.0, 1.0);                       // LFO +0.5
            expectWithinAbsoluteError (b.zero, 0.5f, 1e-6f);
            expectWithinAbsoluteError (b.from, 0.5f, 1e-6f); expectWithinAbsoluteError (b.to, 0.75f, 1e-6f);
            b = barSpan (-1.0, -1.0, 1.0);                      // LFO -1
            expectWithinAbsoluteError (b.from, 0.0f, 1e-6f); expectWithinAbsoluteError (b.to, 0.5f, 1e-6f);
            b = barSpan (0.0, -8.0, 12.0);                      // asymmetric, at zero: empty bar on the zero mark
            expectWithinAbsoluteError (b.zero, 0.4f, 1e-6f);
            expectWithinAbsoluteError (b.from, 0.4f, 1e-6f); expectWithinAbsoluteError (b.to, 0.4f, 1e-6f);
            b = barSpan (5.0, 0.0, 1.0);                        // clamped
            expectWithinAbsoluteError (b.to, 1.0f, 1e-6f);
            b = barSpan (1.0, 1.0, 1.0);                        // no range
            expectWithinAbsoluteError (b.to, 0.0f, 1e-6f);
        }

        beginTest ("LED: automatic 0/1 detection, sticky exclusion");
        {

            BinaryTracker gate;
            for (int i = 0; i < 25; ++i) gate.observe (i % 2);
            expect (gate.looksBinary());
            expect (showsAsLed (false, 0, 0, gate));
            expect (showsAsLed (true, 0, 1, gate), "hints exactly 0..1 still allow it");
            expect (! showsAsLed (true, 0, 7, gate), "a declared range that is not 0..1 says it is not a switch");

            BinaryTracker few;
            for (int i = 0; i < 5; ++i) few.observe (1);
            expect (! few.looksBinary(), "not enough observations yet");

            BinaryTracker zeros;
            for (int i = 0; i < 50; ++i) zeros.observe (0);
            expect (! zeros.looksBinary(), "never on: a counter at 0, not a lamp");

            // a row of lights: each member is 1 only now and then, the card decides
            BinaryTracker off, on;
            for (int i = 0; i < 25; ++i) { off.observe (0); on.observe (i == 3 ? 1 : 0); }
            expect (! off.looksBinary());
            expect (combine (off, on).looksBinary(), "one lit member is enough for the whole card");
            BinaryTracker bad;
            for (int i = 0; i < 25; ++i) bad.observe (i == 3 ? 0.5 : 0);
            expect (! combine (on, bad).looksBinary(), "one non-binary member excludes the whole card");
            BinaryTracker young;
            young.observe (1);
            expect (! combine (on, young).looksBinary(), "the member with fewest readings decides when");

            BinaryTracker counter;
            for (int i = 0; i < 25; ++i) counter.observe (i % 2);
            counter.observe (2);
            for (int i = 0; i < 25; ++i) counter.observe (i % 2);
            expect (! counter.looksBinary(), "one non-binary value excludes it for good");
        }

        beginTest ("formatMonitorValue: up to 3 decimals, audio in dB");
        {
            expectEquals (formatMonitorValue (1.23456), juce::String ("1.235"));
            expectEquals (formatMonitorValue (7.0), juce::String ("7"));
            expectEquals (formatMonitorValue (0.5), juce::String ("0.5"));
            expectEquals (formatMonitorValue (-0.0001), juce::String ("0"));
            expectEquals (formatMonitorValue (1.0, true), juce::String ("0 dB"));
            expectEquals (formatMonitorValue (0.5, true), juce::String ("-6 dB"));
            expectEquals (formatMonitorValue (0.0, true), juce::String ("-60 dB"));
        }

        beginTest ("kindOf: one look per kind");
        {
            expect (kindOf (true,  false, false, false, false, 0, 0)  == Kind::text);
            expect (kindOf (false, true,  true,  false, false, 0, 0)  == Kind::audio,   "audio wins over spark");
            expect (kindOf (false, false, true,  true,  false, 0, 1)  == Kind::lamp,    "a lamp wins over spark");
            expect (kindOf (false, false, true,  false, true,  0, 1)  == Kind::spark);
            expect (kindOf (false, false, false, false, true, -1, 1)  == Kind::bipolar);
            expect (kindOf (false, false, false, false, false, -8, 12) == Kind::bipolar, "seen range across zero");
            expect (kindOf (false, false, false, false, true,  0, 1)  == Kind::range);
            expect (kindOf (false, false, false, false, false, 0, 5)  == Kind::adaptive);
            expect (kindOf (false, false, false, false, true,  0, 7, true) == Kind::steps);
            expect (kindOf (false, false, false, true,  true,  0, 1, true) == Kind::lamp, "0..1 is a lamp, not steps");
        }

        beginTest ("steps: integer range of 3..33 values, only integers seen");
        {
            IntegerTracker ints;
            for (int i = 0; i < 12; ++i) ints.observe (i % 8);
            expect (ints.looksInteger());
            expect (showsAsSteps (true, 0, 7, ints));
            expect (showsAsSteps (true, -2, 2, ints), "negative integer ranges too");
            expect (! showsAsSteps (false, 0, 0, ints), "no declared range: a counter, not steps");
            expect (! showsAsSteps (true, 0, 1, ints), "two values: that is a lamp");
            expect (! showsAsSteps (true, 0, 100, ints), "too many cells");
            expect (! showsAsSteps (true, 0, 7.5, ints), "range ends must be integers");
            IntegerTracker pan;
            for (int i = 0; i < 12; ++i) pan.observe (i == 4 ? 0.3 : 0);
            expect (! showsAsSteps (true, -1, 1, pan), "one fractional value excludes it (a Pan -1..1)");
            IntegerTracker young;
            young.observe (3);
            expect (! young.looksInteger(), "not enough readings yet");
        }

        beginTest ("audio channels: block RMS and meter ballistics");
        {
            const float sine[4] = { 0.0f, 1.0f, 0.0f, -1.0f };
            expectWithinAbsoluteError (blockRms (sine, 4), std::sqrt (0.5), 1e-9);
            const double dc[3] = { 0.5, 0.5, 0.5 };
            expectWithinAbsoluteError (blockRms (dc, 3), 0.5, 1e-12);
            expectWithinAbsoluteError (blockRms<float> (nullptr, 4), 0.0, 1e-12);
            expectWithinAbsoluteError (blockRms (dc, 0), 0.0, 1e-12);

            expectWithinAbsoluteError (meterBallistics (0.8, 0.2), 0.8, 1e-12);          // rises at once
            expectWithinAbsoluteError (meterBallistics (0.0, 1.0), 0.82, 1e-12);         // falls slowly
            expectWithinAbsoluteError (meterBallistics (0.9, 1.0), 0.9, 1e-12);          // never below the new value
        }

        beginTest ("linearToDb");
        {
            expectWithinAbsoluteError (linearToDb (1.0), 0.0, 1e-9);
            expectWithinAbsoluteError (linearToDb (0.5), -6.0206, 1e-3);
            expectWithinAbsoluteError (linearToDb (0.0), -60.0, 1e-9);
            expectWithinAbsoluteError (linearToDb (1e-9), -60.0, 1e-9);
        }
    }
};

//==============================================================================
struct ChannelDiagnosticsTests final : public juce::UnitTest
{
    ChannelDiagnosticsTests() : juce::UnitTest ("Channel diagnostics", kCategory) {}

    void runTest() override
    {
        using namespace csoundChannels;

        beginTest ("classify: parameter > host > read&written > written > orphan");
        {
            expect (classify (true,  false, true,  false) == Origin::parameter);
            expect (classify (true,  true,  true,  true)  == Origin::parameter);
            expect (classify (false, true,  true,  false) == Origin::host);
            expect (classify (false, false, true,  true)  == Origin::readWritten);
            expect (classify (false, false, false, true)  == Origin::written);
            expect (classify (false, false, true,  false) == Origin::orphan);
            expect (classify (false, false, false, false) == Origin::orphan);
        }

        beginTest ("isNeverReadParameter: parameter written by the code and never read (GUI indicator)");
        {
            Info p; p.type = Type::control; p.origin = Origin::parameter;
            p.input = false; p.output = true;  expect (isNeverReadParameter (p));
            p.input = true;  p.output = true;  expect (! isNeverReadParameter (p));
            p.input = true;  p.output = false; expect (! isNeverReadParameter (p));
            p.origin = Origin::written; p.input = false; p.output = true; expect (! isNeverReadParameter (p));
        }

        beginTest ("canMap: control channels that are not parameters/host");
        {
            Info k; k.type = Type::control; k.origin = Origin::orphan;
            expect (canMap (k));
            k.origin = Origin::written;     expect (canMap (k));
            k.origin = Origin::parameter;   expect (! canMap (k));
            k.origin = Origin::host;        expect (! canMap (k));
            Info s; s.type = Type::string; s.origin = Origin::orphan;   expect (! canMap (s));
            Info a; a.type = Type::audio;  a.origin = Origin::orphan;   expect (! canMap (a));
        }

        beginTest ("summarize and sortForDisplay");
        {
            std::vector<Info> infos;
            auto add = [&] (const char* name, Origin o) { Info i; i.name = name; i.origin = o; infos.push_back (i); };
            add ("Zeta", Origin::written);
            add ("beta", Origin::orphan);
            add ("HOST_BPM", Origin::host);
            add ("alpha", Origin::orphan);
            add ("Gain", Origin::parameter);
            add ("fb", Origin::readWritten);

            const auto s = summarize (infos);
            expectEquals (s.total, 6); expectEquals (s.orphans, 2); expectEquals (s.parameters, 1);
            expectEquals (s.host, 1); expectEquals (s.written, 1); expectEquals (s.readWritten, 1);

            sortForDisplay (infos);
            expectEquals (infos[0].name, juce::String ("alpha"));   // orfani prima, per nome senza maiuscole
            expectEquals (infos[1].name, juce::String ("beta"));
            expectEquals (infos[2].name, juce::String ("Gain"));
            expectEquals (infos[3].name, juce::String ("fb"));
            expectEquals (infos[4].name, juce::String ("Zeta"));
            expectEquals (infos[5].name, juce::String ("HOST_BPM"));
        }

        beginTest ("natural sort: Acc2 before Acc10; families of indexed names");
        {
            std::vector<Info> infos;
            for (const char* n : { "Acc10", "Acc2", "Acc1", "Hold1", "Hold2", "Solo", "Acc3", "Hold3", "Pair1", "Pair2" })
            {
                Info i; i.name = n; i.origin = Origin::readWritten; infos.push_back (i);
            }
            sortForDisplay (infos);
            expectEquals (infos[0].name, juce::String ("Acc1"));
            expectEquals (infos[1].name, juce::String ("Acc2"));
            expectEquals (infos[2].name, juce::String ("Acc3"));
            expectEquals (infos[3].name, juce::String ("Acc10"));

            const auto fams = findFamilies (infos);
            expectEquals ((int) fams.size(), 2);                 // Acc (4) e Hold (3); Pair (2) e Solo no
            expectEquals (fams[0].base, juce::String ("Acc"));
            expectEquals ((int) fams[0].channels.size(), 4);
            expectEquals (infos[(size_t) fams[0].channels[3]].name, juce::String ("Acc10"));   // ordinati per indice
            expectEquals (fams[1].base, juce::String ("Hold"));

            // origine diversa = famiglia diversa
            infos[0].origin = Origin::orphan;
            const auto fams2 = findFamilies (infos);
            expectEquals ((int) fams2.size(), 2);                 // Acc2,3,10 (3) + Hold; Acc1 da solo
            expectEquals ((int) fams2[0].channels.size(), 3);
        }
    }
};

#ifndef CSOUND_TESTS_CORE_ONLY
//==============================================================================
// Suite che usano il processor: lavorano sullo stato corrente e lo
// ripristinano alla fine (captureSessionSnapshot / restoreSessionSnapshot).
struct ScopedSessionRestore
{
    explicit ScopedSessionRestore (CsoundAudioProcessor& p) : proc (p), snap (p.captureSessionSnapshot()) { proc.quietImportMessages = true; }
    ~ScopedSessionRestore() { proc.quietImportMessages = false; proc.restoreSessionSnapshot (snap); }
    CsoundAudioProcessor& proc;
    CsoundAudioProcessor::SessionSnapshot snap;
};

static CsoundAudioProcessor* currentProcessor = nullptr;

struct CabbageImportTests final : public juce::UnitTest
{
    CabbageImportTests() : juce::UnitTest ("Cabbage / CsoundQt import", kCategory) {}

    void runTest() override
    {
        if (currentProcessor == nullptr)
            return;

        auto& proc = *currentProcessor;
        ScopedSessionRestore restore (proc);

        beginTest ("Cabbage: slider, checkbox, button, combobox, xypad");
        {
            const juce::String csd =
                "<Cabbage>\n"
                "form size(400,300)\n"
                "rslider bounds(0,0,50,50), channel(\"Cutoff\"), range(50, 1000, 200, 0.5, 1)\n"
                "checkbox bounds(0,0,50,50), channel(\"OnOff\"), value(1)\n"
                "button bounds(0,0,50,50), channel(\"Play\"), value(0), latched(1)\n"
                "combobox bounds(0,0,50,50), channel(\"Wave\"), text(\"Saw\",\"Square\",\"Tri\"), value(2)\n"
                "xypad bounds(0,0,100,100), channel(\"X\",\"Y\"), rangeX(-1,1,0), rangeY(0,10,5)\n"
                "</Cabbage>\n"
                "<CsoundSynthesizer>\n<CsInstruments>\nsr=48000\nksmps=32\nnchnls=2\n0dbfs=1\n"
                "instr 1\nkc chnget \"Cutoff\"\nendin\n</CsInstruments>\n<CsScore>\ni1 0 z\n</CsScore>\n</CsoundSynthesizer>\n";

            expect (proc.loadSessionFromText (csd));

            auto findFloat = [&] (const juce::String& name) -> int
            {
                for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
                    if (proc.getChannelParamSlot (i).channelName == name) return i;
                return -1;
            };
            auto findBool = [&] (const juce::String& name) -> int
            {
                for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
                    if (proc.getBoolParamSlot (i).channelName == name) return i;
                return -1;
            };
            auto findChoice = [&] (const juce::String& name) -> int
            {
                for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
                    if (proc.getChoiceParamSlot (i).channelName == name) return i;
                return -1;
            };

            const int cut = findFloat ("Cutoff");
            expect (cut >= 0, "Cutoff imported");
            if (cut >= 0)
            {
                const auto s = proc.getChannelParamSlot (cut);
                expectWithinAbsoluteError (s.minValue, 50.0f, 1e-4f);
                expectWithinAbsoluteError (s.maxValue, 1000.0f, 1e-4f);
                expectWithinAbsoluteError (s.defaultValue, 200.0f, 1e-4f);
                expectWithinAbsoluteError (s.skew, 0.5f, 1e-4f);
                expectWithinAbsoluteError (s.increment, 1.0f, 1e-4f);
            }

            const int on = findBool ("OnOff");
            expect (on >= 0, "checkbox imported");
            if (on >= 0) expect (proc.getBoolParamSlot (on).defaultValue, "checkbox value(1) -> ON");

            const int play = findBool ("Play");
            expect (play >= 0, "button imported as Bool (was skipped before)");
            if (play >= 0) expect (! proc.getBoolParamSlot (play).defaultValue);
            if (play >= 0) expect (! proc.getBoolParamSlot (play).momentary, "latched(1): a toggle");

            const int wave = findChoice ("Wave");
            expect (wave >= 0, "combobox imported");
            if (wave >= 0)
            {
                const auto s = proc.getChoiceParamSlot (wave);
                expectEquals (s.optionLabels.size(), 3);
                expectEquals (s.defaultIndex, 1, "combobox value(2) is 1-based -> index 1 (Square)");
            }

            expect (findFloat ("X") >= 0 && findFloat ("Y") >= 0, "xypad -> two Float slots");
            if (findFloat ("Y") >= 0)
                expectWithinAbsoluteError (proc.getChannelParamSlot (findFloat ("Y")).defaultValue, 5.0f, 1e-4f);
        }

        beginTest ("Cabbage: on overflow, widgets never read by the code (indicators) are dropped first");
        {
            juce::String csd = "<Cabbage>\n";
            // 10 LED first in the file (never read), then 60 switches (read): 70 > 64 Bool slots
            for (int i = 1; i <= 10; ++i) csd << "checkbox bounds(0,0,10,10), channel(\"Led" << i << "\"), value(0)\n";
            for (int i = 1; i <= 60; ++i) csd << "checkbox bounds(0,0,10,10), channel(\"Sw" << i << "\"), value(0)\n";
            csd << "</Cabbage>\n<CsoundSynthesizer><CsInstruments>\ninstr 1\n";
            for (int i = 1; i <= 60; ++i) csd << "k" << i << " chnget \"Sw" << i << "\"\n";
            for (int i = 1; i <= 10; ++i) csd << "chnset k1, \"Led" << i << "\"\n";
            csd << "endin\n</CsInstruments></CsoundSynthesizer>\n";
            expect (proc.loadSessionFromText (csd));

            int sw = 0, led = 0;
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
            {
                const auto n = proc.getBoolParamSlot (i).channelName;
                if (n.startsWith ("Sw")) ++sw;
                if (n.startsWith ("Led")) ++led;
            }
            expectEquals (sw, 60, "all read switches mapped");
            expectEquals (led, 4, "only as many LEDs as there is room for (64 - 60)");
        }

        beginTest ("Cabbage: a channel used by two widgets gives ONE parameter");
        {
            const juce::String csd =
                "<Cabbage>\n"
                "rslider bounds(0,0,50,50), channel(\"Freq\"), range(0,1,0.5)\n"
                "xypad bounds(0,0,100,100), channel(\"Freq\",\"Dev\"), rangeX(0,1,0.5), rangeY(0,1,0.5)\n"
                "</Cabbage>\n<CsoundSynthesizer><CsInstruments>\nkf chnget \"Freq\"\n</CsInstruments></CsoundSynthesizer>\n";
            expect (proc.loadSessionFromText (csd));
            int count = 0;
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
                if (proc.getChannelParamSlot (i).channelName == "Freq") ++count;
            expectEquals (count, 1);
        }

        beginTest ("Button: MODE, INIT, EVENT ON/OFF survive save and reload");
        {
            const juce::String csd =
                "<CsoundSynthesizer><CsInstruments>\ninstr 1\nkp chnget \"Pad\"\nendin\n</CsInstruments></CsoundSynthesizer>\n"
                "<apeCsoundParams>\n<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<APE_CSOUND_STATE>\n"
                "<CHANNEL_PARAM_SLOTS/><INT_PARAM_SLOTS/>\n<BOOL_PARAM_SLOTS>\n"
                "<SLOT index=\"0\" channel=\"Pad\" default=\"1\" momentary=\"1\" event=\"i 14.1 0 -1\" eventOff=\"i -14.1 0 0\"/>\n"
                "<SLOT index=\"1\" channel=\"Drone\" default=\"1\"/>\n"
                "</BOOL_PARAM_SLOTS>\n<CHOICE_PARAM_SLOTS/>\n</APE_CSOUND_STATE>\n</apeCsoundParams>\n";

            auto check = [this, &proc] (const char* when)
            {
                const auto pad = proc.getBoolParamSlot (0);
                expectEquals (pad.channelName, juce::String ("Pad"), when);
                expect (pad.momentary, when);
                expect (pad.defaultValue, juce::String (when) + ": INIT kept while Momentary");
                expect (! pad.startsOn(), juce::String (when) + ": a Momentary starts off anyway");
                expectEquals (pad.eventLine, juce::String ("i 14.1 0 -1"), when);
                expectEquals (pad.eventOffLine, juce::String ("i -14.1 0 0"), when);
                const auto drone = proc.getBoolParamSlot (1);
                expect (! drone.momentary && drone.startsOn() && drone.eventLine.isEmpty() && drone.eventOffLine.isEmpty(), when);
            };

            expect (proc.loadSessionFromText (csd));
            check ("loaded");
            const auto saved = proc.buildSessionText();
            expect (saved.contains ("eventOff=\"i -14.1 0 0\""));
            expect (proc.loadSessionFromText (saved));
            check ("saved and reloaded");
        }

        beginTest ("Cabbage: button latched(0) -> momentary");
        {
            const juce::String csd =
                "<Cabbage>\nbutton bounds(0,0,50,50), channel(\"Hit\"), value(1), latched(0)\n</Cabbage>\n"
                "<CsoundSynthesizer><CsInstruments>\nkh chnget \"Hit\"\n</CsInstruments></CsoundSynthesizer>\n";
            expect (proc.loadSessionFromText (csd));
            bool found = false;
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
                if (proc.getBoolParamSlot (i).channelName == "Hit")
                {
                    found = true;
                    expect (proc.getBoolParamSlot (i).momentary);
                    expect (! proc.getBoolParamSlot (i).startsOn(), "a momentary button starts off");
                }
            expect (found);
        }

        beginTest ("CsoundQt: value buttons (latch / momentary) and event buttons");
        {
            const juce::String csd =
                "<CsoundSynthesizer><CsInstruments>\ninstr 1\nendin\n</CsInstruments></CsoundSynthesizer>\n"
                "<bsbPanel>\n"
                "<bsbObject type=\"BSBButton\"><objectName>hold</objectName><type>value</type><pressedValue>1</pressedValue><latch>false</latch><latched>false</latched></bsbObject>\n"
                "<bsbObject type=\"BSBButton\"><objectName>sw</objectName><type>value</type><pressedValue>1</pressedValue><latch>true</latch><latched>false</latched></bsbObject>\n"
                "<bsbObject type=\"BSBButton\"><objectName>on</objectName><type>value</type><pressedValue>1</pressedValue><latch>true</latch><latched>true</latched></bsbObject>\n"
                "<bsbObject type=\"BSBButton\"><objectName></objectName><type>event</type><text></text><eventLine>i1 0 10</eventLine><latch>false</latch></bsbObject>\n"
                "<bsbObject type=\"BSBButton\"><objectName>b3682</objectName><type>event</type><text>Held note</text><eventLine>i 2.1 0 -1</eventLine><latch>false</latch></bsbObject>\n"
                "<bsbObject type=\"BSBButton\"><objectName>b3683</objectName><type>event</type><text>Drone</text><eventLine>i \"Drone\" 0 -1</eventLine><latch>true</latch></bsbObject>\n"
                "</bsbPanel>\n";
            expect (proc.loadSessionFromText (csd));
            int hold = -1, sw = -1, on = -1, ev = -1, held = -1, drone = -1;
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
            {
                const auto n = proc.getBoolParamSlot (i).channelName;
                if (n == "hold") hold = i;
                if (n == "sw") sw = i;
                if (n == "on") on = i;
                if (n == "Event1") ev = i;
                if (n == "Held_note") held = i;
                if (n == "Drone") drone = i;
            }
            expect (on >= 0 && proc.getBoolParamSlot (on).startsOn(), "latch + latched -> Toggle starting on");
            expect (held >= 0, "event button named after its text, not b3682");
            if (held >= 0)
            {
                const auto s = proc.getBoolParamSlot (held);
                expect (s.momentary);
                expectEquals (s.eventLine, juce::String ("i 2.1 0 -1"));
                expectEquals (s.eventOffLine, juce::String ("i -2.1 0 -1"), "held note: OFF on release, as CsoundQt");
            }
            expect (drone >= 0, "latched event button");
            if (drone >= 0)
            {
                const auto s = proc.getBoolParamSlot (drone);
                expect (! s.momentary, "latch -> Toggle");
                expectEquals (s.eventOffLine, juce::String ("i \"-Drone\" 0 -1"), "named instrument turned off");
            }
            expect (hold >= 0 && proc.getBoolParamSlot (hold).momentary, "latch off -> momentary");
            expect (sw >= 0 && ! proc.getBoolParamSlot (sw).momentary && ! proc.getBoolParamSlot (sw).defaultValue,
                    "latch on -> toggle, starting off");
            expect (ev >= 0 && proc.getBoolParamSlot (ev).momentary, "event button -> momentary Event1");
            if (ev >= 0) expectEquals (proc.getBoolParamSlot (ev).eventLine, juce::String ("i1 0 10"), "its score line as EVENT");
            expect (! proc.getCsdTextWithSurroundings().contains ("apeEventButtons"), "the code is not changed");
        }

        beginTest ("CsoundQt: knob, checkbox, dropdown, controller");
        {
            const juce::String csd =
                "<CsoundSynthesizer><CsInstruments>\nkg chnget \"gain\"\n</CsInstruments></CsoundSynthesizer>\n"
                "<bsbPanel>\n"
                "<bsbObject type=\"BSBKnob\"><objectName>gain</objectName><minimum>0</minimum><maximum>2</maximum><value>1.5</value><resolution>0.01</resolution></bsbObject>\n"
                "<bsbObject type=\"BSBCheckBox\"><objectName>mute</objectName><selected>true</selected><pressedValue>1</pressedValue></bsbObject>\n"
                "<bsbObject type=\"BSBDropdown\"><objectName>mode</objectName><bsbDropdownItemList><bsbDropdownItem><name>A</name></bsbDropdownItem><bsbDropdownItem><name>B</name></bsbDropdownItem></bsbDropdownItemList><selectedIndex>1</selectedIndex></bsbObject>\n"
                "<bsbObject type=\"BSBController\"><objectName>cx</objectName><objectName2>cy</objectName2><xMin>0</xMin><xMax>1</xMax><xValue>0.25</xValue><yMin>0</yMin><yMax>1</yMax><yValue>0.75</yValue></bsbObject>\n"
                "</bsbPanel>\n";
            expect (proc.loadSessionFromText (csd));

            bool gain = false, cx = false, cy = false, mute = false, mode = false;
            for (int i = 0; i < CsoundAudioProcessor::numChannelParams; ++i)
            {
                const auto s = proc.getChannelParamSlot (i);
                if (s.channelName == "gain") { gain = true; expectWithinAbsoluteError (s.defaultValue, 1.5f, 1e-4f); expectWithinAbsoluteError (s.maxValue, 2.0f, 1e-4f); }
                if (s.channelName == "cx")   { cx = true;   expectWithinAbsoluteError (s.defaultValue, 0.25f, 1e-4f); }
                if (s.channelName == "cy")   { cy = true;   expectWithinAbsoluteError (s.defaultValue, 0.75f, 1e-4f); }
            }
            for (int i = 0; i < CsoundAudioProcessor::numBoolParams; ++i)
                if (proc.getBoolParamSlot (i).channelName == "mute") { mute = true; expect (proc.getBoolParamSlot (i).defaultValue); }
            for (int i = 0; i < CsoundAudioProcessor::numChoiceParams; ++i)
                if (proc.getChoiceParamSlot (i).channelName == "mode") { mode = true; expectEquals (proc.getChoiceParamSlot (i).defaultIndex, 1); expectEquals (proc.getChoiceParamSlot (i).optionLabels.size(), 2); }
            expect (gain && cx && cy && mute && mode, "all CsoundQt widgets imported");
        }
    }
};

struct SnapshotTreeTests final : public juce::UnitTest
{
    SnapshotTreeTests() : juce::UnitTest ("Parameter snapshots", kCategory) {}

    void runTest() override
    {
        if (currentProcessor == nullptr)
            return;

        auto& proc = *currentProcessor;
        ScopedSessionRestore restore (proc);

        // Stato di partenza noto: un solo Float "snapTest" 0..10, nessuno snapshot.
        proc.loadSessionFromText ("<CsoundSynthesizer><CsInstruments>\nk1 chnget \"snapTest\"\n</CsInstruments></CsoundSynthesizer>\n");
        proc.setParameterSnapshotsTree (juce::ValueTree ("SNAPSHOTS"));

        CsoundAudioProcessor::ChannelParamSlot slot;
        slot.channelName = "snapTest";
        slot.minValue = 0.0f; slot.maxValue = 10.0f; slot.defaultValue = 0.0f;
        proc.setChannelParamSlot (0, slot);
        auto* param = proc.apvts.getParameter (CsoundAudioProcessor::getChannelParamID (0));
        expect (param != nullptr);
        if (param == nullptr) return;

        auto setReal = [&] (double real) { param->setValueNotifyingHost (CsoundAudioProcessor::normalizeChannelParam (slot, real)); };
        auto getReal = [&] { return CsoundAudioProcessor::denormalizeChannelParam (slot, param->getValue()); };

        beginTest ("add / unique names / rename");
        {
            expectEquals (proc.getNumParameterSnapshots(), 0);
            setReal (2.0);
            const int a = proc.addParameterSnapshot ("");
            expectEquals (a, 0);
            expectEquals (proc.getParameterSnapshotName (0), juce::String ("Snapshot 1"));
            setReal (7.0);
            const int b = proc.addParameterSnapshot ("  Loud  ");
            expectEquals (b, 1);
            expectEquals (proc.getParameterSnapshotName (1), juce::String ("Loud"));
            expectEquals (proc.makeUniqueSnapshotName(), juce::String ("Snapshot 2"));
            proc.renameParameterSnapshot (0, "Quiet");
            expectEquals (proc.getParameterSnapshotName (0), juce::String ("Quiet"));
            proc.renameParameterSnapshot (0, "   ");
            expectEquals (proc.getParameterSnapshotName (0), juce::String ("Quiet"), "blank rename ignored");
        }

        beginTest ("recall restores the value, overwrite replaces it, remove shifts indices");
        {
            setReal (5.0);
            expect (proc.recallParameterSnapshot (0));
            expectWithinAbsoluteError (getReal(), 2.0, 1e-3);
            expect (proc.recallParameterSnapshot (1));
            expectWithinAbsoluteError (getReal(), 7.0, 1e-3);

            setReal (9.0);
            proc.overwriteParameterSnapshot (0);
            expectEquals (proc.getParameterSnapshotName (0), juce::String ("Quiet"), "overwrite keeps the name");
            setReal (1.0);
            proc.recallParameterSnapshot (0);
            expectWithinAbsoluteError (getReal(), 9.0, 1e-3);

            proc.removeParameterSnapshot (0);
            expectEquals (proc.getNumParameterSnapshots(), 1);
            expectEquals (proc.getParameterSnapshotName (0), juce::String ("Loud"));
            expect (! proc.recallParameterSnapshot (5), "out of range recall fails");
        }

        beginTest ("tree round trip (undo path) and a snapshot of a missing channel is harmless");
        {
            const auto tree = proc.getParameterSnapshotsTree();
            expectEquals (tree.getNumChildren(), 1);
            proc.setParameterSnapshotsTree (juce::ValueTree ("SNAPSHOTS"));
            expectEquals (proc.getNumParameterSnapshots(), 0);
            proc.setParameterSnapshotsTree (tree);
            expectEquals (proc.getNumParameterSnapshots(), 1);

            // il canale sparisce: il recall non deve toccare nulla ne' fallire rumorosamente
            CsoundAudioProcessor::ChannelParamSlot other = slot;
            other.channelName = "renamed";
            proc.setChannelParamSlot (0, other);
            setReal (3.0);
            proc.recallParameterSnapshot (0);
            expectWithinAbsoluteError (CsoundAudioProcessor::denormalizeChannelParam (other, param->getValue()), 3.0, 1e-3);
        }
    }
};
#endif

//==============================================================================
Summary runAll (CsoundAudioProcessor* processor, const std::function<void (const juce::String&)>& log)
{
    static ParameterGroupsTests groupsTests;
    static CabbageHelpersTests  cabbageTests;
    static LayoutHelpersTests   layoutTests;
    static SyntaxHelpersTests   syntaxTests;
    static MonitorLayoutTests   monitorTests;
    static ChannelDiagnosticsTests channelTests;
   #ifndef CSOUND_TESTS_CORE_ONLY
    static CabbageImportTests   importTests;
    static SnapshotTreeTests    snapshotTests;
    currentProcessor = processor;
   #else
    juce::ignoreUnused (processor);
   #endif

    struct Runner final : public juce::UnitTestRunner
    {
        explicit Runner (const std::function<void (const juce::String&)>& l) : logFn (l) { setAssertOnFailure (false); setPassesAreLogged (false); }
        void logMessage (const juce::String& m) override { logFn (m); }
        const std::function<void (const juce::String&)>& logFn;
    };

    Runner runner (log);
    runner.runTestsInCategory (kCategory);

    Summary summary;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        const auto* r = runner.getResult (i);
        summary.passes   += r->passes;
        summary.failures += r->failures;
        for (const auto& m : r->messages)
            summary.failureMessages.add (r->unitTestName + " / " + r->subcategoryName + ": " + m);
    }

    log ("--- Self-tests: " + juce::String (summary.passes) + " passed, " + juce::String (summary.failures) + " failed ---");

   #ifndef CSOUND_TESTS_CORE_ONLY
    currentProcessor = nullptr;
   #endif
    return summary;
}

} // namespace csoundTests
