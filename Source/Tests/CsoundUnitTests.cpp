#include "CsoundUnitTests.h"

#include "../CsoundParameterGroups.h"
#include "../CsoundCabbageHelpers.h"
#include "../CsoundLayoutHelpers.h"
#include "../CsoundSyntaxHelpers.h"

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
