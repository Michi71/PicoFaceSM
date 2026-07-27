/*
  PicoFaceSM -- ARP Solina String Ensemble

  Nachbau nach dem Original-Schaltplan (doc/ARP Solina Schematics.pdf,
  Sheet 015.0214 "Signal Flow Diagram" und Sheet 015.0212 "Schematic
  Diagram"), DSP-Modelle angelehnt an string-machine von Jean-Pierre Cimalando
  (string-machine/, Boost Software License 1.0), das seinerseits auf einem
  Modell von Peter Whiting beruht.

  Signalfluss des Originals:

    Master Oscillator (SAA1004) + Tuning
      -> Divider Circuit: 9x SAJ110 Teiler -> Sawtooth Circuits
      -> Gate Circuit: 10x TDA470 Torschaltungen (pro Taste 4' und 8')
         + Sustain Circuits
         -> Gate Output Circuit  --> VIOLA (8')    / VIOLIN (4')
         -> Formant Circuit TR5  --> TRUMPET (8')  / HORN (4')
      -> Bass Circuit: Low-Tone Selection -> Clipper -> Bass Sustain
         -> CELLO (8') / CONTRA BASS (16') -> Low-Pass
      -> Register Circuit -> VCA -> Low-Pass
      -> Modulator Circuit I/II/III (je TCA350Y BBD)
      -> Output Amplifier -> Correction Filter -> Out

    Control Circuit: Tremolo-Oszillator (schnell) und Chorus-Oszillator
    (langsam), je ueber Tiefpass, Phase Shift und Inverter auf die drei
    Modulatorschaltungen C1/C2/C3 verteilt.

  Die Solina ist kein Polysynth, sondern eine Orgel mit Frequenzteilern.
  Alle Toene stammen aus einem Master-Oszillator und sind phasenstarr
  gekoppelt; es gibt keine Verstimmung zwischen Stimmen. Die Register sind
  Filterabgriffe, keine Wellenformen. Die gesamte Bewegung im Klang kommt
  aus dem Ensemble.
*/

#ifndef SOLINA_DEFS_H
#define SOLINA_DEFS_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------------ */
/* Host-/Target-Umschaltung (analog PicoFaceCP / PicoFaceYC)                 */
/* ------------------------------------------------------------------------ */
#ifdef SOLINA_HOST_BUILD
#ifndef PICO_AUDIO_I2S_BUFFERS_PER_CHANNEL
#define PICO_AUDIO_I2S_BUFFERS_PER_CHANNEL 3
#endif
#ifndef SAMPLES_PER_BUFFER
#define SAMPLES_PER_BUFFER 16
#endif
#else
#include "audio_subsystem.h"
#endif

#define I2S_BUFFERS         PICO_AUDIO_I2S_BUFFERS_PER_CHANNEL
#define I2S_BUFFER_WORDS    SAMPLES_PER_BUFFER
#define SAMPLING_RATE       (44100)

#define SOLINA_BLOCK        I2S_BUFFER_WORDS

/* ------------------------------------------------------------------------ */
/* Klaviatur                                                                 */
/*                                                                           */
/* Das Original hat 49 Tasten. Der Schaltplan (Panel A) teilt sie in fuenf   */
/* Gruppen auf, die im Gate Output Circuit jeweils ein eigenes RC-Glied      */
/* sehen -- also eine Tastaturteilung der Klangfarbe. Diese Struktur wird    */
/* hier uebernommen: Filter sitzen pro Gruppe hinter der Summenschiene, nicht*/
/* pro Ton. Das ist zugleich schaltungstreu und um Groessenordnungen         */
/* billiger als ein Filtersatz je Stimme.                                    */
/* ------------------------------------------------------------------------ */
#define SOLINA_KEY_FIRST    36      /* C2 */
#define SOLINA_KEY_LAST     84      /* C6 -- 49 Tasten */
#define SOLINA_NGROUPS      5
#define SOLINA_KEYS_PER_GROUP 10

/* Bass-Sektion: Low-Tone Selection Circuit deckt die unteren zwei Oktaven ab
 * und arbeitet mit tiefster-Ton-Prioritaet (ein Ton). */
#define SOLINA_BASS_LAST    59      /* B3 */

/* Maximal gleichzeitig klingende Tasten (Divide-down kennt keine Stimmen-
 * begrenzung; das hier ist nur die Groesse der Aktivliste). */
