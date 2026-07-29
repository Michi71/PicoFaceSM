/*
  sm_main.cpp -- PicoFaceSM, ARP Solina String Ensemble on the RP2350

  Structured like the master project PicoFaceRD:
      Core 0  audio producer in the main loop, plus USB, MIDI, controls and
              display
      IRQ     microscopic, so the next DMA transfer is always re-armed in
              time

  Unlike RD there is no voice worker on core 1: the Solina does not need one.
  Core 1 stays unused for now.
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

#ifndef PICO_AUDIO_I2S_DMA_IRQ
#define PICO_AUDIO_I2S_DMA_IRQ 0
#endif

#include "project_config.h"
#include "sm_ipc.h"
#include "midi_input_usb.h"
#include "SM_Synth_Bridge.h"
#include "SM_Midi.h"
#include "SM_Controller.h"
#include "SM_Display.h"
#include "sm_settings.h"
#include "veeprom.h"
#include "audio_subsystem.h"
#include "pico_hw.h"
#include "get_serial.h"

#if __has_include("bsp/board_api.h")
#include "bsp/board_api.h"
#else
#include "bsp/board.h"
#endif

#include "u8g2.h"
#include "encoder.h"
#include "push_button.h"

/* ------------------------------------------------------------------------ */
/* Global instances                                                          */
/* ------------------------------------------------------------------------ */
Encoder encSel(pio1, 0, {PIN_SEL_CLK, PIN_SEL_DT}, PIN_UNUSED, NORMAL_DIR, ROTARY_CPR, false, 444);
Encoder encA(pio1, 1, {PIN_PA_CLK, PIN_PA_DT}, PIN_UNUSED, NORMAL_DIR, ROTARY_CPR, false, 444);
Encoder encB(pio1, 2, {PIN_PB_CLK, PIN_PB_DT}, PIN_UNUSED, NORMAL_DIR, ROTARY_CPR, false, 444);

PushButton btSel(PIN_SEL_SW, 50);
PushButton btA(PIN_PA_SW, 50);
PushButton btB(PIN_PB_SW, 50);

audio_buffer_pool_t* ap = nullptr;
static volatile uint32_t g_pio_stall_count = 0;
static uint8_t g_ui_flush_row = 16;   /* 16 = idle, 0..15 = next half tile row */
u8g2_t u8g2;

SM_Synth_Bridge smBridge;
MIDIInputUSB    usbmidi;
SM_Midi         smMidi;
SM_Controller   smController(smMidi);

/* ------------------------------------------------------------------------ */
/* Applying IPC packets on the audio side                                    */
/* ------------------------------------------------------------------------ */
static void ipc_apply(uint32_t pkt)
{
    switch (ipc_type(pkt)) {
        case IPC_CMD_SM_NOTE_ON:
            smBridge.noteOn(ipc_d1(pkt), (uint8_t) ipc_d2(pkt));
            break;

        case IPC_CMD_SM_NOTE_OFF:
            smBridge.noteOff(ipc_d1(pkt));
            break;

        case IPC_CMD_SM_CC:
            switch (ipc_d1(pkt)) {
                case 7:   smBridge.setVolume((uint8_t) ipc_d2(pkt)); break;
                case 64:  smBridge.sustain((uint8_t) ipc_d2(pkt));   break;
                case 120:
                case 123: smBridge.allNotesOff();                    break;
                default:  break;
            }
            break;

        case IPC_CMD_SM_PITCH_BEND:
            smBridge.pitchBend(ipc_d2(pkt));
            break;

        case IPC_CMD_SM_PARAM: {
            const uint8_t  id = ipc_d1(pkt);
            const uint16_t v  = ipc_d2(pkt);
            if (id == SM_PARAM_PROGRAM)
                smBridge.setProgram((int32_t) v);
            else if (id < SOLINA_PARAM_COUNT)
                smBridge.setParameter(id, (float) v / 1000.0f);
            break;
        }

        default:
            break;
    }
}

