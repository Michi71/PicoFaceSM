/*
  solina_phaser.h -- Phaser

  ACHTUNG: Nicht im Original. Die ARP Solina von 1974 hat keinen Phaser --
  ihr Schaltplan kennt nur die drei Modulatorschaltungen des Ensembles.

  Der Nachbau von Behringer hat einen dazubekommen; im Handbuch
  (doc/BE071AAM8US1.pdf) steht unter "Modulation Section":

      Buttons  Modulation, phaser
      Controls Color, rate

  und auf der Rueckseite je eine Klinkenbuchse "Phaser in" und "Phaser out",
  der Phaser sitzt dort also als Einschleifpunkt hinter dem Ensemble. Genauso
  ist er hier verdrahtet: hinter den Modulatorschaltungen, vor Output
  Amplifier und Correction Filter.

  Aufbau: sechs Allpaesse erster Ordnung je Kanal, deren Eckfrequenz ein LFO
  zwischen 200 Hz und 1600 Hz durchstimmt, dazu eine Rueckkopplung ("Color")
  und eine feste Mischung halb trocken, halb nass. Der rechte Kanal laeuft
  90 Grad versetzt, damit die Bewegung im Stereobild bleibt.

  Die Allpass-Koeffizienten werden einmal je Block nachgefuehrt, nicht je
  Abtastwert -- bei LFO-Raten unter 10 Hz ist das reichlich, und es spart den
  Tangens im Abtasttakt.
*/

#ifndef SOLINA_PHASER_H
#define SOLINA_PHASER_H

#include "solina_defs.h"
#include "solina_dsp.h"

#define SOLINA_PHASER_STAGES 6

class SolinaPhaser
{
public:
    void init(float sampleRate);
    void reset();

    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const     { return enabled_; }

    void setRate(float hz);      /* SOLINA_PHASER_HZ_MIN .. MAX */
    void setColor(float c);      /* 0..1 -> Rueckkopplung       */

    /* Bearbeitet den Stereobus an Ort und Stelle. */
    void process(float* l, float* r, int count);

private:
    struct Channel {
        float x1[SOLINA_PHASER_STAGES];
        float y1[SOLINA_PHASER_STAGES];
        float fb;      /* letzter Ausgang fuer die Rueckkopplung */
        float phase;   /* LFO-Phase 0..1                         */
    };

    inline float runStages(Channel& c, float in, float a1) const;

    float samplerate_ = 44100.0f;
    bool  enabled_ = false;

    float inc_ = 0.0f;        /* LFO-Schritt je Abtastwert */
    float feedback_ = 0.0f;

    Channel ch_[2] = {};
};

#endif /* SOLINA_PHASER_H */
