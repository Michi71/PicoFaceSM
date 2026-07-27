/*
  solina_keyboard.h -- Manual Circuit + Gate Circuit + Sustain Circuits

  Original (Schaltplan, Sheet 015.0212):
      Zehn TDA470 liefern je Taste ein 4'- und ein 8'-Tor. Die "Sustain
      Circuits" laden bzw. entladen einen Kondensator ueber die Frontplatten-
      regler CRESCENDO (Anstieg) und SUSTAIN (Abfall); das Ergebnis steuert
      das Tor. Es gibt also keine Stimmenzuteilung -- jede Taste hat ihr
      eigenes Tor, alle 49 koennen gleichzeitig klingen.

      Die Toraussgaenge laufen auf fuenf Sammelschienen, je eine pro
      Tastaturgruppe (Panel A). Jede Gruppe sieht im Gate Output Circuit ihr
      eigenes RC-Glied -- eine Tastaturteilung der Klangfarbe. Deshalb wird
      hier pro Gruppe und Fusslage summiert, nicht pro Ton.

      Der Bass Circuit hat eine eigene "Low-Tone Selection" mit
      tiefster-Ton-Prioritaet und eine eigene Sustain-Schaltung.
*/

#ifndef SOLINA_KEYBOARD_H
#define SOLINA_KEYBOARD_H

#include "solina_defs.h"
#include "solina_divider.h"

class SolinaKeyboard
{
public:
    void init(float sampleRate);
    void reset();

    void noteOn(int note);
    void noteOff(int note);
    void allNotesOff();
    void setSustainPedal(bool on);

    /* Frontplatte: CRESCENDO = Anstiegszeit, SUSTAIN = Abfallzeit (Sekunden) */
    void setCrescendo(float seconds);
    void setSustain(float seconds);

    /*
     * Rendert einen Block.
     *   bus8/bus4   [Gruppe][Sample] -- Summenschienen des Manuals
     *   bass8/bass16              -- Bass Circuit, monophon
     */
    void process(SolinaDivider& divider,
                 float bus8[SOLINA_NGROUPS][SOLINA_BLOCK],
                 float bus4[SOLINA_NGROUPS][SOLINA_BLOCK],
                 float* bass8, float* bass16,
                 int count);

    bool anyActive() const { return activeCount_ > 0 || bassActive_; }

    static int groupOf(int note)
    {
        int g = (note - SOLINA_KEY_FIRST) / SOLINA_KEYS_PER_GROUP;
        if (g < 0) g = 0;
        if (g >= SOLINA_NGROUPS) g = SOLINA_NGROUPS - 1;
        return g;
    }

    /* Mittenfrequenz einer Tastaturgruppe (fuer die Filterabstimmung) */
    static float groupCenterHz(int g);

private:
    struct Key {
        uint8_t note;
        uint8_t group;
        bool    held;
        bool    sustained;
        float   env;
    };

    void   retireFinished();
    int    findActive(int note) const;
    int    releaseCandidate() const;
    int    lowestHeldBassNote() const;

    float  samplerate_ = 44100.0f;
    float  attackCoef_ = 0.01f;
    float  releaseCoef_ = 0.001f;
    float  crescendo_ = 0.06f;
    float  sustainTime_ = 0.30f;
    bool   pedal_ = false;

    Key    active_[SOLINA_MAX_ACTIVE_KEYS] = {};
    int    activeCount_ = 0;

    /* Bass Circuit: ein Ton mit tiefster-Ton-Prioritaet */
    bool   bassActive_ = false;
    int    bassNote_ = -1;
    float  bassEnv_ = 0.0f;

    /* Tastenzustand fuer die Tiefsten-Ton-Auswahl */
    bool   keyHeld_[128] = {};
};

#endif /* SOLINA_KEYBOARD_H */
