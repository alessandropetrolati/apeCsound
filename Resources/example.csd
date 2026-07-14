<CsoundSynthesizer>
<CsOptions>
; Niente "-odac" qui: CsoundAudioProcessor genera audio solo tramite
; processBlock() (spout letto a mano), mai da un device aperto
; direttamente da Csound. Questo file e' solo un riferimento: il
; contenuto di default gia' caricato nell'editor all'avvio vive in
; CsoundAudioProcessor::defaultCsdText() (Source/PluginProcessor.cpp).
</CsOptions>
<CsInstruments>

sr     = 44100
ksmps  = 32
nchnls = 2
0dbfs  = 1

instr 1
  aenv linen 0.3, 0.05, p3, 0.1
  asig poscil aenv, 440
  outs asig, asig
endin

</CsInstruments>
<CsScore>
i 1 0 3
e
</CsScore>
</CsoundSynthesizer>
