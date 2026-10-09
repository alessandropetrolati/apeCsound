#include "CsoundActionSheet.h"
#include <cmath>

namespace
{
    // Stessa palette usata da CsoundParameterPanelLookAndFeel (non esportata
    // da CsoundParameterEditor.cpp, quindi ridefinita qui) - per coerenza
    // visiva fra il resto del plugin e questo foglio.
    const juce::Colour kSheetBg      { 0xff141d24 };
    const juce::Colour kRowPressedBg { 0xff1c2730 };
    const juce::Colour kSeparator    { 0xff2a3540 };
    const juce::Colour kText         { 0xffe8eef1 };
    const juce::Colour kTextDisabled { 0xff5b6b74 };
    const juce::Colour kTextMuted    { 0xff8a9aa5 };
    const juce::Colour kAccent       { 0xff4aa3b8 };

    // Icone disegnate a mano con la stessa tecnica usata altrove in questo
    // codebase per le icone "a linea" (es. makeAudioEngineIconPath in
    // CsoundLookAndFeel.cpp): un Path aperto convertito in un ribbon
    // riempibile via PathStrokeType::createStrokedPath, cosi' il tratto ha
    // estremita'/giunti arrotondati invece di dover gestire uno stroke a
    // runtime nel paint().
    juce::Path makeCheckmarkPath()
    {
        juce::Path p;
        p.startNewSubPath (2.0f, 12.5f);
        p.lineTo (9.0f, 19.5f);
        p.lineTo (22.0f, 4.5f);

        juce::Path filled;
        juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath (filled, p);
        return filled;
    }

    juce::Path makeChevronRightPath()
    {
        juce::Path p;
        p.startNewSubPath (7.0f, 4.0f);
        p.lineTo (16.0f, 12.0f);
        p.lineTo (7.0f, 20.0f);

        juce::Path filled;
        juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath (filled, p);
        return filled;
    }

    juce::Path makeChevronLeftPath()
    {
        juce::Path p;
        p.startNewSubPath (17.0f, 4.0f);
        p.lineTo (8.0f, 12.0f);
        p.lineTo (17.0f, 20.0f);

        juce::Path filled;
        juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath (filled, p);
        return filled;
    }

    // Icone delle righe (richiesta esplicita: "inserisci le icone sulla sx
    // di ciascuna voce dei due menu") - tutte disegnate a mano in un
    // riquadro 24x24, stessa tecnica di makeCheckmarkPath/makeChevron*Path
    // sopra (Path aperto -> PathStrokeType::createStrokedPath per le forme
    // "a linea"; addRoundedRectangle/addEllipse direttamente per le forme
    // "piene"/outline, che Path::fillPath gestisce bene anche se composte
    // da piu' sotto-path non sovrapposti). Nessun font-glyph: coerente con
    // checkmark/chevron, che sono anch'essi disegnati cosi'.
    juce::Path strokeOpenPath (const juce::Path& open, float thickness = 1.9f)
    {
        juce::Path filled;
        juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath (filled, open);
        return filled;
    }

    // Freccia dritta (stelo + punta a "V") - usata per Undo/Redo invece di
    // una curva: piu' semplice da disegnare a mano restando comunque
    // immediatamente leggibile come "indietro"/"avanti" nella cronologia.
    juce::Path makeArrowPath (bool pointingLeft)
    {
        juce::Path p;
        const float stemFrom  = pointingLeft ? 19.0f : 5.0f;
        const float stemTo    = pointingLeft ? 5.0f  : 19.0f;
        const float headTipX  = pointingLeft ? 5.0f  : 19.0f;
        const float headBackX = pointingLeft ? 11.0f : 13.0f;

        p.startNewSubPath (stemFrom, 12.0f);
        p.lineTo (stemTo, 12.0f);

        p.startNewSubPath (headBackX, 6.0f);
        p.lineTo (headTipX, 12.0f);
        p.lineTo (headBackX, 18.0f);

        return strokeOpenPath (p, 2.2f);
    }

    // Freccia verso una "vaschetta" (tray) orizzontale in basso - glifo
    // standard upload (punta in alto, usata per "Load") / download (punta
    // in basso, usata per "Save").
    juce::Path makeTrayArrowPath (bool pointingUp)
    {
        juce::Path p;
        const float stemFrom = pointingUp ? 16.0f : 4.0f;
        const float stemTo   = pointingUp ? 4.0f  : 16.0f;
        const float headY    = pointingUp ? 4.0f  : 16.0f;
        const float sideY    = pointingUp ? 9.0f  : 11.0f;

        p.startNewSubPath (12.0f, stemFrom);
        p.lineTo (12.0f, stemTo);

        p.startNewSubPath (7.0f, sideY);
        p.lineTo (12.0f, headY);
        p.lineTo (17.0f, sideY);

        p.startNewSubPath (4.0f, 19.0f);
        p.lineTo (4.0f, 21.5f);
        p.lineTo (20.0f, 21.5f);
        p.lineTo (20.0f, 19.0f);

        return strokeOpenPath (p, 1.9f);
    }

