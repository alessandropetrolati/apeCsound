<CsoundSynthesizer>
<CsOptions>
; Niente "-odac"/"-iadc" qui: CsoundAudioProcessor gestisce l'intero I/O
; audio da solo tramite processBlock() (spin scritto e spout letto a mano),
; mai da device aperti direttamente da Csound. Questo file e' solo un
; riferimento: il contenuto di default gia' caricato nell'editor all'avvio
; vive in CsoundAudioProcessor::defaultCsdText() (Source/PluginProcessor.cpp).
</CsOptions>
<CsInstruments>

; nchnls_i dichiara i canali di INGRESSO (bus Input del plugin, es.
; microfono con la Standalone, o sidechain in una DAW): letti nel .csd
; con l'opcode "inch". nchnls resta per i canali di USCITA.
sr       = 44100
ksmps    = 32
nchnls   = 2
nchnls_i = 2
0dbfs    = 1

instr 1
  aenv linen 0.3, 0.05, p3, 0.1
  asig poscil aenv, 440

  ; Audio in ingresso dal bus Input del plugin (microfono/segnale live):
  ; richiede che l'host (o il device audio della Standalone) abbia un
  ; ingresso attivo, altrimenti ainL/ainR restano a 0.
  ainL inch 1
  ainR inch 2

  outs asig + ainL, asig + ainR
endin

</CsInstruments>
<CsScore>
i 1 0 3600
e
</CsScore>
</CsoundSynthesizer>