/*
 * veeprom lock hooks: there is nothing to park. The write runs on core 0
 * between two audio blocks and core 1 is idle. veeprom disables the
 * interrupts itself and restores the QMI timing afterwards.
 */
static bool sm_flash_lock(void)   { return true; }
static void sm_flash_unlock(void) {}

extern "C" void __not_in_flash_func(i2s_callback_func)()
{
    /* Rendering happens in the main loop; the IRQ stays deliberately tiny. */
    if (!ap) return;
    g_pio_stall_count += audio_i2s_consume_txstall();
}

static void sm_ui_draw();
static void sm_note_on_cb(uint8_t n, uint8_t v, uint8_t c)  { smMidi.onNoteOn(n, v, c); }
static void sm_note_off_cb(uint8_t n, uint8_t v, uint8_t c) { smMidi.onNoteOff(n, v, c); }
static void sm_cc_cb(uint8_t cc, uint8_t v, uint8_t c)      { smMidi.onControlChange(cc, v, c); }
static void sm_pc_cb(uint8_t p, uint8_t c)                  { smMidi.onProgramChange(p, c); }
static void sm_pb_cb(uint16_t v, uint8_t c)                 { smMidi.onPitchBend(v, c); }

/* ------------------------------------------------------------------------ */
#if SM_SAFE_MODE
#define SM_MARK(x) do { printf("SM: " x "\n"); } while (0)
#else
#define SM_MARK(x) do { } while (0)
#endif

int main(void)
{
    pico_init();
    SM_MARK("pico_init ok");
    smBridge.init();
    SM_MARK("engine ok");

    board_init();
    usb_serial_init();
    tusb_init();
    SM_MARK("usb ok");

    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_pico_hw_i2c,
                                          u8x8_gpio_and_delay_pico);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
    SM_MARK("display ok");

    /* Hold the splash screen for two seconds, keeping USB alive */
    sm_display_splash(&u8g2);
    absolute_time_t splash_end = make_timeout_time_ms(2000);
    while (!time_reached(splash_end)) { tud_task(); sleep_ms(1); }

    ap = init_audio(smBridge.currentSampleRate(), 6);
    SM_MARK("audio ok");
    irq_set_priority(DMA_IRQ_0 + PICO_AUDIO_I2S_DMA_IRQ, 0x00);
    irq_set_priority(USBCTRL_IRQ, 0xC0);

    btSel.Init();  btA.Init();  btB.Init();
    encSel.init(); encA.init(); encB.init();

    smMidi.init();
    SM_MARK("encoder+midi ok");

#if !SM_SAFE_MODE
    veeprom_set_lock_hooks(sm_flash_lock, sm_flash_unlock);
    veeprom_init();
    {
        SmSettingsV1 s;
        uint16_t len = 0, ver = 0;
        if (veeprom_load(&s, sizeof(s), &len, &ver) &&
            ver == SM_SETTINGS_VERSION && len >= sizeof(s))
        {
            smController.importSettings(s);
        }
    }
#endif

    usbmidi.setNoteOnCallback(sm_note_on_cb);
    usbmidi.setNoteOffCallback(sm_note_off_cb);
    usbmidi.setCCCallback(sm_cc_cb);
    usbmidi.setProgramChangeCallback(sm_pc_cb);
    usbmidi.setPitchBendCallback(sm_pb_cb);

#if SM_SAFE_MODE
    /* Safe mode: a held chord, so the audio path can be checked without
     * MIDI. If nothing comes out here, the MIDI path is not the cause. */
    smBridge.noteOn(48, 100);
    smBridge.noteOn(55, 100);
    smBridge.noteOn(64, 100);
    smBridge.noteOn(67, 100);