    // Pannello laterale (Show Parameters): rettangolo + divisorio verticale
    // vicino al bordo sinistro, come una sidebar.
    juce::Path makeSidebarPanelPath()
    {
        juce::Path outline;
        outline.addRoundedRectangle (3.0f, 4.0f, 18.0f, 16.0f, 2.5f);
        auto filled = strokeOpenPath (outline, 1.7f);

        juce::Path divider;
        divider.startNewSubPath (9.5f, 4.6f);
        divider.lineTo (9.5f, 19.4f);
        filled.addPath (strokeOpenPath (divider, 1.7f));
        return filled;
    }

    // Console/terminale (Show Console): stesso rettangolo, con un prompt
    // ">" e una sottolineatura al posto del divisorio.
    juce::Path makeConsolePath()
    {
        juce::Path outline;
        outline.addRoundedRectangle (3.0f, 4.0f, 18.0f, 16.0f, 2.5f);
        auto filled = strokeOpenPath (outline, 1.7f);

        juce::Path prompt;
        prompt.startNewSubPath (7.0f, 9.5f);
        prompt.lineTo (10.0f, 12.0f);
        prompt.lineTo (7.0f, 14.5f);
        prompt.startNewSubPath (11.5f, 14.5f);
        prompt.lineTo (16.0f, 14.5f);
        filled.addPath (strokeOpenPath (prompt, 1.7f));
        return filled;
    }

    // Slider (Add Slider Float/Int): linea orizzontale + manopola; la
    // variante Int aggiunge piccole tacche per suggerire passi discreti.
    juce::Path makeSliderPath (bool withTicks)
    {
        juce::Path line;
        line.startNewSubPath (4.0f, 13.0f);
        line.lineTo (20.0f, 13.0f);
        auto filled = strokeOpenPath (line, 2.0f);

        if (withTicks)
        {
            juce::Path ticks;
            for (float x : { 8.0f, 12.0f, 16.0f })
            {
                ticks.startNewSubPath (x, 8.5f);
                ticks.lineTo (x, 10.3f);
            }
            filled.addPath (strokeOpenPath (ticks, 1.6f));
        }

        juce::Path knob;
        knob.addEllipse (11.5f, 9.5f, 7.0f, 7.0f);
        filled.addPath (knob);
        return filled;
    }

    // Toggle (Add Toggle): capsula "pillola" con la manopola a destra
    // (posizione ON), come un interruttore iOS.
    juce::Path makeTogglePath()
    {
        juce::Path pill;
        pill.addRoundedRectangle (3.0f, 7.5f, 18.0f, 9.0f, 4.5f);
        auto filled = strokeOpenPath (pill, 1.7f);

        juce::Path knob;
        knob.addEllipse (13.3f, 8.8f, 6.4f, 6.4f);
        filled.addPath (knob);
        return filled;
    }

    // Menu/Combo (Add Menu): riquadro "chip" con una chevron verso il
    // basso, come una tendina a scelta multipla.
    juce::Path makeComboMenuPath()
    {
        juce::Path box;
        box.addRoundedRectangle (3.0f, 7.0f, 14.0f, 10.0f, 2.5f);
        auto filled = strokeOpenPath (box, 1.7f);

        juce::Path chevron;
        chevron.startNewSubPath (16.5f, 10.3f);
        chevron.lineTo (19.3f, 13.1f);
        chevron.lineTo (22.1f, 10.3f);
        filled.addPath (strokeOpenPath (chevron, 1.8f));
        return filled;
    }

