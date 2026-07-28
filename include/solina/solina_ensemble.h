/*
  solina_ensemble.h -- Control Circuit + Modulator Circuit I/II/III

  Original (Schaltplan, Sheet 015.0212, Bereich "CONTROL CIRCUIT *3*"):

      TREMOLO OSCILLATOR   741, 2M2 (101) + 1M Trimmer (71), 68n (100), 56K (97)
        -> LOW-PASS FILTER 741 ueber 5K6/680K/680K/680K und 6K8/56n/180n/5n6
        -> PHASE SHIFT     741, 47n (83), 820K (82), 10K (79)
        -> 22K/15uF auf die Modulatorschaltungen

      CHORUS OSCILLATOR    741, 1M8 (43) + 1M Trimmer (48), 680n (46), 56K (50)
        -> LOW-PASS FILTER 741 ueber 8K2/1M/1M/1M und 6K8/120n/680n/1M/56n
        -> PHASE SHIFT     741, 1M5 (16), 220n (13), 33K (12)
        -> PHASE INVERTER  741, 33K (11), 10K (6), 680n (2)
        -> C1 / C2 / C3 (220R je Ausgang)

      Aus den beiden Oszillatoren entstehen also drei um jeweils rund 120 Grad
      versetzte Steuersignale, die die Taktfrequenz der drei BBD-Leitungen
      (TCA350Y) in den Modulatorschaltungen I, II und III verstimmen.

      Der Zeitkonstanten-Vergleich bestaetigt die Bereiche:
        Tremolo 2M2 x 68n  = 150 ms  -> einige Hz
        Chorus  1M8 x 680n = 1,22 s  -> unter 1 Hz

  Modell (nach string-machine: SolinaChorus, LFO3PhaseDual, Delay3Phase):

      Zwei LFO-Reihen mit je drei Phasen (0, 120, 240 Grad) werden addiert.
      Drei Verzoegerungsleitungen mit 5 ms +/- 1 ms, davor eine dreistufige
      Anti-Alias-Kette. Der Ausgang wird mit Vorzeichen kombiniert:

          L = d1 + d2 - d3        R = d1 - d2 - d3

      Die BBD-Emulation aus string-machine (bbd_line.cpp) ist hier bewusst
      *nicht* uebernommen: sie rechnet zwei Filter fuenfter Ordnung mit
      std::complex<double> bei einem internen Takt von 2*185/5ms = 74 kHz je
      Leitung. Der Cortex-M33 des RP2350 hat nur eine Single-Precision-FPU.
*/

#ifndef SOLINA_ENSEMBLE_H
#define SOLINA_ENSEMBLE_H

#include "solina_defs.h"
#include "solina_dsp.h"

class SolinaEnsemble
{
public:
    void init(float sampleRate);
    void reset();

    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const     { return enabled_; }

    /* Trimmer des Control Circuit */
    void setTremoloRate(float hz);
    void setTremoloDepth(float d);   /* 0..1 */
    void setChorusRate(float hz);
    void setChorusDepth(float d);    /* 0..1 */

    /* Klangfarbe der Rekonstruktionsfilter, 1.0 = Schaltplanwerte */
    void setReconScale(float s);

    /* Stereobreite: 0 = mono wie das Original, 1 = maximal */
    void setWidth(float w);

    /* Ein Block: Monoeingang, Stereoausgang */
    void process(const float* in, float* outL, float* outR, int count);

    /* Phasenanzeige (im Original die Kontrolllampe) */
    float phase1() const { return ph1_[0]; }
    float phase2() const { return ph2_[0]; }

private:
    struct Line {
        float* buf = nullptr;
        int    size = 0;
        int    w = 0;
    };

    inline float readDelay(const Line& l, float delaySamples) const;

    float samplerate_ = 44100.0f;
    bool  enabled_ = true;

    /* Control Circuit */
    float ph1_[SOLINA_ENSEMBLE_LINES] = {};   /* Tremolo-Reihe */
    float ph2_[SOLINA_ENSEMBLE_LINES] = {};   /* Chorus-Reihe  */
    float inc1_ = 0.0f, inc2_ = 0.0f;
    float depth1_ = 0.5f, depth2_ = 0.5f;

    /* Modulator Circuit I/II/III */
    Line  line_[SOLINA_ENSEMBLE_LINES];
    float mem_[SOLINA_ENSEMBLE_LINES]
              [(int) (SOLINA_ENSEMBLE_MAX_MS * 0.001f * 48000.0f) + 4] = {};

    /* Anti-Alias-Kette vor den Leitungen */
    SolinaBiquad aa1_, aa2_, aa3_;

    /*
     * Rekonstruktionstiefpass hinter den Leitungen.
     *
     * Im Signalflussplan hat jede Modulatorschaltung hinter dem TCA350Y einen
     * eigenen "LOW-PASS FILTER TR4-5" -- eine getaktete Eimerkette braucht
     * beides, Anti-Alias davor und Rekonstruktion danach. Weil die
     * Ausgangsmatrix linear ist, ist ein Filter je Ausgangskanal rechnerisch
     * identisch mit je einem hinter jeder der drei Leitungen, kostet aber nur
     * zwei statt drei.
     */
    SolinaBiquad reconL1_, reconL2_, reconR1_, reconR2_;
    float reconScale_ = 1.0f;

    float width_ = 0.7f;

    float delayCenter_ = 0.0f;   /* in Abtastwerten */
    float delayVar_    = 0.0f;
};

#endif /* SOLINA_ENSEMBLE_H */
