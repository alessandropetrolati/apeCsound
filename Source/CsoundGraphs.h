#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include "csound.h"

/**
    Grafici di Csound (ftable, display, dispfft...) mostrati nel pannello
    "Graphs" del menu principale.

    Csound disegna i grafici tramite i callback MakeGraph/DrawGraph/KillGraph
    dell'host (vedi csound_graph_display.h): il processore li installa
    (PluginProcessor.cpp, compileAndStart) e li inoltra a questo store, che
    ne tiene una copia ridotta (al massimo maxColumns colonne min/max) in
    slot preallocati: nessuna allocazione nel thread audio, dove arrivano le
    chiamate di display/dispfft a k-rate.

    Un grafico e' identificato dalla didascalia di Csound ("ftable 1:",
    "instr 1, signal asig:"...): la stessa ftable rigenerata, o lo stesso
    display in una nuova nota, aggiorna lo stesso riquadro.
*/
class CsoundGraphStore
{
public:
    static constexpr int maxGraphs   = 64;
    static constexpr int maxColumns  = 512;
    static constexpr int captionSize = 64;

    struct Graph
    {
        char  caption[captionSize] {};
        float lo[maxColumns] {};
        float hi[maxColumns] {};
        int   numColumns = 0;   // colonne valide in lo/hi
        int   numPoints  = 0;   // punti originali del grafico Csound
        float scale      = 1.0f; // massimo assoluto (asse Y)
        int   polarity   = 0;   // NOPOL/NEGPOL/POSPOL/BIPOL di cwindow.h
        bool  isFtable   = false;
        uint32_t version = 0;   // incrementato a ogni disegno
    };

    /** Nuovo motore (compileAndStart): svuota tutti i grafici. */
    void clear();

    /** MakeGraph: restituisce l'indice dello slot + 1 (0 = pieno). */
    int open (const char* caption);

    /** DrawGraph. Le ftable sono sempre registrate (create una volta sola
        all'init); gli altri grafici solo se il pannello e' aperto. */
    void draw (int slotPlusOne, const cs_float* data, int numPoints, double absScale, int polarity);

    /** Pannello aperto/chiuso: decide se registrare display/dispfft. */
    void setViewerActive (bool active) noexcept { viewerActive.store (active); }

    /** Contatore delle modifiche: il pannello ricopia solo se cambia. */
    uint32_t getChangeCount() const noexcept { return changeCount.load(); }

    /** Copia dei grafici presenti (message thread). */
    void snapshot (std::vector<Graph>& out);

private:
    juce::SpinLock lock;
    Graph graphs[maxGraphs];
    int numGraphs = 0;
    std::atomic<uint32_t> changeCount { 0 };
    std::atomic<bool> viewerActive { false };
};

/**
    Pannello "Graphs": pannello agganciato come Parametri e Console (voce
    "Show Graphs" del menu principale), in basso accanto alla consolle.
    Griglia scorrevole di riquadri, uno per grafico. display/dispfft sono
    registrati solo mentre il pannello e' visibile.
*/
class CsoundGraphsPanel final : public juce::Component,
                                private juce::Timer
{
public:
    explicit CsoundGraphsPanel (CsoundGraphStore& store);
    ~CsoundGraphsPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    void refresh (bool force);

    struct Grid final : public juce::Component
    {
        void paint (juce::Graphics& g) override;
        int heightForWidth (int width) const;
        std::vector<CsoundGraphStore::Graph> graphs;
        int tileHeight = 150;
        static constexpr int minTileWidth = 260;
        static constexpr int gap = 8;
    };

    static constexpr int headerHeight = 22;

    CsoundGraphStore& store;
    juce::Viewport viewport;
    Grid grid;
    uint32_t lastChange = 0xffffffff;
};
