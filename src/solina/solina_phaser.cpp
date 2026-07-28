/*
  solina_phaser.cpp -- Phaser (Zutat des Behringer-Nachbaus, nicht im Original)
*/

#include "solina/solina_phaser.h"

#include <math.h>
#include <string.h>

void SolinaPhaser::init(float sampleRate)
{
    SolinaSineTable::init();

    samplerate_ = sampleRate;
    setRate(SOLINA_PHASER_HZ_MIN);
    setColor(0.5f);
    reset();
}

void SolinaPhaser::reset()
{
    memset(ch_, 0, sizeof(ch_));
    /* Der rechte Kanal laeuft eine Viertelperiode versetzt -- so bewegt sich
     * die Kerbenlage im Stereobild statt in der Mitte. */
    ch_[0].phase = 0.0f;
    ch_[1].phase = 0.25f;
}

void SolinaPhaser::setRate(float hz)
{
    if (hz < SOLINA_PHASER_HZ_MIN) hz = SOLINA_PHASER_HZ_MIN;
    if (hz > SOLINA_PHASER_HZ_MAX) hz = SOLINA_PHASER_HZ_MAX;
    inc_ = hz / samplerate_;
}

void SolinaPhaser::setColor(float c)
{
    if (c < 0.0f) c = 0.0f;
    if (c > 1.0f) c = 1.0f;
    /* Bis 0,7 -- darueber wird die Rueckkopplung zickig und pfeift. */
    feedback_ = c * 0.7f;
}

/* Allpass erster Ordnung, kaskadiert:  y = a1*x + x1 - a1*y1 */
inline float SolinaPhaser::runStages(Channel& c, float in, float a1) const
{
    float x = in;
    for (int s = 0; s < SOLINA_PHASER_STAGES; ++s)
    {
        const float y = a1 * x + c.x1[s] - a1 * c.y1[s];
        c.x1[s] = x;
        c.y1[s] = y;
        x = y;
    }
    return x;
}

void SolinaPhaser::process(float* l, float* r, int count)
{
    if (!enabled_)
    {
        /* Bei abgeschaltetem Phaser laeuft nur der LFO weiter, damit beim
         * Einschalten kein Sprung entsteht. */
        for (int i = 0; i < count; ++i)
            for (int k = 0; k < 2; ++k)
            {
                ch_[k].phase += inc_;
                if (ch_[k].phase >= 1.0f) ch_[k].phase -= 1.0f;
            }
        return;
    }

    float* buf[2] = { l, r };

    for (int k = 0; k < 2; ++k)
    {
        Channel& c = ch_[k];

        /*
         * Eckfrequenz einmal je Block aus der LFO-Mitte bestimmen. Der
         * Durchlauf geht exponentiell von 200 Hz bis 1600 Hz, also drei
         * Oktaven -- der uebliche Bereich eines Phasers.
         */
        const float mid = c.phase + inc_ * (float) count * 0.5f;
        const float lfo = SolinaSineTable::lookup(mid - (float) ((int) mid));
        const float oct = (lfo + 1.0f) * 0.5f;          /* 0..1 */
        float fc = SOLINA_PHASER_F_MIN
                   * powf(SOLINA_PHASER_F_MAX / SOLINA_PHASER_F_MIN, oct);

        const float nyq = 0.45f * samplerate_;
        if (fc > nyq) fc = nyq;

        const float t  = tanf((float) M_PI * fc / samplerate_);
        const float a1 = (t - 1.0f) / (t + 1.0f);

        float* p = buf[k];
        for (int i = 0; i < count; ++i)
        {
            const float dry = p[i];
            const float wet = runStages(c, dry + c.fb * feedback_, a1);
            c.fb = wet;

            /* Halb trocken, halb nass -- so entstehen die Kerben ueberhaupt. */
            p[i] = 0.5f * dry + 0.5f * wet;

            c.phase += inc_;
            if (c.phase >= 1.0f) c.phase -= 1.0f;
        }
    }
}
