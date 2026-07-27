/*
  SM_Controller.h -- Bedienlogik fuer PicoFaceSM

  Drei Drehgeber wie im Master-Projekt: der erste blaettert durch die Seiten,
  die beiden anderen bedienen die zwei Parameter der aktuellen Seite.

  Die Seitenaufteilung steht als Tabelle in SM_Controller.cpp -- Umsortieren
  heisst dort eine Zeile verschieben, nicht Code aendern.

  Der Controller haelt nur Schattenkopien fuer die Anzeige; die einzige
  Wahrheit ist die Engine auf der Audioseite, erreicht ueber den IPC-Ring.
*/

#ifndef SM_CONTROLLER_H
#define SM_CONTROLLER_H

#include <cstdint>
#include "solina/solina.h"
#include "SM_Midi.h"
#include "sm_ipc.h"
#include "sm_settings.h"

/* Pseudo-Parameter-IDs oberhalb der Engine-Parameter */
enum {
    SM_UI_PROGRAM = SOLINA_PARAM_COUNT,   /* Werksprogramm 0..7 */
    SM_UI_MIDICH,                          /* Empfangskanal 0..15, 16 = Omni */
    SM_UI_COUNT
};

class SM_Controller
{
public:
    explicit SM_Controller(SM_Midi& midi);

    void onEncoder1(int delta);   /* Seite */
    void onEncoder2(int delta);   /* Parameter A */
    void onEncoder3(int delta);   /* Parameter B */

    /* Taster des ersten Drehgebers: zurueck auf die erste Seite.
     * Gibt true, wenn sich dadurch etwas geaendert hat. */
    bool homePage();

    int         currentPage() const { return page_; }
    int         pageCount() const;
    const char* pageName() const;

    const char* paramAName() const;
    const char* paramBName() const;
    void        paramAText(char* dst, size_t n) const;
    void        paramBText(char* dst, size_t n) const;

    uint8_t     midiChannel() const { return midiCh_; }

    /* Persistenz */
    void exportSettings(SmSettingsV1& s) const;
    void importSettings(const SmSettingsV1& s);

    /* Beim Programmwechsel ueber MIDI die Schattenkopien nachziehen */
    void syncFromProgram(int32_t program);

private:
    void  adjust(int slot, int delta);       /* slot 0 = A, 1 = B */
    int   paramIdOf(int slot) const;
    void  sendParam(int id, float v);
    void  formatValue(int id, char* dst, size_t n) const;

    SM_Midi& midi_;
    int      page_ = 0;
    uint8_t  midiCh_ = SM_MIDI_OMNI;
    int32_t  program_ = 2;

    /* Schattenkopien der Engine-Parameter, 0..1 */
    float    shadow_[SOLINA_PARAM_COUNT] = {};
};

#endif /* SM_CONTROLLER_H */