#ifndef SOLINA_MAX_ACTIVE_KEYS
#define SOLINA_MAX_ACTIVE_KEYS 49
#endif

/* ------------------------------------------------------------------------ */
/* Ensemble (Modulator Circuit I/II/III)                                     */
/*                                                                           */
/* Drei BBD-Leitungen TCA350Y. Die Laufzeit liegt bei etwa 5 ms und wird um  */
/* +/- 1 ms moduliert (Kalibrierung aus string-machine, Delay3Phase.cpp).    */
/* Der Ausgang wird mit Vorzeichen kombiniert:                               */
/*     L = d1 + d2 - d3      R = d1 - d2 - d3                                */
/* ------------------------------------------------------------------------ */
#define SOLINA_ENSEMBLE_LINES     3
#define SOLINA_ENSEMBLE_DELAY_MS  5.0f
#define SOLINA_ENSEMBLE_VAR_MS    1.0f
#define SOLINA_ENSEMBLE_MAX_MS    8.0f   /* Puffergroesse je Leitung */

/* Control Circuit: Frequenzbereiche der beiden Steueroszillatoren.
 * Aus den Bauteilwerten des Schaltplans:
 *   Tremolo: 2M2 + 1M Trimmer, 68n   -> einige Hz
 *   Chorus : 1M8 + 1M Trimmer, 680n  -> unter 1 Hz
 * Die Grenzen decken sich mit string-machine (LFO3PhaseDual.dsp). */
#define SOLINA_TREMOLO_HZ_MIN   3.0f
#define SOLINA_TREMOLO_HZ_MAX   9.0f
#define SOLINA_CHORUS_HZ_MIN    0.3f
#define SOLINA_CHORUS_HZ_MAX    0.9f

/* Anti-Alias-Kette vor den Verzoegerungsleitungen
 * (string-machine, Delay3PhaseDigital.dsp) */
#define SOLINA_AA_F1  9561.0f   /* midikey2hz(122.3) */
#define SOLINA_AA_Q1  1.4706f   /* 1/(2-2*0.66)      */
#define SOLINA_AA_F2  9561.0f
#define SOLINA_AA_Q2  1.4706f
#define SOLINA_AA_F3  5751.0f   /* midikey2hz(113.5) */
#define SOLINA_AA_Q3  1.0870f   /* 1/(2-2*0.54)      */

/* Rekonstruktionsfilter hinter den Verzoegerungsleitungen.
 *
 * Modulator Circuit I (doc/StringEnsemble_Schematics-0275.pdf, Seite 4) hat
 * hinter dem BBD ORB 33 ZWEI kaskadierte aktive Tiefpaesse:
 *   Stufe 1  TR5 BC169B, 22K(36)/22K(52)/1K(51), 8n2(37) und 47p(44)
 *   Stufe 2  TR4 BC169B, 22K(48)/1K(45),         2n7(46) und 560p(47)
 * danach erst der Pegeltrimmer 2K2(39) und die Summierung.
 *
 * Die Eckfrequenzen sind aus den Bauteilwerten geschaetzt (Sallen-Key,
 * f = 1/(2*pi*R*sqrt(C1*C2)) mit R = 22K) -- nicht aus einer durchgerechneten
 * Uebertragungsfunktion. Die Transistorstufen sind im Scan nicht vollstaendig
 * verfolgbar. Ueber den Parameter "Ens Tone" nachstellbar. */
#define SOLINA_RECON_F1 11653.0f
#define SOLINA_RECON_F2  5883.0f
#define SOLINA_RECON_Q   0.7071f

/* ------------------------------------------------------------------------ */
/* Endstufe                                                                  */
/*                                                                           */
/* Schwelle der weichen Begrenzung. Unterhalb -3,1 dBFS ist die Kennlinie    */
/* linear; die Werksprogramme liegen im normalen Spiel weit darunter und     */
/* werden gar nicht angefasst. Erst dichte Cluster laufen in die Begrenzung, */
/* statt an der int16-Grenze hart abgeschnitten zu werden.                   */
/* ------------------------------------------------------------------------ */
#define SOLINA_CLIP_THRESHOLD 0.70f

/* ------------------------------------------------------------------------ */
/* Stimmung                                                                  */
/* ------------------------------------------------------------------------ */
#define SOLINA_A4_HZ        440.0f
/* Frequenz von MIDI-Note 0 (C-1) */
#define SOLINA_NOTE0_HZ     8.1757989157f

#endif /* SOLINA_DEFS_H */
