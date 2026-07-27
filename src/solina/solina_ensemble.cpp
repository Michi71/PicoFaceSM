/*
  solina_ensemble.cpp -- Control Circuit + Modulator Circuit I/II/III
*/

#include "solina/solina_ensemble.h"

#include <math.h>
#include <string.h>

void SolinaEnsemble::init(float sampleRate)
{
    samplerate_ = sampleRate;

    const int maxSamples =
        (int) (SOLINA_ENSEMBLE_MAX_MS * 0.001f * sampleRate) + 4;
    const int cap = (int) (sizeof(mem_[0]) / sizeof(float));
    const int size = (maxSamples < cap) ? maxSamples : cap;

    for (int l = 0; l < SOLINA_ENSEMBLE_LINES; ++l)
    {
        line_[l].buf  = mem_[l];
        line_[l].size = size;
        line_[l].w    = 0;
    }

    aa1_.init(sampleRate);
    aa2_.init(sampleRate);
    aa3_.init(sampleRate);
    aa1_.setLowpass(SOLINA_AA_F1, SOLINA_AA_Q1);
    aa2_.setLowpass(SOLINA_AA_F2, SOLINA_AA_Q2);
    aa3_.setLowpass(SOLINA_AA_F3, SOLINA_AA_Q3);

    reconL1_.init(sampleRate); reconL2_.init(sampleRate);
    reconR1_.init(sampleRate); reconR2_.init(sampleRate);
    setReconScale(reconScale_);

    delayCenter_ = SOLINA_ENSEMBLE_DELAY_MS * 0.001f * sampleRate;
    delayVar_    = SOLINA_ENSEMBLE_VAR_MS   * 0.001f * sampleRate;

    setTremoloRate(5.83f);
    setChorusRate(0.58f);

    reset();
}

void SolinaEnsemble::reset()
{
    for (int l = 0; l < SOLINA_ENSEMBLE_LINES; ++l)
    {
        memset(line_[l].buf, 0, sizeof(float) * (size_t) line_[l].size);
        line_[l].w = 0;

        /* Die drei Steuersignale stehen 120 Grad auseinander. */
        ph1_[l] = ((float) l) / (float) SOLINA_ENSEMBLE_LINES;
        ph2_[l] = ((float) l) / (float) SOLINA_ENSEMBLE_LINES;
    }

    aa1_.clear();
    aa2_.clear();
    aa3_.clear();
    reconL1_.clear(); reconL2_.clear();
    reconR1_.clear(); reconR2_.clear();
}

void SolinaEnsemble::setTremoloRate(float hz)
{
    if (hz < SOLINA_TREMOLO_HZ_MIN) hz = SOLINA_TREMOLO_HZ_MIN;
    if (hz > SOLINA_TREMOLO_HZ_MAX) hz = SOLINA_TREMOLO_HZ_MAX;
    inc1_ = hz / samplerate_;
}

void SolinaEnsemble::setChorusRate(float hz)
{
    if (hz < SOLINA_CHORUS_HZ_MIN) hz = SOLINA_CHORUS_HZ_MIN;
    if (hz > SOLINA_CHORUS_HZ_MAX) hz = SOLINA_CHORUS_HZ_MAX;
    inc2_ = hz / samplerate_;
}

void SolinaEnsemble::setTremoloDepth(float d)
{
    depth1_ = (d < 0.0f) ? 0.0f : ((d > 1.0f) ? 1.0f : d);
}

void SolinaEnsemble::setChorusDepth(float d)
{
    depth2_ = (d < 0.0f) ? 0.0f : ((d > 1.0f) ? 1.0f : d);
}

void SolinaEnsemble::setReconScale(float s)
{
    if (s < 0.25f) s = 0.25f;
    if (s > 4.0f)  s = 4.0f;
    reconScale_ = s;

    const float nyq = 0.45f * samplerate_;
    float f1 = SOLINA_RECON_F1 * s;
    float f2 = SOLINA_RECON_F2 * s;
    if (f1 > nyq) f1 = nyq;
    if (f2 > nyq) f2 = nyq;

    reconL1_.setLowpass(f1, SOLINA_RECON_Q);
    reconR1_.setLowpass(f1, SOLINA_RECON_Q);
    reconL2_.setLowpass(f2, SOLINA_RECON_Q);
    reconR2_.setLowpass(f2, SOLINA_RECON_Q);
}

