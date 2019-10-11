/*
   MicroDexed

   MicroDexed is a port of the Dexed sound engine
   (https://github.com/asb2m10/dexed) for the Teensy-3.5/3.6 with audio shield.
   Dexed ist heavily based on https://github.com/google/music-synthesizer-for-android

   (c)2018,2019 H. Wirtz <wirtz@parasitstudio.de>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

*/

#include "config.h"
#define BOUNCE_WITH_PROMPT_DETECTION
#include <Bounce.h>
#include <EEPROM.h>
#include <MIDI.h>
//#include "SSD1322_Plus.h"
#include "Encoder4.h"

#ifndef UI_H_INCLUDED
#define UI_H_INCLUDED

#define NUM_ENCODERS 2

#define VALUE_ENCODER 1

extern Encoder4 enc[NUM_ENCODERS];
extern int32_t enc_val[2];
extern Bounce but[2];
//extern LiquidCrystalPlus_I2C disp;
//extern SSD1322_Plus disp;
extern config_t configuration;
extern uint8_t max_loaded_banks;
extern char bank_name[BANK_NAME_LEN];
extern char voice_name[VOICE_NAME_LEN];
extern uint8_t ui_state;
extern uint8_t ui_main_state;
extern void eeprom_write(void);
extern void set_volume(float v, float pan);
extern elapsedMillis autostore;
extern elapsedMillis long_button_pressed;
extern uint8_t effect_filter_cutoff;
extern uint8_t effect_filter_resonance;
extern uint8_t effect_delay_time;
extern uint8_t effect_delay_feedback;
extern uint8_t effect_delay_volume;
extern bool effect_delay_sync;
extern AudioEffectDelay delay1;
extern AudioMixer4 mixer1;
extern AudioMixer4 mixer2;
extern uint8_t wanted_volume;
extern value_change_t soften_volume;
extern value_change_t soften_filter_res;
extern value_change_t soften_filter_cut;


void handle_ui(void);
void handle_ui_change(void);

void setup_ui(void);

float mapfloat(float val, float in_min, float in_max, float out_min, float out_max);

class MyEncoder : public Encoder
{
    int32_t read()
    {
      return (Encoder::read() / 4);
    }
    void write(int32_t p)
    {
      Encoder::write(p * 4);
    }
};
#endif
