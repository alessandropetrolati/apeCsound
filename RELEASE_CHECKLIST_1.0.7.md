# apeCsound 1.0.7 - release checklist

(1.0.6 was skipped: the .jucer was already at 1.0.7.)

Done in the repository:
- [x] Version 1.0.7 in apeCsound.jucer, INSTALLERS/apeCsound Win.iss, INSTALLERS/apeCsoun d macOS.pkgproj (3 places)
- [x] CHANGELOG.txt: 1.0.7 section consolidated (Breaking changes, Buttons, Monitor, Editor, Layout, Import, Fixes, Guide, Examples)
- [x] README.md: Buttons, Monitor, Jump to Definition, Layout
- [x] In-app Guide: Buttons, Starting instruments with a button, Monitor (kinds, reading a card), Jump to Definition, Layout and settings, Parameters intro, shortcuts
- [x] AppStoreConnect.md: description (EN/IT) updated, "What's New" for 1.0.7 (EN 1263 chars, IT 1375 chars; description EN 2669, IT 2910 - all under 4000)
- [x] Resources/Examples.zip rebuilt (233 files: Cabbage 149, CsoundQT 68, Test 12, ape 3); kExamplesVersion = 12 (ButtonTest, CsoundQtButtons, MonitorTest)
- [x] New source added to the .jucer: Source/CsoundNavigation.h
- [x] Syntax check of every .cpp (g++ -fsyntax-only, Linux) and unit tests: 2354 passed, 0 failed (core suites; the in-app suites run in a Debug build)

To do on the Mac:
1. Projucer: open apeCsound.jucer, Save (resave) so the Xcode / VS / Linux projects pick up CsoundNavigation.h and version 1.0.7.
2. Xcode: Product > Clean Build Folder, build Release for Standalone, AU, AUv3, VST3 (macOS) and the iOS targets.
3. Debug build once: Main menu > Config > Run Self-Tests - expect 0 failures (in-app suites included: Button save/reload, editor layout in the plug-in state, CsoundQt buttons import).
4. Smoke test (Standalone, REAPER VST3, Logic AU, an iPad host such as AUM):
   - Test/ButtonTest.csd: Hold (momentary), Bell (EVENT ON), Pad (held note EVENT ON/OFF), Chord and Arp (code); record the buttons as automation and play it back.
   - Test/CsoundQtButtons.csd and CsoundQT/McCurdy Collection/PhysicalModels/dripwater.csd: import of event buttons (Single Drip held while pressed, Tap On/Off toggles).
   - Test/MonitorTest.csd: every card kind, CLIP (asig * 4), compact mode, Copy chn_k, "no change since Apply".
   - Jump to Definition: Cmd+click a variable / instrument / channel, "Back to line N", Cmd+Option+Left; on iPad select a name > Definition / Back.
   - Layout: resize, close Console, collapse a group, scroll the code, quit and reopen (standalone) / save and reopen the DAW project; Config > Reset Layout.
   - Open a project saved with 1.0.5: parameters, snapshots, linked file come back; Monitor cards of attribute-based channels change look (see CHANGELOG, Breaking changes).
5. Installers: INSTALLERS/build_release_mac.command, then notarize_pkg.command; Windows: Inno Setup with apeCsound Win.iss; Linux: scripts/build_csound_static_linux.sh + the Projucer Makefile.
6. git: commit, tag v1.0.7, push; GitHub release with the 1.0.7 section of CHANGELOG.txt as notes and the installers attached.
7. App Store Connect (iOS / AUv3):
   - Xcode: version 1.0.7 comes from the .jucer; set a new Build number (higher than the last upload).
   - Product > Archive (iOS target with the AUv3 extension), Validate, Distribute > App Store Connect.
   - App Store Connect: new version 1.0.7, select the build, paste "What's New in This Version (1.0.7)" (EN) and "Novità di questa versione (1.0.7)" (IT) from AppStoreConnect.md; update the description if you want the new bullets.
   - Screenshots: optional new ones of the Monitor cards and of a Button configuration (MODE / INIT / EVENT ON / OFF).
   - Submit for review.
