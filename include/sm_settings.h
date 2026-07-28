/*
  sm_settings.h -- persistierter Frontplattenzustand

  Nutzlast fuer das veeprom-Anhaengeprotokoll. Gespeichert wird das
  Werksprogramm, der Empfangskanal und alle Engine-Parameter in Promille
  (0..1000), damit der Datensatz klein und versionsstabil bleibt.

  Bei Layoutaenderungen SM_SETTINGS_VERSION erhoehen -- veeprom verwirft
  Datensaetze mit abweichender Version.
*/

#ifndef SM_SETTINGS_H
#define SM_SETTINGS_H

#include <stdint.h>
#include "solina/solina.h"

#define SM_SETTINGS_VERSION 2u

/* Version 2: Parameterliste um den Phaser erweitert (drei Eintraege), damit
 * verschieben sich alle Indizes ab SOLINA_TONE_LOWPASS. Aeltere Datensaetze
 * werden von veeprom anhand der Version verworfen. */
struct __attribute__((packed)) SmSettingsV1 {
    uint8_t  program;                        /* 0..7                  */
    uint8_t  midiCh;                         /* 0..15, 16 = Omni      */
    uint16_t param[SOLINA_PARAM_COUNT];      /* je 0..1000 Promille   */
};

/* veeprom traegt 240 Byte Nutzlast je Datensatz. Der Wachhund faengt ab,
 * falls die Parameterliste ueber diese Grenze waechst. */
static_assert(sizeof(SmSettingsV1) <= 240,
              "SmSettingsV1 passt nicht mehr in einen veeprom-Datensatz");

#endif /* SM_SETTINGS_H */