    // Occhio aperto/chiuso (Open/Close Config): ellisse + pupilla piena
    // per l'aperto, linea curva "a palpebra" + barra diagonale per il
    // chiuso - stesso lessico visivo usato ovunque per show/hide.
    // IDENTICHE a makeEyeIconPath()/makeEyeOffIconPath() in
    // CsoundParameterEditor.cpp (stesso Material Design "visibility"/
    // "visibility_off", viewBox 24x24) - richiesto esplicitamente: "usa le
    // stesse icone per Open Config e Close Config, unica sia per il menu
    // sia per il bottone toggle sulla view" - prima questo foglio disegnava
    // un occhio diverso (ellisse+pupilla a mano), visivamente incoerente
    // col bottone "occhio" di ogni riga del pannello Parametri. Duplicate
    // qui (non condivise via header) solo perche' sono funzioni locali al
    // namespace anonimo di ciascun file, stesso schema delle altre icone
    // "a mano" in questo file - il contenuto del Path e' pero' letteralmente
    // lo stesso, quindi il disegno risultante e' identico pixel per pixel.
    juce::Path makeEyePath (bool open)
    {
        if (open)
            return juce::Drawable::parseSVGPath (
                "M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zm0 "
                "12.5c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 "
                "3-3-1.34-3-3-3z");

        return juce::Drawable::parseSVGPath (
            "M12 7c2.76 0 5 2.24 5 5 0 .65-.13 1.26-.36 1.83l2.92 2.92c1.51-1.26 2.7-2.89 3.43-4.75-1.73-4.39-6-7.5-"
            "11-7.5-1.4 0-2.74.25-3.98.7l2.16 2.16C10.74 7.13 11.35 7 12 7zM2 4.27l2.28 2.28.46.46C3.08 8.3 1.78 "
            "10.02 1 12c1.73 4.39 6 7.5 11 7.5 1.55 0 3.03-.3 4.38-.84l.42.42L19.73 22 21 20.73 3.27 3 2 4.27zM7.53 "
            "9.8l1.55 1.55c-.05.21-.08.43-.08.65 0 1.66 1.34 3 3 3 .22 0 .44-.03.65-.08l1.55 1.55c-.67.33-1.41.53-"
            "2.2.53-2.76 0-5-2.24-5-5 0-.79.2-1.53.53-2.2zm4.31-.78l3.15 3.15.02-.16c0-1.66-1.34-3-3-3l-.17.01z");
    }

    // Cestino (Remove Parameters): coperchio + corpo trapezoidale + due
    // costole verticali, il classico glifo "trash".
    juce::Path makeTrashPath()
    {
        juce::Path p;
        p.startNewSubPath (10.0f, 4.3f);
        p.lineTo (14.0f, 4.3f);
        p.lineTo (14.0f, 6.3f);
        p.startNewSubPath (6.0f, 6.3f);
        p.lineTo (18.0f, 6.3f);
        p.startNewSubPath (7.2f, 6.3f);
        p.lineTo (8.2f, 20.0f);
        p.lineTo (15.8f, 20.0f);
        p.lineTo (16.8f, 6.3f);
        p.startNewSubPath (10.3f, 9.0f);
        p.lineTo (10.7f, 17.3f);
        p.startNewSubPath (13.7f, 9.0f);
        p.lineTo (13.3f, 17.3f);
        return strokeOpenPath (p, 1.7f);
    }

    // Ripristina ai valori di default (Reset to INIT Values): arco quasi
    // completo con una piccola freccia che lo richiude, come il classico
    // glifo "restore" - distinto da Undo/Redo (freccia dritta) perche'
    // questa azione non e' annullabile (vedi resetAllParametersToInit()).
    juce::Path makeResetDefaultPath()
    {
        juce::Path arc;
        arc.addCentredArc (12.0f, 13.0f, 7.5f, 7.5f, 0.0f,
                             juce::degreesToRadians (50.0f), juce::degreesToRadians (320.0f), true);
        auto filled = strokeOpenPath (arc, 2.1f);

        juce::Path head;
        head.addTriangle (17.3f, 8.1f, 20.6f, 9.5f, 17.9f, 12.5f);
        filled.addPath (head);
        return filled;
    }

    // Pagina "nuova" (Initialize Session): rettangolo con l'angolo in alto
    // a destra ripiegato ("dog-ear", il classico glifo "nuovo documento")
    // + due righe corte a suggerire del testo - distinto sia dal cestino
    // (distruttivo/rimozione) sia dall'arco di resetDefault (ripristino di
    // un VALORE), perche' qui l'intera sessione (codice + mapping) viene
    // sostituita con quella di partenza, non solo azzerata.
    juce::Path makeNewDocumentPath()
    {
        juce::Path page;
        page.startNewSubPath (6.0f, 2.5f);
        page.lineTo (15.0f, 2.5f);
        page.lineTo (19.5f, 7.0f);
        page.lineTo (19.5f, 21.5f);
        page.lineTo (6.0f, 21.5f);
        page.closeSubPath();

        juce::Path foldedCorner;
        foldedCorner.startNewSubPath (15.0f, 2.5f);
        foldedCorner.lineTo (15.0f, 7.0f);
        foldedCorner.lineTo (19.5f, 7.0f);

        auto filled = strokeOpenPath (page, 1.7f);
        filled.addPath (strokeOpenPath (foldedCorner, 1.7f));

        juce::Path lines;
        lines.startNewSubPath (9.0f, 12.5f);
        lines.lineTo (16.5f, 12.5f);
        lines.startNewSubPath (9.0f, 16.5f);
        lines.lineTo (16.5f, 16.5f);
        filled.addPath (strokeOpenPath (lines, 1.6f));

        return filled;
    }

