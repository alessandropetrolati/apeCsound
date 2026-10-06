<CsoundSynthesizer>
<CsOptions>
-n -d -+rtmidi=NULL -M0 -m0d
</CsOptions>
<CsInstruments>
; Initialize the global variables.
ksmps = 32
nchnls = 2
0dbfs = 1

instr 1

    ; --- PARAMETRO DI CONTROLLO ---
    ; La prima riga (commentata) userebbe cabbageGetValue per ambienti Cabbage
    ; kFreeze cabbageGetValue "freeze"
    kFreeze chnget "FREEZE"

    ; --- PARAMETRI FFT ---
    ifftsize  = 1024                ; numero di campioni per finestra FFT
    iBIN      = ifftsize / 2 + 1    ; numero di bin frequenziali (N/2+1: include il bin di Nyquist -
                                     ; con ifftsize/2 soltanto gli array erano un bin piu' corti di
                                     ; quanto pvs2tab scrive davvero, causando una resintesi muta)
    iOverlap  = 4                   ; quante finestre si sovrappongono tra loro (4 = 75% di overlap)
    iHopSize  = ifftsize / iOverlap ; salto in campioni tra un'analisi e la successiva (256 campioni)

    ; --- PARAMETRI DI FINESTRA ---
    iwinsize  = ifftsize            ; la finestra di analisi ha la stessa dimensione dell'FFT
    iwinshape = 1                   ; tipo di finestra: von-Hann (attenua gli artefatti di bordo)

    ; --- SORGENTE AUDIO ---
    ;Sfile = "audio/1 Anechoic orchestra.wav"
    ain, ain2 ins;//diskin Sfile, 1, 0, 1 ; legge il file a velocità 1x, da posizione 0, con loop (1)

    ; --- ANALISI SPETTRALE (dominio fase-vocoder) ---
    ; pvsanal trasforma il segnale audio ain in un flusso PVS (Phase Vocoder Stream)
    ; che contiene ampiezza e frequenza per ciascun bin, aggiornato ogni iHopSize campioni
    fftin pvsanal ain, ifftsize, iHopSize, iwinsize, iwinshape

    ; --- ARRAY PER DATI SPETTRALI ---
    ; Arrays di ingresso: contengono lo spettro "vivo" del segnale analizzato
    kInMAG[]  init iBIN   ; ampiezze  dei bin in ingresso
    kInFREQ[] init iBIN   ; frequenze dei bin in ingresso

    ; Arrays di uscita: contengono lo spettro che andrà alla risintesi
    ; (uguale all'ingresso in modalità normale, congelato in modalità freeze)
    kOutMAG[]  init iBIN
    kOutFREQ[] init iBIN

    ; --- COPIA DEL FRAME SPETTRALE IN ARRAY k-rate ---
    ; pvs2tab estrae ampiezza e frequenza dal flusso PVS in due array separati
    ; kframe vale 1 ogni volta che un nuovo frame è disponibile (ogni iHopSize campioni)
    kframe pvs2tab kInMAG, kInFREQ, fftin

    ; --- CONTATORE DI CAMPIONI ---
    ; kCount accumula i campioni elaborati; quando raggiunge iHopSize
    ; significa che un nuovo frame spettrale è pronto per essere processato
    kCount init 0

    if kCount >= iHopSize then

        kIndex = 0
        until kIndex == iBIN do   ; scorre tutti i bin frequenziali (0 → 512)

            if kFreeze == 0 then
                ; MODALITÀ NORMALE: aggiorna l'uscita con i dati spettrali correnti
                kOutMAG[kIndex]  = kInMAG[kIndex]
                kOutFREQ[kIndex] = kInFREQ[kIndex]
            else
                ; MODALITÀ FREEZE: l'uscita mantiene i valori precedenti
                ; (le righe seguenti sono ridondanti ma esplicitano l'intenzione:
                ;  kOutMAG e kOutFREQ non vengono sovrascritti, lo spettro rimane congelato)
                kOutMAG[kIndex]  = kOutMAG[kIndex]
                kOutFREQ[kIndex] = kOutFREQ[kIndex]
            endif

            kIndex = kIndex + 1
        od

        kCount = 0  ; resetta il contatore dopo aver processato il frame

    endif

    ; --- RICOSTRUZIONE DEL FLUSSO PVS ---
    ; tab2pvs riassembla gli array di ampiezza e frequenza in un nuovo flusso PVS
    fOut1 tab2pvs kOutMAG, kOutFREQ, iHopSize, iwinsize, iwinshape

    ; --- RISINTESI AUDIO ---
    ; pvsynth ricostruisce il segnale audio dal flusso PVS tramite overlap-add
    aOut pvsynth fOut1

    out aOut, aOut  ; uscita stereo (lo stesso segnale su entrambi i canali)

    ; avanza il contatore di ksmps campioni ad ogni ciclo k-rate
    kCount = kCount + ksmps
endin

/*
Logica centrale del freeze in sintesi:

freeze OFF  →  kOut[] ← kIn[]   (spettro si aggiorna continuamente)
freeze ON   →  kOut[] ← kOut[]  (spettro rimane identico al frame precedente)
Il risultato sonoro è un pad infinito che cristallizza l'istante in cui freeze viene attivato, perché la risintesi continua a ricostruire sempre lo stesso contenuto armonico senza mai ricevere nuovi dati dall'analisi.
*/


</CsInstruments>
<CsScore>
;causes Csound to run for about 7000 years...
f0 z
;starts instrument 1 and runs it for a week
i1 0 [60*60*24*7]
</CsScore>
</CsoundSynthesizer>
