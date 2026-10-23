#pragma once

#include <JuceHeader.h>

/**
    Browser interno dei .csd di apeCsound (apeCsound/CSD: Examples, User e
    sottocartelle), usato su iOS al posto del document picker.

    Perche': su iOS la cartella di apeCsound sta nel contenitore dell'App
    Group (condiviso fra app Standalone ed estensione AUv3), che ne' l'app
    File ne' UIDocumentPickerViewController mostrano mai. Con il solo picker
    gli esempi e i file salvati in CSD/User erano irraggiungibili. Questo
    browser legge e scrive direttamente nel contenitore, identico nell'app e
    nell'AUv3; "Other Locations..." apre comunque il picker di sistema per
    iCloud Drive e gli altri provider.

    Presentato come il picker nativo: velo scuro e un foglio che sale dal
    basso - "form sheet" centrato su iPad (schermi larghi), "page sheet" a
    tutta larghezza con gli angoli in alto arrotondati su iPhone. Barra in
    alto stile iOS: Cancel a sinistra, titolo al centro, Save a destra (solo
    in salvataggio). Si chiude con Cancel, Esc o un tocco fuori dal foglio.

    Modalita':
      - open: tocco su un file -> onOpen(file);
      - save: campo nome sotto la barra (sopra la tastiera), Save ->
        onSave(file); se il file esiste, Save diventa "Replace" (rosso) e
        serve un secondo tocco. Toccare un file ne copia il nome nel campo.
      - manage ("Manage Files...", solo iOS): tutta la cartella apeCsound
        (CSD, SSDIR, SFDIR, SADIR, INCDIR), tutti i file con dimensione.
        Tocco su un file = seleziona/deseleziona; "Export" in alto a destra
        (i selezionati, o tutti quelli della cartella) -> onExport(files);
        "Import..." in basso -> onImport(cartella corrente). Il chiamante
        usa il picker di sistema; il foglio resta aperto (refreshContents).
    I file dentro CSD/User si possono cancellare (cestino a destra della
    riga, secondo tocco su "Delete" per confermare); in manage tutti tranne
    quelli in CSD/Examples. Gli esempi no: si ripristinano con "Restore Examples".
*/
class CsdBrowserView final : public juce::Component
{
public:
    enum class Mode { open, save, manage };

    std::function<void (const juce::File&)> onOpen;
    std::function<void (const juce::File&)> onSave;
    std::function<void (const juce::Array<juce::File>&)> onExport; // manage
    std::function<void (const juce::File&)> onImport;              // manage: cartella di destinazione
    std::function<void()> onOtherLocations;
    std::function<void (const juce::String&)> onLog;

    CsdBrowserView()
    {
        setWantsKeyboardFocus (true);
        setInterceptsMouseClicks (true, true);
        addAndMakeVisible (sheet);
    }

    /** root: cartella CSD (non si sale oltre). start: cartella iniziale
        (dentro root, altrimenti root). proposedName: solo in save. */
    void show (Mode newMode, const juce::String& newTitle, const juce::File& newRoot,
               const juce::File& start, const juce::String& proposedName = {})
    {
        sheet.setup (newMode, newTitle, newRoot, start, proposedName);

        closing = false;
        setAlpha (1.0f);
        setVisible (true);
        toFront (true);

        // Il foglio sale dal basso (come i fogli modali di iOS).
        const auto target = sheetBounds();
        auto& animator = juce::Desktop::getInstance().getAnimator();
        animator.cancelAnimation (&sheet, false);
        sheet.setBounds (target.translated (0, getHeight() - target.getY()));
        animator.animateComponent (&sheet, target, 1.0f, 260, false, 0.0, 0.0);
        repaint();

        sheet.focusInitial();
    }

    /** Rilegge la cartella corrente (es. dopo un Import). */
    void refreshContents() { sheet.refresh(); }