    // Informazioni (About): cerchio con una "i" (punto + asta) - il glifo
    // universale per "informazioni sull'app".
    juce::Path makeInfoPath()
    {
        juce::Path circle;
        circle.addEllipse (3.0f, 3.0f, 18.0f, 18.0f);
        auto filled = strokeOpenPath (circle, 1.7f);

        juce::Path stem;
        stem.startNewSubPath (12.0f, 10.8f);
        stem.lineTo (12.0f, 16.6f);
        filled.addPath (strokeOpenPath (stem, 2.0f));

        juce::Path dot;
        dot.addEllipse (10.75f, 6.6f, 2.5f, 2.5f);
        filled.addPath (dot);
        return filled;
    }

    // Libro aperto: due pagine con il dorso al centro.
    juce::Path makeBookPath()
    {
        juce::Path outline;
        outline.startNewSubPath (3.0f, 5.0f);
        outline.lineTo (9.0f, 5.0f);
        outline.quadraticTo (11.0f, 5.0f, 12.0f, 7.0f);
        outline.quadraticTo (13.0f, 5.0f, 15.0f, 5.0f);
        outline.lineTo (21.0f, 5.0f);
        outline.lineTo (21.0f, 18.0f);
        outline.lineTo (15.0f, 18.0f);
        outline.quadraticTo (13.0f, 18.0f, 12.0f, 20.0f);
        outline.quadraticTo (11.0f, 18.0f, 9.0f, 18.0f);
        outline.lineTo (3.0f, 18.0f);
        outline.closeSubPath();
        auto filled = strokeOpenPath (outline, 1.6f);

        juce::Path spine;
        spine.startNewSubPath (12.0f, 7.0f);
        spine.lineTo (12.0f, 20.0f);
        filled.addPath (strokeOpenPath (spine, 1.4f));
        return filled;
    }

    // Parentesi tonde con "=" a sinistra: la forma funzionale "a = f(x)".
    juce::Path makeCodeBracesPath()
    {
        juce::Path left;
        left.startNewSubPath (11.5f, 4.5f);
        left.quadraticTo (7.5f, 12.0f, 11.5f, 19.5f);
        auto filled = strokeOpenPath (left, 1.8f);

        juce::Path right;
        right.startNewSubPath (16.5f, 4.5f);
        right.quadraticTo (20.5f, 12.0f, 16.5f, 19.5f);
        filled.addPath (strokeOpenPath (right, 1.8f));

        juce::Path eq;
        eq.startNewSubPath (2.5f, 10.0f); eq.lineTo (6.5f, 10.0f);
        eq.startNewSubPath (2.5f, 14.0f); eq.lineTo (6.5f, 14.0f);
        filled.addPath (strokeOpenPath (eq, 1.8f));
        return filled;
    }

    // Forbici stilizzate: due anelli in basso e due lame incrociate.
    juce::Path makeCutPath()
    {
        juce::Path rings;
        rings.addEllipse (3.5f, 14.5f, 6.0f, 6.0f);
        rings.addEllipse (14.5f, 14.5f, 6.0f, 6.0f);
        auto filled = strokeOpenPath (rings, 1.6f);

        juce::Path blades;
        blades.startNewSubPath (8.0f, 15.5f);  blades.lineTo (19.0f, 3.5f);
        blades.startNewSubPath (16.0f, 15.5f); blades.lineTo (5.0f, 3.5f);
        filled.addPath (strokeOpenPath (blades, 1.8f));
        return filled;
    }

    // Due rettangoli sovrapposti.
    juce::Path makeCopyPath()
    {
        juce::Path back;
        back.addRoundedRectangle (4.0f, 3.0f, 11.0f, 13.0f, 2.0f);
        auto filled = strokeOpenPath (back, 1.6f);

        juce::Path front;
        front.addRoundedRectangle (9.0f, 8.0f, 11.0f, 13.0f, 2.0f);
        filled.addPath (strokeOpenPath (front, 1.6f));
        return filled;
    }

    // Blocco appunti con clip in alto.
    juce::Path makePastePath()
    {
        juce::Path board;
        board.addRoundedRectangle (5.0f, 5.0f, 14.0f, 16.0f, 2.0f);
        auto filled = strokeOpenPath (board, 1.6f);

        juce::Path clip;
        clip.addRoundedRectangle (9.0f, 3.0f, 6.0f, 4.0f, 1.0f);
        filled.addPath (clip);
        return filled;
    }

