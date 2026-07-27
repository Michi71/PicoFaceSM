# PicoFaceSM — ARP Solina String Ensemble

Emulation der ARP Solina String Ensemble auf derselben Hardware-/Software-Basis
wie [PicoFaceRD](https://github.com/Michi71/PicoFaceRD),
[PicoFaceCP](https://github.com/Michi71/PicoFaceCP),
[PicoFaceDX](https://github.com/Michi71/PicoFaceDX) und
[PicoFaceYC](https://github.com/Michi71/PicoFaceYC).

**Stand:** Engine + Renderer + Host-Testprogramm. RP2350-Anbindung folgt.

Die Klangerzeugung folgt dem Original-Schaltplan
(`doc/StringEnsemble_Schematics-0275.pdf`, Seite 4, Sheet 015.0212) und dem
Signalflussplan (`doc/ARP Solina Schematics.pdf`, Sheet 015.0214):

```
Master Oscillator (SAA1004) + Tuning
  → Divider Circuit: 9× SAJ110 Teiler → Sawtooth Circuits
  → Gate Circuit: 10× TDA470 (pro Taste 4' und 8') + Sustain Circuits
     → Gate Output Circuit  → VIOLA (8')   / VIOLIN (4')
     → Formant Circuit TR5  → TRUMPET (8') / HORN (4')
  → Bass Circuit: Low-Tone Selection → Clipper → Bass Sustain
     → CELLO (8') / CONTRA BASS (16') → Low-Pass
  → Register Circuit → VCA → Low-Pass
  → Modulator Circuit I/II/III (je BBD ORB 33 + zweistufiger Tiefpass)
  → Output Amplifier → Correction Filter → Out

Control Circuit: Tremolo-Oszillator (schnell) und Chorus-Oszillator (langsam),
je über Tiefpass, Phase Shift und Inverter auf C1/C2/C3 verteilt.
```

Entscheidend ist: die Solina ist **kein Polysynth**, sondern eine Orgel mit
Frequenzteilern. Alle Töne stammen aus einem Master-Oszillator und sind
phasenstarr gekoppelt, es gibt keine Verstimmung zwischen Stimmen. Die sechs
Register sind Filterabgriffe, keine Wellenformen. Die gesamte Bewegung im
Klang kommt aus dem Ensemble.

## Herkunft der Quellen

Die Engine folgt dem Schaltplan in der Struktur; die DSP-Modelle für
Ensemble, Filter und Wellenformer sind an
[string-machine](https://github.com/jpcima/string-machine) von Jean-Pierre
Cimalando angelehnt (Boost Software License 1.0, Referenzbaum unter
`string-machine/`), das seinerseits auf einem Modell von Peter Whiting beruht.

| Datei | Original | Inhalt |
|---|---|---|
| `solina_divider.h` | Master Oscillator Circuit + Divider Circuit | 12 Phasenakkumulatoren, Oktaven durch Schieben — bitgenau phasenstarr |
| `solina_keyboard.{h,cpp}` | Manual + Gate Circuit + Sustain Circuits | Tor je Taste, RC-Hüllkurve, Summenschienen je Tastaturgruppe, Bass mit tiefster-Ton-Priorität |
| `solina_registers.{h,cpp}` | Gate Output + Formant + Bass Circuit | die sechs Register als Filterabgriffe |
| `solina_ensemble.{h,cpp}` | Control Circuit + Modulator I/II/III | Dual-LFO mit je drei Phasen, drei Delays 5 ms ± 1 ms, Ausgangsmatrix |
| `solina_dsp.h` | — | Ein-Pol-Filter, Biquad, Soft-Clipper, polyBLEP (aus string-machine) |
| `solina.{h,cpp}` | Register Circuit + Output Amplifier | Parameter, Programme, MIDI, Ausgangsstufe |

## Signalfluss

```
 12 Phasenakkumulatoren (Master + Teiler, phasenstarr)
        │
        ├─ je gedrückter Taste: 8' und 4' Sägezahn (polyBLEP) × Tor-Hüllkurve
        │        └─ summiert auf 5 Tastaturgruppen  ──► Klangfarbe folgt der Lage
        │
        ├─ Gate Output Circuit  (LP → HP → Höhenanhebung → Clipper)
        │     └─ Viola 8'  /  Violin 4'
        ├─ Formant Circuit      (LP)
        │     └─ Trumpet 8' /  Horn 4'
        └─ Bass Circuit         (tiefster Ton, Clipper, LP)
              └─ Cello 8'  /  Contrabass 16'
                     │
                     ▼
        Register Circuit → DC-Sperre
                     ▼
        Ensemble: 3 Delays 5 ms ± 1 ms, moduliert von
                  Tremolo-LFO (3–9 Hz) + Chorus-LFO (0,3–0,9 Hz),
                  je 0°/120°/240°
                  L = d1 + d2 − d3     R = d1 − d2 − d3
                     ▼
        Output Amplifier + Correction Filter → L/R
```

## Parameter

22 Parameter, jeweils `0.0 … 1.0`. Die ersten elf entsprechen genau der
Frontplatte (Behringer-Handbuch: „Buttons Contrabass, cello, viola, violin,
trumpet, horn / Controls Volume bass, crescendo, sustain, volume, tune"),
danach folgen die Trimmer des Control Circuit und die Filterabstimmung, die im
Original feste Bauteilwerte sind. Siehe `enum SolinaParam` in
`include/solina/solina.h`.

## Host-Test (macOS)

```bash
./test/build_solina.sh
./test/solina_test
```

Voraussetzung: PortMidi (`brew install portmidi`). Öffnet einen virtuellen
MIDI-Eingang `solina` und spielt über CoreAudio; die Tastenbelegung steht im
Kopf von `test/solina_test.cpp`.

## Polyphonie

Das Manual (Viola, Violin, Trumpet, Horn) ist **voll polyphon über alle 49
Tasten** — es gibt keine Stimmenzuteilung, jede Taste hat ihr eigenes Tor und
ihre eigene Hüllkurve, wie die zehn TDA470 im Original. Die Bass-Sektion
(Cello, Contrabass) ist **einstimmig mit Vorrang für den tiefsten Ton**, das
ist der Low-Tone Selection Circuit.

Ausklingende Tasten belegen ihren Platz, bis die Hüllkurve abgefallen ist.
Ist die Liste voll, wird die am weitesten abgeklungene ausklingende Taste
weiterverwendet (die Hüllkurve wird übernommen, der Pegel springt also
nicht). Sind wirklich alle 49 Tore von gehaltenen oder vom Pedal gehaltenen
Tasten belegt, bleibt eine weitere Taste stumm — das Original hat auch nur
49 Torschaltungen.

## Messwerte (Host, M4)

| | |
|---|---|
| Stimmung | A4 = 440,000 Hz, C4 = 261,626 Hz |
| Phasenstarrheit C2→C6 | < 5·10⁻⁷ (Rechengenauigkeit) |
| Aliasing (Sägezahn, ohne Registerfilter) | < −40 dB über den ganzen Umfang |
| Ruhelaufzeit Ensemble | 5,06 ms (Soll 5,00 ms + Gruppenlaufzeit der Anti-Alias-Kette) |
| L/R-Korrelation Ensemble | 0,42 |
| Pegel der acht Programme | −19,7 bis −8,6 dBFS, 20 Tasten gleichzeitig −4,5 dBFS |
| Alle 49 Tasten, jedes Programm | ≤ 0,0 dBFS (weiche Begrenzung greift) |
| Registerabgleich | alle sechs innerhalb 0,8 dB |
| Rechenzeit | 10 Tasten: 296× Echtzeit, 0,34 % eines Cores |

## Bewusste Abweichungen vom Original

1. **Die Filter sitzen pro Tastaturgruppe, nicht pro Ton.** Das Original hat im
   Gate Output Circuit je Gruppe ein eigenes RC-Glied (10K mit 5n6 / 10n / 22n /
   47n …), also eine Tastaturteilung der Klangfarbe. string-machine filtert
   tongenau. Die Gruppenlösung ist schaltungstreu und um Größenordnungen
   billiger.
2. **Die Eckfrequenzen sind relativ zur Gruppenmitte angesetzt**, mit den
   Verhältnissen aus string-machine (dort gegen das Original abgehört). Die
   Transistorstufen des Formant Circuit sind aus dem Scan nicht sicher genug
   auszuwerten, um daraus direkt Übertragungsfunktionen abzuleiten. Alle vier
   Werte sind über die Parameter `Tone LP/HP/Shelf` und `Formant` nachjustierbar.
3. **Die Registerpegel werden abgeglichen** (Formant −14,4 dB, Bass −8,4 dB
   gegen die Streicher). Im Original erledigen das die Widerstände am
   Registerschalter; ohne den Abgleich übersteuern die Blechregister.
4. **Kubische Interpolation statt linearer** in den Verzögerungsleitungen
   (string-machine: `de.fdelayltv(1, …)`). Messbar am Modulationsverhalten
   ändert das nichts; es ist nur die sauberere Variante und kostet kaum etwas.
5. **Tremolo-Tiefe auf 0,10 statt 0,3071.** Der Wert aus string-machine erzeugt
   bei 5,83 Hz rund **19 Cent** Tonhöhenhub — ein hörbares Vibrato statt eines
   Schwebens. Bei 0,10 sind es rund 6 Cent, in der Größenordnung der langsamen
   Reihe (5,7 Cent bei 0,58 Hz). Gemessen an einem Dauerton, RMS-Hüllkurve:

   | Tremolo-Tiefe | Tonhöhenhub | Hüllkurve langsam (<2 Hz) | schnell (>3 Hz) | Hub |
   |---|---|---|---|---|
   | 0,307 | 19,4 ct | 10 % | 76 % | 19,1 dB |
   | 0,120 |  7,6 ct | 20 % | 54 % | 13,5 dB |
   | 0,100 |  6,3 ct | — | — | — |
   | 0,000 |  0,0 ct | 24 % | 42 % | 10,8 dB |

   Der Rest der schnellen Bewegung ist der Kammfilter selbst: bei 5 ms
   Grundlaufzeit und ±1 ms Hub wandert ein Teilton mehrfach je LFO-Halbwelle
   durch die Kerben. Das macht das Original genauso.
6. **Rekonstruktionsfilter aus `doc/StringEnsemble_Schematics-0275.pdf`.**
   Modulator Circuit I hat hinter dem BBD (`ORB 33`) zwei kaskadierte aktive
   Tiefpässe — Stufe 1 mit 8n2/47p, Stufe 2 mit 2n7/560p, je 22K. Als
   Sallen-Key gerechnet (`f = 1/(2πR·√(C1C2))`, R = 22K) ergibt das rund
   11,7 kHz und 5,9 kHz. Die Transistorstufen sind nicht vollständig
   verfolgbar, die Werte sind also Schätzungen aus den Bauteilwerten. Hörbar
   messbar unterscheiden sie sich kaum von einem einzelnen 2-Pol bei 5,75 kHz,
   sie sind nur die korrektere Ordnung. Über den Parameter `Ens Tone`
   verstellbar. **Vorgabe ist 0,20** (7,7 / 3,9 kHz) statt der Schaltplanwerte
   bei 0,50 — nach Gehör gewählt, weil die hohen Teiltöne am schnellsten durch
   die Kammfilterkerben wandern und damit am meisten zur Unruhe beitragen.
   Energie über 4 kHz sinkt dadurch von 13 % auf 7,5 %.
7. **Keine BBD-Emulation.** Die aus string-machine (`bbd_line.cpp`) rechnet zwei
   Filter fünfter Ordnung mit `std::complex<double>` bei einem internen Takt von
   2·185/5 ms = 74 kHz je Leitung, also grob 16 Mio. Double-Operationen pro
   Sekunde. Der Cortex-M33 des RP2350 hat nur eine Single-Precision-FPU.
   Verwendet wird die digitale Variante desselben 3-Phasen-Delays.
8. **Anschlagdynamik wird ignoriert** — die Torschaltung des Originals kennt nur
   auf und zu.
9. **Weiche Begrenzung in der Endstufe** statt hartem Abschneiden an der
   `int16`-Grenze. Unterhalb −3,1 dBFS ist die Kennlinie exakt linear, darüber
   läuft sie asymptotisch gegen 1,0. Normales Spiel wird dadurch überhaupt
   nicht angefasst (0,000 % der Abtastwerte bei einem Akkord und bei 20
   Tasten), ein Cluster über alle 49 Tasten wird um höchstens 2 dB
   zurückgenommen, und selbst alle sechs Register mit beiden Lautstärkereglern
   am Anschlag bleiben bei 0,0 dBFS statt +10,2 dBFS. Der Output Amplifier des
   Originals begrenzt an seinen Versorgungsschienen ebenso weich.

## Lizenz

GPL v3. Die aus [string-machine](https://github.com/jpcima/string-machine)
übernommenen DSP-Modelle stehen unter der Boost Software License 1.0; der
Referenzbaum liegt unter `string-machine/`.
