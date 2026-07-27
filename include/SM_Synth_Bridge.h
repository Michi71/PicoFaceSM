/*
  SM_Synth_Bridge.h -- Anbindung der Solina-Engine an das Pico-Audio-Subsystem

  Die Bruecke ist bewusst duenn: die Engine rendert bereits blockweise nach
  Float und bringt ihre eigene weiche Begrenzung mit. Hier bleibt das
  Umsetzen auf das int32-Stereoformat des I2S-Puffers, das Haltepedal und
  die Lastmessung.

  Aufteilung wie im Master-Projekt PicoFaceRD: der Produzent laeuft im
  Main-Loop von Core 0, der DMA-IRQ bleibt mikroskopisch. Einen Worker auf
  Core 1 braucht die Solina nicht -- die Engine kostet auf dem Host 0,33 %
  eines M4-Cores bei zehn Tasten.
*/

#ifndef SM_SYNTH_BRIDGE_H
#define SM_SYNTH_BRIDGE_H

#include <cstdint>
#include "solina/solina.h"

class SM_Synth_Bridge
{
public:
    void init();

    /* Fuellt einen I2S-Puffer (Stereo, int32 interleaved). */
    void fill_buffer_i32(int32_t* out, int length);

    /* --- MIDI ---------------------------------------------------------- */
    void noteOn(uint8_t note, uint8_t vel)
    {
        noteOnCount_++;
        solina_.noteOn(note, vel);
    }

    void noteOff(uint8_t note)      { solina_.noteOff(note); }
    void sustain(uint8_t value)     { solina_.processMidiController(0x40, value); }
    void allNotesOff()              { solina_.stopVoices(); }
    void pitchBend(uint16_t b14)    { solina_.setPitchBend((int32_t) b14); }

    /* --- Panel --------------------------------------------------------- */
    void setParameter(int32_t index, float value) { solina_.setParameter(index, value); }
    float parameter(int32_t index) const          { return solina_.getParameter(index); }

    void setProgram(int32_t p)      { solina_.setProgram(p); }
    int32_t program() const         { return solina_.getProgram(); }
    int32_t programCount() const    { return solina_.getProgramCount(); }
    void programName(char* dst) const { solina_.getProgramName(dst); }

    void setVolume(uint8_t v)       { solina_.setVolume(v); }

    uint32_t currentSampleRate() const { return SAMPLING_RATE; }

    /* --- Diagnose (Fusszeile des Displays) ----------------------------- */
    int      cpuLoadPeakPercent() const { return cpuPeak_; }
    void     resetCpuPeak()             { cpuPeak_ = 0; }
    uint32_t noteOnCount() const        { return noteOnCount_; }

private:
    Solina   solina_;
    uint32_t noteOnCount_ = 0;
    int      cpuPeak_ = 0;
};

#endif /* SM_SYNTH_BRIDGE_H */