    // Rettangolo tratteggiato: "seleziona tutto".
    juce::Path makeSelectAllPath()
    {
        juce::Path filled;
        const float x0 = 4.0f, y0 = 4.0f, x1 = 20.0f, y1 = 20.0f, dash = 3.0f, t = 1.6f;

        for (float x = x0; x < x1; x += dash * 2.0f)
        {
            filled.addRectangle (x, y0, juce::jmin (dash, x1 - x), t);
            filled.addRectangle (x, y1 - t, juce::jmin (dash, x1 - x), t);
        }

        for (float y = y0; y < y1; y += dash * 2.0f)
        {
            filled.addRectangle (x0, y, t, juce::jmin (dash, y1 - y));
            filled.addRectangle (x1 - t, y, t, juce::jmin (dash, y1 - y));
        }

        return filled;
    }

    // Righe di testo rientrate con una freccia a sinistra.
    juce::Path makeIndentPath()
    {
        juce::Path filled;
        filled.addRoundedRectangle (4.0f, 4.5f, 16.0f, 1.8f, 0.9f);
        filled.addRoundedRectangle (11.0f, 9.0f, 9.0f, 1.8f, 0.9f);
        filled.addRoundedRectangle (11.0f, 13.5f, 9.0f, 1.8f, 0.9f);
        filled.addRoundedRectangle (4.0f, 18.0f, 16.0f, 1.8f, 0.9f);

        juce::Path arrow;
        arrow.addTriangle (4.0f, 9.0f, 8.5f, 12.2f, 4.0f, 15.4f);
        filled.addPath (arrow);
        return filled;
    }

    // ";" grande: il commento di Csound.
    juce::Path makeCommentPath()
    {
        juce::Path filled;
        filled.addEllipse (10.0f, 6.0f, 4.0f, 4.0f);
        filled.addEllipse (10.0f, 13.0f, 4.0f, 4.0f);

        juce::Path tail;
        tail.startNewSubPath (13.0f, 16.5f);
        tail.quadraticTo (12.5f, 19.5f, 10.0f, 20.5f);
        filled.addPath (strokeOpenPath (tail, 1.6f));
        return filled;
    }

    juce::Path makeSheetIconPath (CsoundActionSheetIcon icon)
    {
        switch (icon)
        {
            case CsoundActionSheetIcon::undo:         return makeArrowPath (true);
            case CsoundActionSheetIcon::redo:          return makeArrowPath (false);
            case CsoundActionSheetIcon::save:          return makeTrayArrowPath (false);
            case CsoundActionSheetIcon::load:          return makeTrayArrowPath (true);
            case CsoundActionSheetIcon::sidebarPanel:  return makeSidebarPanelPath();
            case CsoundActionSheetIcon::console:       return makeConsolePath();
            case CsoundActionSheetIcon::sliderFloat:   return makeSliderPath (false);
            case CsoundActionSheetIcon::sliderInt:     return makeSliderPath (true);
            case CsoundActionSheetIcon::toggleSwitch:  return makeTogglePath();
            case CsoundActionSheetIcon::comboMenu:     return makeComboMenuPath();
            case CsoundActionSheetIcon::eyeOpen:       return makeEyePath (true);
            case CsoundActionSheetIcon::eyeClosed:     return makeEyePath (false);
            case CsoundActionSheetIcon::trash:         return makeTrashPath();
            case CsoundActionSheetIcon::resetDefault:  return makeResetDefaultPath();
            case CsoundActionSheetIcon::newDocument:   return makeNewDocumentPath();
            case CsoundActionSheetIcon::info:          return makeInfoPath();
            case CsoundActionSheetIcon::book:          return makeBookPath();
            case CsoundActionSheetIcon::codeBraces:    return makeCodeBracesPath();
            case CsoundActionSheetIcon::cut:           return makeCutPath();
            case CsoundActionSheetIcon::copy:          return makeCopyPath();
            case CsoundActionSheetIcon::paste:         return makePastePath();
            case CsoundActionSheetIcon::selectAll:     return makeSelectAllPath();
            case CsoundActionSheetIcon::indent:        return makeIndentPath();
            case CsoundActionSheetIcon::comment:       return makeCommentPath();
            case CsoundActionSheetIcon::none:
            default:                                   return {};
        }
    }
}

