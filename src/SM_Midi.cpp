// =====================================================================
// SM_Midi.cpp -- MIDI-Frontend fuer PicoFaceSM
//
// Nimmt die eingehenden Ereignisse entgegen, filtert nach Empfangskanal
// und reicht sie ueber den IPC-Ring an den Audioproduzenten weiter.
//
// Erkannt und umgesetzt:
//   Note On / Note Off    Klaviatur C2..C6 (36..84); Noten ausserhalb
//                         werden oktavweise hineingefaltet. Note On mit
//                         Anschlagstaerke 0 gilt als Note Off.
//   CC 7   Lautstaerke
//   CC 64  Haltepedal     >= 64 = an
//   CC 72  Sustain        Abfallzeit der Sustain-Schaltung
//   CC 73  Crescendo      Anstiegszeit der Sustain-Schaltung
//   CC 80..85             Registerzuege Contrabass..Horn
//   CC 93  Ensemble       Modulator-Schaltungen an/aus
//   CC 120 All Sound Off
//   CC 121 Reset All Ctrl Pedal aus, Pitchbend auf Mitte
//   CC 123 All Notes Off
//   Program Change        0..7, darueber modulo
//   Pitch Bend            +/- 2 Halbtoene, wirkt auf den Master-Oszillator
//
// Nicht umgesetzt:
//   Anschlagdynamik       die Torschaltung des Originals kennt nur auf/zu
//   CC 1  Modulation      die Solina hat kein spielbares Vibrato
// =====================================================================

#include "SM_Midi.h"
#include "sm_ipc.h"
#include "solina/solina.h"

// ---------------------------------------------------------------------
// Die Solina hat 49 Tasten (C2..C6). Noten ausserhalb werden oktavweise
// hineingefaltet -- identisch fuer Note On und Note Off, damit ein Paar
// immer dieselbe Taste trifft.
// ---------------------------------------------------------------------
static uint8_t foldToRange(uint8_t note)
{
    while (note < SOLINA_KEY_FIRST) note += 12;
    while (note > SOLINA_KEY_LAST)  note -= 12;
    return note;
}

void SM_Midi::init(uint8_t rxChannel)
{
    setRxChannel(rxChannel);
}

void SM_Midi::onNoteOn(uint8_t note, uint8_t vel, uint8_t ch)
{
    if (!channelMatches(ch)) return;

    if (vel == 0) {
        ipc_send_note_off(foldToRange(note));
        return;
    }
    ipc_send_note_on(foldToRange(note), vel);
}

void SM_Midi::onNoteOff(uint8_t note, uint8_t vel, uint8_t ch)
{
    (void) vel;   // keine Loslassdynamik
    if (!channelMatches(ch)) return;

    ipc_send_note_off(foldToRange(note));
}

void SM_Midi::onControlChange(uint8_t cc, uint8_t val, uint8_t ch)
{
    if (!channelMatches(ch)) return;

    switch (cc) {
        case 7:     // Lautstaerke
        case 64:    // Haltepedal
        case 120:   // All Sound Off
        case 123:   // All Notes Off
            ipc_send_cc(cc, val);
            break;

        // Registerzuege ueber die General-Purpose-Controller 80..85,
        // damit sich die Frontplatte fernsteuern laesst.
        case 80: ipc_send_param(SOLINA_CONTRABASS, val >= 64 ? 1000 : 0); break;
        case 81: ipc_send_param(SOLINA_CELLO,      val >= 64 ? 1000 : 0); break;
        case 82: ipc_send_param(SOLINA_VIOLA,      val >= 64 ? 1000 : 0); break;
        case 83: ipc_send_param(SOLINA_VIOLIN,     val >= 64 ? 1000 : 0); break;
        case 84: ipc_send_param(SOLINA_TRUMPET,    val >= 64 ? 1000 : 0); break;
        case 85: ipc_send_param(SOLINA_HORN,       val >= 64 ? 1000 : 0); break;

        // Ensemble auf dem ueblichen Chorus-Schalter
        case 93: ipc_send_param(SOLINA_ENSEMBLE,   val >= 64 ? 1000 : 0); break;

        // Huellkurve als Dauerwerte (0..127 -> 0..1000 Promille)
        case 73: ipc_send_param(SOLINA_CRESCENDO, (uint16_t)(val * 1000 / 127)); break;
        case 72: ipc_send_param(SOLINA_SUSTAIN,   (uint16_t)(val * 1000 / 127)); break;

        case 121:   // Reset All Controllers
            ipc_send_cc(64, 0);
            ipc_send_pitch_bend(8192);
            break;

        default:
            break;
    }
}

void SM_Midi::onProgramChange(uint8_t program, uint8_t ch)
{
    if (!channelMatches(ch)) return;

    ipc_send_param(SM_PARAM_PROGRAM,
                   (uint16_t) (program % SOLINA_NPROGRAMS));
}

void SM_Midi::onPitchBend(uint16_t value, uint8_t ch)
{
    if (!channelMatches(ch)) return;

    if (value > 16383) value = 16383;
    ipc_send_pitch_bend(value);
}

uint8_t SM_Midi::getRxChannel() const
{
    return rxChannel_;
}

void SM_Midi::setRxChannel(uint8_t ch)
{
    if (ch > SM_MIDI_OMNI)
        ch = SM_MIDI_OMNI;
    rxChannel_ = ch;
}
