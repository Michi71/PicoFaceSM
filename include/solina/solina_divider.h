/*
  solina_divider.h -- Master Oscillator Circuit + Divider Circuit

  Original (Schaltplan, Sheet 015.0212):
      Master Oscillator TR2/SAA1004 mit Tuning-Trimmer erzeugt die oberste
      Oktave (12 Halbtoene). Neun SAJ110 halbieren daraus die tieferen
      Oktaven. Die nachgeschalteten "Sawtooth Circuits" formen aus den
      Rechtecken Saegezaehne.

  Modell:
      Zwoelf Phasenakkumulatoren, je einer pro Tonklasse, laufen mit der
      Frequenz der *untersten* Oktave. Jede hoehere Oktave entsteht durch
      Linksschieben des Akkumulators -- das ist bitgenau dasselbe wie eine
      Teilerkette, nur rueckwaerts betrachtet, und garantiert die
      Phasenstarrheit aller Oktaven derselben Tonklasse.

      Genau daraus lebt der Solina-Klang: zwischen zwei gegriffenen Toenen
      gibt es keinerlei Schwebung. Alle Bewegung kommt aus dem Ensemble.
*/

#ifndef SOLINA_DIVIDER_H
#define SOLINA_DIVIDER_H

#include "solina_defs.h"
#include "solina_dsp.h"

class SolinaDivider
{
public:
    void init(float sampleRate)
    {
        samplerate_ = sampleRate;
        setTune(0.0f);
        reset();
    }

    void reset()
    {
        /* Alle Teiler starten phasengleich -- wie nach dem Einschalten,
         * wenn die Teilerkette aus einem Zaehlerreset laeuft. */
        for (int pc = 0; pc < 12; ++pc)
            acc_[pc] = 0;
    }

    /* Master Oscillator Tuning, in Halbtoenen */
    void setTune(float semitones)
    {
        tune_ = semitones;
        const float ratio = powf(2.0f, semitones / 12.0f);

        for (int pc = 0; pc < 12; ++pc)
        {
            /* Frequenz der MIDI-Note pc (unterste Oktave, C-1..B-1) */
            const float f = SOLINA_NOTE0_HZ * powf(2.0f, ((float) pc) / 12.0f)
                            * ratio;
            stepLow_[pc] = f / samplerate_;
            inc_[pc] = (uint32_t) (stepLow_[pc] * 4294967296.0f + 0.5f);
        }
    }

    float tune() const { return tune_; }

    /* Ein Abtastschritt der gesamten Teilerkette */
    inline void tick()
    {
        for (int pc = 0; pc < 12; ++pc)
            acc_[pc] += inc_[pc];
    }

    /* Phase 0..1 der MIDI-Note */
    inline float phase(int note) const
    {
        const int pc  = note - 12 * (note / 12);
        const int oct = note / 12;
        return (float) (acc_[pc] << oct) * (1.0f / 4294967296.0f);
    }

    /* Schrittweite (Perioden pro Abtastwert) der MIDI-Note */
    inline float step(int note) const
    {
        const int pc  = note - 12 * (note / 12);
        const int oct = note / 12;
        return stepLow_[pc] * (float) (1u << oct);
    }

private:
    float    samplerate_ = 44100.0f;
    float    tune_ = 0.0f;
    uint32_t acc_[12] = {};
    uint32_t inc_[12] = {};
    float    stepLow_[12] = {};
};

#endif /* SOLINA_DIVIDER_H */