void CsoundActionSheet::show (juce::Component& host, const juce::String& title,
                               std::vector<CsoundActionSheetItem> items,
                               std::function<void (int)> onSelected)
{
    // Si autogestisce come juce::PopupMenu::showMenuAsync: creato sull'heap,
    // si rimuove e si distrugge da solo alla chiusura (vedi dismiss()).
    auto* sheet = new CsoundActionSheet (title, std::move (items), std::move (onSelected));
    host.addAndMakeVisible (sheet);
    sheet->setBounds (host.getLocalBounds());
    sheet->toFront (false);
    sheet->animateIn();
}

CsoundActionSheet::CsoundActionSheet (const juce::String& title, std::vector<CsoundActionSheetItem> items,
                                       std::function<void (int)> onSelectedIn)
    : onSelected (std::move (onSelectedIn))
{
    pageStack.push_back ({ title, std::move (items) });

    rowsViewport.setViewedComponent (&rowsContent, false);
    rowsViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (rowsViewport);
}

void CsoundActionSheet::resized()
{
    recomputeLayout();
}

void CsoundActionSheet::parentSizeChanged()
{
    // BUG corretto (richiesta esplicita: "non si ridimensiona con il
    // component padre") - show() fissava le bounds UNA SOLA volta alla
    // creazione (host.getLocalBounds() di allora); se la finestra del
    // plugin veniva ridimensionata DOPO, questo foglio restava della
    // vecchia dimensione. setBounds() qui sotto fa scattare da solo
    // resized() -> recomputeLayout(), che ricalcola sheetArea centrata
    // nella nuova geometria del genitore.
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds());
}

void CsoundActionSheet::recomputeLayout()
{
    const bool hasBack = pageStack.size() > 1;
    const auto& page = pageStack.back();

    // Popup CENTRATO di larghezza ragionevole (richiesta esplicita: "troppo
    // grande... un menu ragionevolmente ampio senza esagerare"), non piu'
    // un bottom sheet a tutta larghezza - clampata tra popupMinWidth e la
    // larghezza host meno i due margini esterni.
    const int sheetWidth = juce::jmin (popupPreferredWidth,
                                         juce::jmax (popupMinWidth, getWidth() - popupOuterMargin * 2));

    laidOutRows.clear();
    int y = 0;

    if (hasBack)
    {
        laidOutRows.push_back ({ { 0, y, sheetWidth, rowHeight }, -1, LaidOutRow::Kind::back });
        y += rowHeight;
    }

    for (int i = 0; i < (int) page.items.size(); ++i)
    {
        const auto& item = page.items[(size_t) i];
        auto kind = LaidOutRow::Kind::normal;
        int h = rowHeight;

        if (item.isSeparator)
        {
            kind = LaidOutRow::Kind::separator;
            h = separatorHeight;
        }
        else if (item.sectionHeader.isNotEmpty())
        {
            kind = LaidOutRow::Kind::sectionHeader;
            h = sectionHeaderHeight;
        }

        laidOutRows.push_back ({ { 0, y, sheetWidth, h }, i, kind });
        y += h;
    }

    const int contentHeight = y + 10;
    rowsContent.setSize (sheetWidth, contentHeight);

    const int titleHeight = page.title.isNotEmpty() ? titleAreaHeight : 0;
    const int chromeHeight = topPadding + titleHeight;
    const int available = (getHeight() * maxSheetHeightPercent) / 100;
    const int sheetHeight = juce::jmin (chromeHeight + contentHeight, available);

    sheetArea = juce::Rectangle<int> (sheetWidth, sheetHeight).withCentre (getLocalBounds().getCentre());

    rowsViewport.setBounds (sheetArea.getX(), sheetArea.getY() + chromeHeight,
                             sheetArea.getWidth(), juce::jmax (0, sheetArea.getHeight() - chromeHeight));

    repaint();
}

void CsoundActionSheet::animateIn()
{
    // Solo un fade (0 -> 1) sull'INTERO componente (scrim compreso, vedi
    // setAlpha() sotto - juce::Component::setAlpha applica la trasparenza a
    // tutto l'albero, figli compresi, in un colpo) - niente piu' uno slide
    // dal basso: non ha senso per un popup centrato (richiesta esplicita,
    // vedi il commento in testa alla classe nel .h).
    sheetAlpha = 0.0f;
    setAlpha (0.0f);
    startTimerHz (60);
}

void CsoundActionSheet::timerCallback()
{
    sheetAlpha += (1.0f - sheetAlpha) * 0.3f;

    if (sheetAlpha > 0.98f)
    {
        sheetAlpha = 1.0f;
        stopTimer();
    }

    setAlpha (sheetAlpha);
}

