# apeCsound 1.0.5 - release checklist

Done in the repository (this session):
- [x] Version 1.0.5 in apeCsound.jucer, INSTALLERS/apeCsound Win.iss, INSTALLERS/apeCsoun d macOS.pkgproj
- [x] CHANGELOG.txt consolidated (1.0.5 section), README.md, in-app Guide (Channels, Monitor attributes, live tables, Csound 7 notes, shortcuts, Cabbage import rules)
- [x] Resources/Examples.zip rebuilt (232 files: Cabbage 126, CsoundQT 56, Test 10, ape 2, README_Examples.md); kExamplesVersion = 8
- [x] New sources added to the .jucer: CsoundCabbageHelpers.h, CsoundLayoutHelpers.h, CsoundSyntaxHelpers.h, CsoundMonitorLayout.h, CsoundChannelDiagnostics.h, CsoundChannelsPanel.{h,cpp}, Tests/CsoundUnitTests.{h,cpp}
- [x] Syntax check of every .cpp (Linux, g++ -fsyntax-only) and unit tests: 2143 assertions, 0 failures

To do on the Mac:
1. Projucer: open apeCsound.jucer, Save (resave) so the Xcode / VS / Linux projects get the new files.
2. Xcode: Product > Clean Build Folder, build Release for Standalone, AU, AUv3, VST3 (macOS) and the iOS targets.
3. Run the self-tests once in a Debug build (Main menu > Config > Run Self-Tests): expect "2143 passed, 0 failed" (in-app suites included).
4. Smoke test (Standalone + REAPER VST3 + Logic AU):
   - Load Test/EvaluateTest.csd, Apply, Shift+Return on instr 2, Cmd+E "i 2 0 1".
   - Test/MonitorStressTest.csd: Monitor cards (dB, spark, int, groups), Show Graphs live tables, Show Channels (Cmd+4).
   - Cabbage/Instruments/Synths/Bassline_TB-303.csd: load (fresh import), groups, Play toggle, Channels shows no "read, nobody writes".
   - Test/MidiOutTest.csd with a synth after apeCsound in REAPER.
   - VoiceOver (Cmd+F5): toolbar buttons, file menu, a parameter card, the XY pad in Test/XYPadTest.csd, the code editor.
   - Open a project saved with 1.0.4: parameters, snapshots and the linked file come back (AU: a Menu automation lane must be re-assigned, see CHANGELOG).
5. Installers: INSTALLERS/build_release_mac.command, then notarize_pkg.command; Windows: Inno Setup with apeCsound Win.iss; Linux: scripts/build_csound_static_linux.sh + the Projucer Makefile.
6. git: commit, tag v1.0.5, push; attach the installers to the GitHub release with the 1.0.5 section of CHANGELOG.txt as notes.
7. App Store Connect (iOS/AUv3): bump the build number in Xcode, archive, upload, release notes from CHANGELOG (see AppStoreConnect.md).