/*
 * Kubische Interpolation (Catmull-Rom) statt der linearen aus string-machine
 * (de.fdelayltv(1, ...)).
 *
 * Bei einer schnell durchgestimmten Verzoegerung wandert der Lesezeiger
 * staendig durch den Bruchteilbereich. Lineare Interpolation wirkt dabei wie
 * ein Tiefpass, dessen Daempfung vom Bruchteil abhaengt -- die Hoehen werden
 * also im Takt der Modulation an- und abgeschwaecht. Genau das erzeugt die
 * unruhige Koernung, die bei dichtem Material und allen Registern auffaellt.
 */
inline float SolinaEnsemble::readDelay(const Line& l, float delaySamples) const
{
    float rp = (float) l.w - delaySamples;
    while (rp < 0.0f)
        rp += (float) l.size;

    const int   i1 = (int) rp;
    const float mu = rp - (float) i1;

    const int n = l.size;
    const int i0 = (i1 > 0)     ? (i1 - 1) : (n - 1);
    const int i2 = (i1 + 1 < n) ? (i1 + 1) : 0;
    const int i3 = (i2 + 1 < n) ? (i2 + 1) : 0;

    const float y0 = l.buf[i0], y1 = l.buf[i1];
    const float y2 = l.buf[i2], y3 = l.buf[i3];

    const float a = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c = 0.5f * (y2 - y0);

    return ((a * mu + b) * mu + c) * mu + y1;
}

void SolinaEnsemble::process(const float* in, float* outL, float* outR,
                             int count)
{
    if (!enabled_)
    {
        /* Der Ensemble-Schalter des Originals trennt die Modulatorschaltungen
         * ab; das Signal laeuft trocken durch. Die Leitungen laufen weiter,
         * damit beim Einschalten kein Sprung entsteht. */
        for (int i = 0; i < count; ++i)
        {
            const float x = aa3_.process(aa2_.process(aa1_.process(in[i])));
            for (int l = 0; l < SOLINA_ENSEMBLE_LINES; ++l)
            {
                Line& ln = line_[l];
                ln.buf[ln.w] = x;
                ln.w = (ln.w + 1 < ln.size) ? (ln.w + 1) : 0;

                ph1_[l] += inc1_; if (ph1_[l] >= 1.0f) ph1_[l] -= 1.0f;
                ph2_[l] += inc2_; if (ph2_[l] >= 1.0f) ph2_[l] -= 1.0f;
            }
            outL[i] = in[i];
            outR[i] = in[i];
        }
        return;
    }

    const float twoPi = 2.0f * (float) M_PI;

    for (int i = 0; i < count; ++i)
    {
        /* Anti-Alias-Kette vor den Leitungen */
        const float x = aa3_.process(aa2_.process(aa1_.process(in[i])));

        float d[SOLINA_ENSEMBLE_LINES];

        for (int l = 0; l < SOLINA_ENSEMBLE_LINES; ++l)
        {
            Line& ln = line_[l];

            /* Control Circuit: Summe beider Steueroszillatoren */
            const float mod = sinf(twoPi * ph1_[l]) * depth1_
                            + sinf(twoPi * ph2_[l]) * depth2_;

            ln.buf[ln.w] = x;
            ln.w = (ln.w + 1 < ln.size) ? (ln.w + 1) : 0;

            float delay = delayCenter_ + delayVar_ * mod;
            if (delay < 1.0f)                        delay = 1.0f;
            if (delay > (float) (ln.size - 2))       delay = (float) (ln.size - 2);

            d[l] = readDelay(ln, delay);

            ph1_[l] += inc1_; if (ph1_[l] >= 1.0f) ph1_[l] -= 1.0f;
            ph2_[l] += inc2_; if (ph2_[l] >= 1.0f) ph2_[l] -= 1.0f;
        }

        /* Ausgangsmatrix der drei Modulatorschaltungen, danach der
         * Rekonstruktionstiefpass (Low-Pass Filter TR4-5). */
        outL[i] = reconL2_.process(reconL1_.process(d[0] + d[1] - d[2]));
        outR[i] = reconR2_.process(reconR1_.process(d[0] - d[1] - d[2]));
    }
}