    void hide()
    {
        if (! isVisible() || closing)
            return;

        closing = true;
        sheet.releaseFocus();

        // Il foglio scende, poi la vista sparisce (niente fadeOut del
        // ComponentAnimator: nasconderebbe subito il foglio, sostituito da
        // un'immagine, e la discesa non si vedrebbe).
        juce::Desktop::getInstance().getAnimator().animateComponent (
            &sheet, sheet.getBounds().translated (0, getHeight() - sheet.getY()), 1.0f, 200, false, 0.0, 0.0);

        juce::Component::SafePointer<CsdBrowserView> safeThis (this);
        juce::Timer::callAfterDelay (210, [safeThis]
        {
            if (safeThis != nullptr && safeThis->closing)
            {
                safeThis->closing = false;
                safeThis->setVisible (false);
            }
        });
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.40f));
    }

    void resized() override
    {
        if (! juce::Desktop::getInstance().getAnimator().isAnimating (&sheet))
            sheet.setBounds (sheetBounds());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // Tocco sul velo, fuori dal foglio: annulla.
        if (e.eventComponent == this && ! sheet.getBounds().contains (e.getPosition()))
            hide();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            hide();
            return true;
        }
        return false;
    }

private:
    //==========================================================================
    static juce::Colour sheetBg() { return juce::Colour (0xff141d24); }
    static juce::Colour border()  { return juce::Colour (0xff2a3540); }
    static juce::Colour text()    { return juce::Colour (0xffe8eef1); }
    static juce::Colour muted()   { return juce::Colour (0xff8a9aa5); }
    static juce::Colour accent()  { return juce::Colour (0xff4aa3b8); }
    static juce::Colour rowBg()   { return juce::Colour (0xff1c2730); }
    static juce::Colour danger()  { return juce::Colour (0xffe5534b); }

    // Area sicura su iOS (notch, barra di stato, indicatore home): stessa
    // logica di GuideView::resized().
    juce::Rectangle<int> safeArea() const
    {
        auto area = getLocalBounds();
       #if JUCE_IOS
        if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        {
            const auto safeOnScreen = display->safeAreaInsets.subtractedFrom (display->totalArea);
            const auto safeLocal = getLocalArea (nullptr, safeOnScreen).getIntersection (area);
            if (! safeLocal.isEmpty())
                area = safeLocal;
        }
       #endif
        return area;
    }

    bool isFormSheet() const
    {
        const auto s = safeArea();
        return s.getWidth() >= 700 && s.getHeight() >= 560;
    }

    juce::Rectangle<int> sheetBounds() const
    {
        const auto s = safeArea();

        if (isFormSheet())
        {
            // iPad: foglio centrato, dimensioni del form sheet di sistema.
            const int w = juce::jmin (620, s.getWidth() - 80);
            const int h = juce::jmin (720, s.getHeight() - 60);
            sheet.bottomInset = 0;
            return juce::Rectangle<int> (w, h).withCentre (s.getCentre());
        }

        // iPhone (o finestra stretta): page sheet a tutta larghezza, staccato
        // di poco dall'alto; arriva fino al fondo della vista (sotto
        // l'indicatore home, come quello di sistema).
        auto r = getLocalBounds().withTop (s.getY() + 10);
        sheet.bottomInset = getLocalBounds().getBottom() - s.getBottom();
        return r;
    }

    //==========================================================================
    // Bottone di testo della barra di navigazione (Cancel / Save), stile iOS.
    struct BarButton final : public juce::Button
    {
        BarButton() : juce::Button ({}) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto c = colour;
            if (! isEnabled())      c = muted().withAlpha (0.5f);
            else if (down)          c = c.withAlpha (0.45f);
            else if (over)          c = c.brighter (0.2f);

            g.setColour (c);
            g.setFont (juce::Font (juce::FontOptions (17.0f, bold ? juce::Font::bold : juce::Font::plain)));
            g.drawFittedText (getButtonText(), getLocalBounds(), justification, 1);
        }

        juce::Colour colour = accent();
        bool bold = false;
        juce::Justification justification = juce::Justification::centredLeft;
    };

    //==========================================================================
    struct Sheet final : public juce::Component,
                         private juce::ListBoxModel
    {
        explicit Sheet (CsdBrowserView& o) : owner (o)
        {
            cancelButton.setButtonText ("Cancel");
            cancelButton.onClick = [this] { owner.hide(); };
            addAndMakeVisible (cancelButton);

            saveButton.setButtonText ("Save");
            saveButton.bold = true;
            saveButton.justification = juce::Justification::centredRight;
            saveButton.onClick = [this] { if (mode == Mode::manage) performExport(); else performSave(); };
            addChildComponent (saveButton);

            backButton.setButtonText (juce::String::charToString (0x2039) + " Back");
            backButton.onClick = [this] { goUp(); };
            addAndMakeVisible (backButton);

            otherButton.setButtonText ("Other Locations...");
            otherButton.justification = juce::Justification::centred;
            otherButton.onClick = [this]
            {
                if (mode == Mode::manage)
                {
                    // Il foglio resta aperto: il picker si apre sopra.
                    if (owner.onImport)
                        owner.onImport (current);
                    return;
                }

                owner.hide();
                if (owner.onOtherLocations)
                    owner.onOtherLocations();
            };
            addAndMakeVisible (otherButton);

            nameField.setMultiLine (false);
            nameField.setReturnKeyStartsNewLine (false);
            nameField.setFont (juce::Font (juce::FontOptions (16.0f)));
            nameField.setIndents (10, 0);
            nameField.setJustification (juce::Justification::centredLeft);
            nameField.setColour (juce::TextEditor::backgroundColourId, rowBg());
            nameField.setColour (juce::TextEditor::textColourId, text());
            nameField.setColour (juce::TextEditor::outlineColourId, border());
            nameField.setColour (juce::TextEditor::focusedOutlineColourId, accent());
            nameField.setColour (juce::TextEditor::highlightColourId, accent().withAlpha (0.4f));
            nameField.setColour (juce::CaretComponent::caretColourId, accent());
            nameField.setTextToShowWhenEmpty ("Name", muted());
            nameField.onTextChange = [this] { setReplacePending (false); };
            nameField.onReturnKey  = [this] { performSave(); };
            addChildComponent (nameField);

            list.setModel (this);
            list.setRowHeight (48);
            list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
            list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
            list.setRowSelectedOnMouseDown (false);
            addAndMakeVisible (list);
        }

        void setup (Mode newMode, const juce::String& newTitle, const juce::File& newRoot,
                    const juce::File& start, const juce::String& proposedName)
        {
            mode  = newMode;
            title = newTitle;
            root  = newRoot;
            root.createDirectory();

            current = (start == root || start.isAChildOf (root)) && start.isDirectory() ? start : root;

            nameField.setVisible (mode == Mode::save);
            saveButton.setVisible (mode != Mode::open);
            cancelButton.setButtonText (mode == Mode::manage ? "Done" : "Cancel");
            otherButton.setButtonText (mode == Mode::manage ? "Import Files Here..." : "Other Locations...");
            nameField.setText (proposedName, juce::dontSendNotification);
            selectedFiles.clear();
            setReplacePending (false);

            refresh();
            resized();
        }

        void focusInitial()
        {
            if (mode == Mode::save)
            {
                nameField.grabKeyboardFocus();
                const auto name = nameField.getText();
                const auto dot = name.lastIndexOfChar ('.');
                nameField.setHighlightedRegion ({ 0, dot > 0 ? dot : name.length() });
            }
            else
            {
                owner.grabKeyboardFocus();
            }
        }

        void releaseFocus() { nameField.giveAwayKeyboardFocus(); }

        //======================================================================
        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const float r = 12.0f;
            const bool form = owner.isFormSheet();

            juce::Path shape;
            shape.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(), r, r,
                                       true, true, form, form);
            g.setColour (sheetBg());
            g.fillPath (shape);
            g.setColour (border());
            g.strokePath (shape, juce::PathStrokeType (1.0f));

            // Maniglia (solo page sheet, come su iPhone).
            if (! form)
            {
                g.setColour (muted().withAlpha (0.5f));
                g.fillRoundedRectangle (b.getCentreX() - 18.0f, 5.0f, 36.0f, 5.0f, 2.5f);
            }

            // Titolo al centro della barra.
            g.setColour (text());
            g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
            g.drawFittedText (title, getWidth() / 2 - 130, 0, 260, navBarHeight, juce::Justification::centred, 1);

            g.setColour (border());
            g.fillRect (0, navBarHeight, getWidth(), 1);

            // Percorso corrente (relativo alla radice: "CSD / Examples / Cabbage").
            g.setColour (muted());
            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.drawFittedText (displayPath(), pathLabelBounds, juce::Justification::centredLeft, 1);

            if (entries.isEmpty())
            {
                g.setFont (juce::Font (juce::FontOptions (14.0f)));
                g.drawFittedText (mode == Mode::manage ? "Empty folder" : "No .csd files here",
                                  list.getBounds().withHeight (60),
                                  juce::Justification::centred, 1);
            }

            g.setColour (border());
            g.fillRect (0, list.getBottom(), getWidth(), 1);
        }

        void resized() override
        {
            auto c = getLocalBounds();
            c.removeFromBottom (bottomInset);

            auto bar = c.removeFromTop (navBarHeight).reduced (16, 0);
            cancelButton.setBounds (bar.removeFromLeft (90));
            saveButton.setBounds (bar.removeFromRight (90));

            c.removeFromTop (1);
            c = c.reduced (16, 0);

            if (mode == Mode::save)
            {
                c.removeFromTop (10);
                nameField.setBounds (c.removeFromTop (40));
                c.removeFromTop (4);
            }

            auto pathRow = c.removeFromTop (40);
            const bool canGoUp = current != root;
            backButton.setVisible (canGoUp);
            if (canGoUp)
            {
                backButton.setBounds (pathRow.removeFromLeft (76));
                pathRow.removeFromLeft (4);
            }
            pathLabelBounds = pathRow;

            auto bottom = c.removeFromBottom (52);
            otherButton.setBounds (bottom.withSizeKeepingCentre (juce::jmin (260, bottom.getWidth()), 44));

            list.setBounds (c);
        }

        //======================================================================
        struct Entry
        {
            juce::File file;
            bool isFolder = false;
        };

        juce::String displayPath() const
        {
            auto rel = current == root ? juce::String()
                                       : current.getRelativePathFrom (root).replaceCharacter ('\\', '/');
            auto path = root.getFileName();
            if (rel.isNotEmpty())
                path << " / " << rel.replace ("/", " / ");
            return path;
        }

        bool isDeletable (const Entry& en) const
        {
            if (en.isFolder)
                return false;

            // manage: tutto tranne gli esempi di fabbrica (Restore Examples).
            if (mode == Mode::manage)
                return ! en.file.isAChildOf (root.getChildFile ("CSD").getChildFile ("Examples"));

            // Load/Save: solo i file dell'utente (CSD/User e sottocartelle).
            return en.file.isAChildOf (root.getChildFile ("User"));
        }

        void refresh()
        {
            entries.clear();
            pendingDeleteRow = -1;

            auto byName = [] (const juce::File& a, const juce::File& b)
            {
                return a.getFileName().compareNatural (b.getFileName()) < 0;
            };

            auto folders = current.findChildFiles (juce::File::findDirectories, false);
            auto files   = current.findChildFiles (juce::File::findFiles, false,
                                                   mode == Mode::manage ? "*" : "*.csd");
            std::sort (folders.begin(), folders.end(), byName);
            std::sort (files.begin(), files.end(), byName);

            for (auto& f : folders)
                if (! f.getFileName().startsWithChar ('.') && f.getFileName() != "__MACOSX")
                    entries.add ({ f, true });

            for (auto& f : files)
                if (! f.getFileName().startsWithChar ('.'))
                    entries.add ({ f, false });

            selectedFiles.removeIf ([this] (const juce::File& f)
            {
                return ! f.existsAsFile() || f.getParentDirectory() != current;
            });
            updateExportButton();

            list.updateContent();
            list.deselectAllRows();
            list.getVerticalScrollBar().setCurrentRangeStart (0);
            repaint();
        }

        void navigateTo (const juce::File& folder)
        {
            current = folder;
            refresh();
            resized();
        }

        void goUp()
        {
            if (current != root)
                navigateTo (current.getParentDirectory());
        }

        void updateExportButton()
        {
            if (mode != Mode::manage)
                return;

            const bool anyFile = std::any_of (entries.begin(), entries.end(),
                                              [] (const Entry& en) { return ! en.isFolder; });

            saveButton.setButtonText (selectedFiles.isEmpty() ? juce::String ("Export All")
                                                              : "Export (" + juce::String (selectedFiles.size()) + ")");
            saveButton.colour = accent();
            saveButton.setEnabled (anyFile);
            saveButton.repaint();
        }

        void performExport()
        {
            juce::Array<juce::File> files (selectedFiles);

            if (files.isEmpty())
                for (const auto& en : entries)
                    if (! en.isFolder)
                        files.add (en.file);

            if (! files.isEmpty() && owner.onExport)
                owner.onExport (files); // il foglio resta aperto
        }

        void setReplacePending (bool pending)
        {
            if (mode == Mode::manage)
            {
                updateExportButton();
                return;
            }


            replacePending = pending;
            saveButton.setEnabled (true);
            saveButton.setButtonText (pending ? "Replace" : "Save");
            saveButton.colour = pending ? danger() : accent();
            saveButton.repaint();
        }

        void performSave()
        {
            auto name = juce::File::createLegalFileName (nameField.getText().trim());

            if (name.isEmpty())
                return;

            if (! name.endsWithIgnoreCase (".csd"))
                name << ".csd";

            const auto target = current.getChildFile (name);

            if (target.exists() && ! replacePending)
            {
                setReplacePending (true);
                return;
            }

            owner.hide();
            if (owner.onSave)
                owner.onSave (target);
        }

        //======================================================================
        // ListBoxModel
        int getNumRows() override { return entries.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, entries.size()))
                return;

            const auto& en = entries.getReference (row);

            if (selected)
            {
                g.setColour (rowBg());
                g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 2.0f, (float) width, (float) height - 4.0f), 8.0f);
            }

            // Icona
            const juce::Rectangle<float> icon (8.0f, (float) height * 0.5f - 11.0f, 22.0f, 22.0f);
            juce::Path p;
            if (en.isFolder)
            {
                p.startNewSubPath (icon.getX(), icon.getY() + 4.0f);
                p.lineTo (icon.getX() + 8.0f, icon.getY() + 4.0f);
                p.lineTo (icon.getX() + 10.0f, icon.getY() + 7.0f);
                p.lineTo (icon.getRight(), icon.getY() + 7.0f);
                p.lineTo (icon.getRight(), icon.getBottom() - 2.0f);
                p.lineTo (icon.getX(), icon.getBottom() - 2.0f);
                p.closeSubPath();
                g.setColour (accent());
                g.fillPath (p);
            }
            else if (mode == Mode::manage)
            {
                // Cerchio di selezione (stile iOS): pieno con spunta se scelto.
                const auto c = icon.reduced (1.0f);
                if (selectedFiles.contains (en.file))
                {
                    g.setColour (accent());
                    g.fillEllipse (c);
                    juce::Path tick;
                    tick.startNewSubPath (c.getX() + c.getWidth() * 0.27f, c.getCentreY());
                    tick.lineTo (c.getX() + c.getWidth() * 0.45f, c.getY() + c.getHeight() * 0.68f);
                    tick.lineTo (c.getX() + c.getWidth() * 0.75f, c.getY() + c.getHeight() * 0.32f);
                    g.setColour (juce::Colours::white);
                    g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                else
                {
                    g.setColour (muted());
                    g.drawEllipse (c, 1.4f);
                }
            }
            else
            {
                const auto d = icon.reduced (3.0f, 0.0f);
                p.startNewSubPath (d.getX(), d.getY());
                p.lineTo (d.getRight() - 5.0f, d.getY());
                p.lineTo (d.getRight(), d.getY() + 5.0f);
                p.lineTo (d.getRight(), d.getBottom());
                p.lineTo (d.getX(), d.getBottom());
                p.closeSubPath();
                g.setColour (muted());
                g.strokePath (p, juce::PathStrokeType (1.4f));
            }

            int right = width - 8;

            if (en.isFolder)
            {
                g.setColour (muted());
                g.setFont (juce::Font (juce::FontOptions (18.0f)));
                g.drawText (juce::String::charToString (0x203A), right - 20, 0, 20, height, juce::Justification::centred);
                right -= 28;
            }
            else if (isDeletable (en))
            {
                const juce::Rectangle<int> del (width - kDeleteWidth, 0, kDeleteWidth, height);

                if (row == pendingDeleteRow)
                {
                    g.setColour (danger());
                    g.fillRoundedRectangle (del.toFloat().reduced (4.0f, 10.0f), 6.0f);
                    g.setColour (juce::Colours::white);
                    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
                    g.drawText ("Delete", del, juce::Justification::centred);
                }
                else
                {
                    // Cestino
                    const auto t = del.toFloat().withSizeKeepingCentre (14.0f, 16.0f);
                    juce::Path bin;
                    bin.addRectangle (t.getX() - 1.5f, t.getY() + 1.0f, t.getWidth() + 3.0f, 1.5f);
                    bin.addRectangle (t.getCentreX() - 3.0f, t.getY() - 1.0f, 6.0f, 1.5f);
                    g.setColour (muted());
                    g.fillPath (bin);
                    juce::Path body;
                    body.addRoundedRectangle (t.getX() + 1.0f, t.getY() + 4.0f, t.getWidth() - 2.0f, t.getHeight() - 4.0f, 1.5f);
                    g.strokePath (body, juce::PathStrokeType (1.3f));
                }

                right = width - kDeleteWidth - 4;
            }

            if (mode == Mode::manage && ! en.isFolder)
            {
                // Nome completo (estensione inclusa) + dimensione.
                g.setColour (muted());
                g.setFont (juce::Font (juce::FontOptions (12.0f)));
                g.drawText (juce::File::descriptionOfSizeInBytes (en.file.getSize()),
                            right - 70, 0, 70, height, juce::Justification::centredRight);
                right -= 76;
            }

            g.setColour (text());
            g.setFont (juce::Font (juce::FontOptions (15.0f)));
            g.drawFittedText (en.isFolder || mode == Mode::manage ? en.file.getFileName()
                                                                  : en.file.getFileNameWithoutExtension(),
                              40, 0, right - 40, height, juce::Justification::centredLeft, 1, 1.0f);

            g.setColour (border());
            g.fillRect (40, height - 1, width - 40, 1);
        }

        void listBoxItemClicked (int row, const juce::MouseEvent& e) override
        {
            if (! juce::isPositiveAndBelow (row, entries.size()))
                return;

            const auto en = entries[row];

            // Cestino: primo tocco arma, secondo cancella.
            if (isDeletable (en) && e.x >= list.getVisibleRowWidth() - kDeleteWidth)
            {
                if (pendingDeleteRow == row)
                {
                    const bool ok = en.file.deleteFile();
                    if (owner.onLog)
                        owner.onLog (ok ? "--- Deleted " + en.file.getFullPathName() + " ---"
                                        : "--- Could not delete " + en.file.getFullPathName() + " ---");
                    refresh();
                }
                else
                {
                    pendingDeleteRow = row;
                    list.repaint();
                }
                return;
            }

            if (pendingDeleteRow >= 0)
            {
                pendingDeleteRow = -1;
                list.repaint();
            }

            if (en.isFolder)
            {
                navigateTo (en.file);
                return;
            }

            if (mode == Mode::manage)
            {
                if (selectedFiles.contains (en.file))
                    selectedFiles.removeFirstMatchingValue (en.file);
                else
                    selectedFiles.add (en.file);

                updateExportButton();
                list.repaintRow (row);
            }
            else if (mode == Mode::open)
            {
                owner.hide();
                if (owner.onOpen)
                    owner.onOpen (en.file);
            }
            else
            {
                nameField.setText (en.file.getFileName(), juce::dontSendNotification);
                setReplacePending (false);
            }
        }

        //======================================================================
        static constexpr int kDeleteWidth = 64;
        static constexpr int navBarHeight = 52;

        CsdBrowserView& owner;
        Mode mode = Mode::open;
        juce::String title;
        juce::File root, current;
        juce::Array<Entry> entries;
        juce::Array<juce::File> selectedFiles; // manage
        int pendingDeleteRow = -1;
        bool replacePending = false;
        int bottomInset = 0; // page sheet: spazio sotto l'indicatore home

        juce::ListBox list;
        BarButton cancelButton, saveButton, backButton, otherButton;
        juce::TextEditor nameField;
        juce::Rectangle<int> pathLabelBounds;
    };

    //==========================================================================
    mutable Sheet sheet { *this };
    bool closing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CsdBrowserView)
};