void CsoundActionSheet::paint (juce::Graphics& g)
{
    // Scrim piu' leggero (richiesta esplicita: "e' troppo invasivo") - non
    // oscura piu' quasi a meta' il resto dell'interfaccia dietro al popup.
    g.fillAll (juce::Colours::black.withAlpha (0.28f));

    if (sheetArea.getHeight() <= 0)
        return;

    // Popup centrato: TUTTI e quattro gli angoli arrotondati (un bottom
    // sheet arrotondava solo quelli in alto - non piu' applicabile qui).
    juce::Path sheetPath;
    sheetPath.addRoundedRectangle ((float) sheetArea.getX(), (float) sheetArea.getY(),
                                     (float) sheetArea.getWidth(), (float) sheetArea.getHeight(),
                                     sheetCornerRadius, sheetCornerRadius,
                                     true, true, true, true);

    g.setColour (kSheetBg);
    g.fillPath (sheetPath);

    const auto& page = pageStack.back();
    if (page.title.isNotEmpty())
    {
        auto titleBounds = juce::Rectangle<int> (sheetArea.getX(), sheetArea.getY() + topPadding,
                                                    sheetArea.getWidth(), titleAreaHeight);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        g.drawFittedText (page.title, titleBounds.reduced (16, 0), juce::Justification::centred, 1);
    }
}

void CsoundActionSheet::mouseDown (const juce::MouseEvent& event)
{
    // Raggiunto solo se il tap cade FUORI da rowsViewport (che e' un figlio
    // e intercetta da se' i propri eventi) - cioe' sullo scrim scuro.
    pressStartedOnScrim = ! sheetArea.contains (event.getPosition());
}

void CsoundActionSheet::mouseUp (const juce::MouseEvent& event)
{
    if (pressStartedOnScrim && ! sheetArea.contains (event.getPosition()))
        dismiss (0);

    pressStartedOnScrim = false;
}

int CsoundActionSheet::findRowAt (juce::Point<int> positionInRowsContent) const
{
    for (int i = 0; i < (int) laidOutRows.size(); ++i)
    {
        const auto& row = laidOutRows[(size_t) i];
        if (! row.bounds.contains (positionInRowsContent))
            continue;

        if (row.kind == LaidOutRow::Kind::back)
            return i;

        if (row.kind == LaidOutRow::Kind::normal)
        {
            const auto& item = pageStack.back().items[(size_t) row.itemIndex];
            return item.enabled ? i : -1;
        }

        return -1; // separatore/intestazione di sezione: non cliccabile
    }

    return -1;
}

void CsoundActionSheet::setPressedRow (int rowIndex)
{
    if (pressedRowIndex == rowIndex)
        return;

    pressedRowIndex = rowIndex;
    rowsContent.repaint();
}

void CsoundActionSheet::activateRow (int rowIndex)
{
    const auto& row = laidOutRows[(size_t) rowIndex];

    if (row.kind == LaidOutRow::Kind::back)
    {
        goBack();
        return;
    }

    const auto& item = pageStack.back().items[(size_t) row.itemIndex];

    if (! item.subItems.empty())
    {
        // Drill-down: apre una seconda pagina DENTRO lo stesso foglio
        // (con una riga "Back" in testa) invece di un sottomenu annidato.
        pageStack.push_back ({ item.text, item.subItems });
        recomputeLayout();
        rowsViewport.setViewPosition (0, 0);
        return;
    }

    dismiss (item.id);
}

void CsoundActionSheet::goBack()
{
    if (pageStack.size() <= 1)
        return;

    pageStack.pop_back();
    recomputeLayout();
    rowsViewport.setViewPosition (0, 0);
}

void CsoundActionSheet::dismiss (int resultId)
{
    setVisible (false);
    setInterceptsMouseClicks (false, false);

    auto callback = onSelected;
    juce::Component::SafePointer<CsoundActionSheet> self (this);

    // Autodistruzione differita: non si puo' fare "delete this" mentre uno
    // dei propri metodi (questo stesso dismiss(), chiamato da un
    // mouseUp/activateRow ancora sullo stack) e' in esecuzione. Stesso
    // pattern usato altrove in questo codebase (es. onRemoved in
    // CsoundParameterEditor.cpp).
    juce::MessageManager::callAsync ([self, callback, resultId]
    {
        if (self != nullptr)
        {
            if (auto* parent = self->getParentComponent())
                parent->removeChildComponent (self.getComponent());

            delete self.getComponent();
        }

        if (callback)
            callback (resultId);
    });
}

