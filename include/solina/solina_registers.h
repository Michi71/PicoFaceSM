/*
  solina_registers.h -- Gate Output Circuit, Formant Circuit, Bass Circuit,
                        Register Circuit

  Original (Schaltplan, Sheet 015.0214 und 015.0212):

      Die Torausgaenge laufen auf zwei Wege:

        Gate Output Circuit   -> VIOLA (8')   / VIOLIN (4')
          Je Tastaturgruppe ein eigenes RC-Glied (10K Reihe, 10K nach Masse,
          C = 5n6 / 10n / 22n / 47n / ...), danach TR4 und ein
          Formungsnetzwerk 47n-22K-1n-18K.

        Formant Circuit TR5   -> TRUMPET (8') / HORN (4')
          Netzwerk aus 120n/47n/100K/220R um TR5, deutlich tiefer abgestimmt
          und ohne die Hoehenanhebung -- daher der hohle, blaeserartige Ton.

        Bass Circuit          -> CELLO (8')   / CONTRA BASS (16')
          Low-Tone Selection -> Clipper (709) -> Bass Sustain Voltage Circuit
          -> Tiefpass TR1.

  Modell:
      Die Filter sitzen wie im Original *hinter* der Summenschiene und nicht
      pro Ton -- aber pro Tastaturgruppe, weil das Original je Gruppe ein
      eigenes RC-Glied hat. Das ergibt die Tastaturteilung der Klangfarbe
      bei einem Bruchteil der Rechenzeit eines Filtersatzes je Stimme.

      Die Eckfrequenzen sind relativ zur Mittenfrequenz der jeweiligen
      Gruppe angesetzt. Die Verhaeltnisse stammen aus string-machine
      (StringFilters, dort tongenau statt gruppenweise); sie sind dort gegen
      das Original abgehoert worden. Die Werte sind ueber setTone() /
      setFormant() nachjustierbar.
*/

#ifndef SOLINA_REGISTERS_H
#define SOLINA_REGISTERS_H

#include "solina_defs.h"
#include "solina_dsp.h"

class SolinaRegisters
{
public:
    void init(float sampleRate);
    void reset();

    /* Registerschalter der Frontplatte */
    void setViola(bool on)      { viola_ = on; }
    void setViolin(bool on)     { violin_ = on; }
    void setTrumpet(bool on)    { trumpet_ = on; }
    void setHorn(bool on)       { horn_ = on; }
    void setCello(bool on)      { cello_ = on; }
    void setContrabass(bool on) { contrabass_ = on; }

    void setBassVolume(float v) { bassVolume_ = v; }

    /*
     * Abstimmung, jeweils in Halbtoenen relativ zur Gruppenmitte.
     * Vorgaben aus string-machine/plugins/string-machine/StringMachineShared.cpp
     */
    void setTone(float lowpassSemis, float highpassSemis,
                 float shelfSemis, float shelfDb);
    void setFormant(float lowpassSemis);
    void setShaper(float amount);

    /*
     * Ein Block.
     *   bus8/bus4  Summenschienen je Tastaturgruppe
     *   bass8/16   Bass Circuit
     *   out        Register Circuit, Monosumme
     */
    void process(const float bus8[SOLINA_NGROUPS][SOLINA_BLOCK],
                 const float bus4[SOLINA_NGROUPS][SOLINA_BLOCK],
                 const float* bass8, const float* bass16,
                 float* out, int count);

private:
    void updateCutoffs();

    struct GroupFilters {
        /* Gate Output Circuit: Tiefpass, Hochpass, Hoehenanhebung */
        SolinaLPF1   stringLp8, stringLp4;
        SolinaHPF1   stringHp8, stringHp4;
        SolinaBiquad stringShelf8, stringShelf4;
        /* Formant Circuit: nur Tiefpass */
        SolinaLPF1   brassLp8, brassLp4;
    };

    float samplerate_ = 44100.0f;

    GroupFilters grp_[SOLINA_NGROUPS];
    SolinaShaper shaper_;

    /* Bass Circuit */
    SolinaLPF1   bassLp_;
    SolinaDCBlock bassDc_;

    bool viola_ = true, violin_ = false;
    bool trumpet_ = false, horn_ = false;
    bool cello_ = false, contrabass_ = false;
    float bassVolume_ = 0.8f;

    /*
     * Pegelabgleich der Register.
     *
     * Im Original erledigen das die Widerstaende am Registerschalter
     * (100K an H8/H12/H9 im Formant Circuit, das Netzwerk am Bass Circuit).
     * Ohne den Abgleich liegt der Formant-Zweig rund 14 dB und der Bass-Zweig
     * rund 8 dB ueber dem Streicher-Zweig, weil dort kein Hochpass sitzt.
     * Gemessen an einem Vierklang, siehe README.
     */
    static constexpr float kGainString = 1.00f;
    static constexpr float kGainBrass  = 0.19f;   /* -14,4 dB */
    static constexpr float kGainBass   = 0.38f;   /*  -8,4 dB */

    float toneLpSemis_ = 5.2f;
    float toneHpSemis_ = 12.2f;
    float shelfSemis_  = 24.8f;
    float shelfDb_     = 6.0f;
    float formantSemis_ = 16.4f;
};

#endif /* SOLINA_REGISTERS_H */