#endif

    sm_ui_draw();

    SM_MARK("entering main loop");

    bool dirty = false;
    uint32_t last_draw = to_ms_since_boot(get_absolute_time());

    while (true)
    {
        /* Producer in thread context: interruptible by every IRQ. */
        audio_buffer_t* buffer;
        while ((buffer = take_audio_buffer(ap, false)) != nullptr)
        {
            uint32_t pkt;
            while (sm_ipc_pop(&pkt))
                ipc_apply(pkt);

            int32_t* samples = (int32_t*) buffer->buffer->bytes;
            smBridge.fill_buffer_i32(samples, buffer->max_sample_count);
            buffer->sample_count = buffer->max_sample_count;
            give_audio_buffer(ap, buffer);
        }

        if (g_ui_flush_row < 16)
        {
            /* Push the display out in half tile rows (~1.5 ms of I2C each),
             * so the audio lead does not collapse. */
            u8g2_UpdateDisplayArea(&u8g2, (g_ui_flush_row & 1) ? 8 : 0,
                                   (uint8_t) (g_ui_flush_row >> 1), 8, 1);
            g_ui_flush_row++;
        }

        tud_task();
        usbmidi.process();

        /* Push button of the first encoder: back to page 1. Toggled() fires
         * on both edges, so the pressed state is checked as well -- otherwise
         * releasing would trigger a second time. Changing page alters no
         * parameter and therefore does not mark the settings dirty. */
        if (btSel.Toggled() && btSel.ReadButton() == PushButton::PRESSED)
        {
            if (smController.homePage())
                dirty = true;
        }

        int32_t d1 = encSel.delta();
        int32_t d2 = encA.delta();
        int32_t d3 = encB.delta();

        if (d1 != 0) { smController.onEncoder1((int) d1); dirty = true; }
        if (d2 != 0) { smController.onEncoder2((int) d2); dirty = true; }
        if (d3 != 0) { smController.onEncoder3((int) d3); dirty = true; }

        uint32_t now = to_ms_since_boot(get_absolute_time());

        /* Debounced save while idle. Only encoder operation marks the state
         * dirty -- changes driven over MIDI are deliberately transient. */
        static bool settingsDirty = false;
        static uint32_t lastEditMs = 0;
        if (d1 || d2 || d3) { settingsDirty = true; lastEditMs = now; }

#if !SM_SAFE_MODE
        if (settingsDirty && (now - lastEditMs) > 2000u)
        {
            SmSettingsV1 s;
            smController.exportSettings(s);
            veeprom_save(&s, sizeof(s), SM_SETTINGS_VERSION);
            settingsDirty = false;
        }
#else
        (void) lastEditMs;
        settingsDirty = false;
#endif

        if (g_ui_flush_row >= 16 &&
            ((dirty && (now - last_draw > 50)) || (now - last_draw > 500)))
        {
            sm_ui_draw();
            dirty = false;
            last_draw = now;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Display                                                                   */
/* ------------------------------------------------------------------------ */
static void sm_ui_draw()
{
    static SmUiModel m;
    char va[20], vb[20];

    snprintf(m.title, sizeof(m.title), "%s", smController.pageName());
    snprintf(m.page,  sizeof(m.page),  "%d/%d",
             smController.currentPage() + 1, smController.pageCount());

    smController.paramAText(va, sizeof(va));
    smController.paramBText(vb, sizeof(vb));

    /* Empty label = the value describes itself (see kNames). */
    const char* na = smController.paramAName();
    const char* nb = smController.paramBName();
    if (na[0]) snprintf(m.lineA, sizeof(m.lineA), "%s %s", na, va);
    else       snprintf(m.lineA, sizeof(m.lineA), "%s", va);
    if (nb[0]) snprintf(m.lineB, sizeof(m.lineB), "%s %s", nb, vb);
    else       snprintf(m.lineB, sizeof(m.lineB), "%s", vb);

    snprintf(m.footer, sizeof(m.footer), "P%d U%lu D%lu N%lu",
             (int) smBridge.cpuLoadPeakPercent(),
             (unsigned long) g_i2s_underrun_count,
             (unsigned long) sm_ipc_dropped,
             (unsigned long) smBridge.noteOnCount());
    smBridge.resetCpuPeak();

    sm_display_page(&u8g2, m);

    /* No blocking SendBuffer here -- the main loop pushes the buffer out
     * row by row. */
    g_ui_flush_row = 0;
}