void CsoundActionSheet::RowsContent::paint (juce::Graphics& g)
{
    const auto& page = owner.pageStack.back();

    for (int i = 0; i < (int) owner.laidOutRows.size(); ++i)
    {
        const auto& row = owner.laidOutRows[(size_t) i];
        auto bounds = row.bounds;

        if (row.kind == CsoundActionSheet::LaidOutRow::Kind::back)
        {
            if (i == owner.pressedRowIndex)
            {
                g.setColour (kRowPressedBg);
                g.fillRect (bounds);
            }

            auto content = bounds.reduced (CsoundActionSheet::rowHorizontalPadding, 0);
            auto chevronArea = content.removeFromLeft (24).withSizeKeepingCentre (16, 16);
            auto chevron = makeChevronLeftPath();
            chevron.scaleToFit ((float) chevronArea.getX(), (float) chevronArea.getY(),
                                  (float) chevronArea.getWidth(), (float) chevronArea.getHeight(), true);
            g.setColour (kAccent);
            g.fillPath (chevron);

            content.removeFromLeft (8);
            g.setColour (kAccent);
            g.setFont (juce::Font (juce::FontOptions (16.0f)));
            g.drawFittedText ("Back", content, juce::Justification::centredLeft, 1);
            continue;
        }

        const auto& item = page.items[(size_t) row.itemIndex];

        if (row.kind == CsoundActionSheet::LaidOutRow::Kind::separator)
        {
            g.setColour (kSeparator);
            g.fillRect (bounds.withSizeKeepingCentre (bounds.getWidth() - CsoundActionSheet::rowHorizontalPadding * 2, 1));
            continue;
        }

        if (row.kind == CsoundActionSheet::LaidOutRow::Kind::sectionHeader)
        {
            auto content = bounds.reduced (CsoundActionSheet::rowHorizontalPadding, 0);
            g.setColour (kTextMuted);
            g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            g.drawFittedText (item.sectionHeader.toUpperCase(), content, juce::Justification::centredLeft, 1);
            continue;
        }

        // Riga normale
        if (i == owner.pressedRowIndex && item.enabled)
        {
            g.setColour (kRowPressedBg);
            g.fillRect (bounds);
        }

        auto content = bounds.reduced (CsoundActionSheet::rowHorizontalPadding, 0);

        // Spazio per la spunta riservato SEMPRE (anche se non ticked), cosi'
        // il testo resta allineato riga per riga nella stessa pagina.
        auto tickArea = content.removeFromLeft (28);
        if (item.ticked)
        {
            auto check = makeCheckmarkPath();
            auto iconArea = tickArea.withSizeKeepingCentre (18, 18);
            check.scaleToFit ((float) iconArea.getX(), (float) iconArea.getY(),
                                (float) iconArea.getWidth(), (float) iconArea.getHeight(), true);
            g.setColour (kAccent);
            g.fillPath (check);
        }

        if (! item.subItems.empty())
        {
            auto chevronArea = content.removeFromRight (28).withSizeKeepingCentre (14, 14);
            auto chevron = makeChevronRightPath();
            chevron.scaleToFit ((float) chevronArea.getX(), (float) chevronArea.getY(),
                                  (float) chevronArea.getWidth(), (float) chevronArea.getHeight(), true);
            g.setColour (kTextMuted);
            g.fillPath (chevron);
        }

        // Icona (richiesta esplicita) - spazio riservato SEMPRE, come per
        // tickArea sopra, cosi' il testo resta allineato riga per riga
        // anche quando alcune righe della stessa pagina non hanno icona
        // (icon == none).
        auto iconArea = content.removeFromLeft (CsoundActionSheet::iconAreaWidth);
        if (item.icon != CsoundActionSheetIcon::none)
        {
            auto path = makeSheetIconPath (item.icon);
            auto glyphArea = iconArea.withSizeKeepingCentre (18, 18);
            path.scaleToFit ((float) glyphArea.getX(), (float) glyphArea.getY(),
                               (float) glyphArea.getWidth(), (float) glyphArea.getHeight(), true);
            g.setColour (item.enabled ? kTextMuted : kTextDisabled);
            g.fillPath (path);
        }

        g.setColour (item.enabled ? kText : kTextDisabled);
        g.setFont (juce::Font (juce::FontOptions (16.0f)));
        g.drawFittedText (item.text, content, juce::Justification::centredLeft, 1);
    }
}

void CsoundActionSheet::RowsContent::mouseDown (const juce::MouseEvent& event)
{
    owner.setPressedRow (owner.findRowAt (event.getPosition()));
}

void CsoundActionSheet::RowsContent::mouseUp (const juce::MouseEvent& event)
{
    const int rowIndex = owner.findRowAt (event.getPosition());
    const int pressed = owner.pressedRowIndex;
    owner.setPressedRow (-1);

    if (rowIndex == pressed && rowIndex >= 0)
        owner.activateRow (rowIndex);
}

void CsoundActionSheet::RowsContent::mouseExit (const juce::MouseEvent&)
{
    owner.setPressedRow (-1);
}
