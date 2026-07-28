/*
  SM_Synth_Bridge.cpp -- Anbindung der Solina-Engine an das Audio-Subsystem
*/

#include "SM_Synth_Bridge.h"

#include "pico/stdlib.h"
#include "hardware/timer.h"

#ifndef RAM_HOT
#define RAM_HOT(f) __not_in_flash_func(f)
#endif

static inline uint32_t bridge_time_us_32()
{
    return time_us_32();
}

void SM_Synth_Bridge::init()
{
    solina_.setSampleRate((float) SAMPLING_RATE);
    solina_.setVolume(100);
    solina_.setProgram(2);   /* Full Strings */
}

/*
 * Der I2S-Puffer erwartet Stereo-int32 interleaved. Die Engine liefert Float
 * und begrenzt bereits weich (solinaSoftClip), deshalb genuegt hier die
 * Skalierung -- ein zweiter Begrenzer wuerde nur doppelt verzerren.
 */
void RAM_HOT(SM_Synth_Bridge::fill_buffer_i32)(int32_t* out, int length)
{
    const uint32_t t0 = bridge_time_us_32();

    float l[SOLINA_BLOCK];
    float r[SOLINA_BLOCK];

    int done = 0;
    while (done < length)
    {
        int chunk = length - done;
        if (chunk > SOLINA_BLOCK)
            chunk = SOLINA_BLOCK;

        solina_.processFloat(l, r, chunk);

        for (int i = 0; i < chunk; ++i)
        {
            /*
             * Das I2S-Format ist S32: der Abtastwert muss linksbuendig im
             * 32-Bit-Wort stehen. Hier auf 24 Bit skaliert und um 8 Bit nach
             * oben geschoben -- das Master-Projekt nutzt 16 Bit mit << 16,
             * die PIO schiebt so oder so 32 Bit hinaus.
             *
             * Die Engine begrenzt bereits weich auf +/-1, die Klemmung ist
             * nur Absicherung gegen Rundung.
             */
            int32_t dl = (int32_t) (l[i] * 8388607.0f);
            int32_t dr = (int32_t) (r[i] * 8388607.0f);
            if (dl >  8388607) dl =  8388607;
            if (dl < -8388608) dl = -8388608;
            if (dr >  8388607) dr =  8388607;
            if (dr < -8388608) dr = -8388608;

            out[(done + i) * 2 + 0] = dl << 8;
            out[(done + i) * 2 + 1] = dr << 8;
        }

        done += chunk;
    }

    /* Lastanzeige: verbrauchte Zeit gegen die Zeit, die der Block dauert */
    const uint32_t used = bridge_time_us_32() - t0;
    const uint32_t avail = (uint32_t) ((1000000ull * (uint64_t) length) / SAMPLING_RATE);
    if (avail > 0)
    {
        const int pct = (int) ((used * 100u) / avail);
        if (pct > cpuPeak_)
            cpuPeak_ = pct;
    }
}
