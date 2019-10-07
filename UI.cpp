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

#include <Arduino.h>
#include <limits.h>
#include "config.h"
#include "dexed.h"
#include "dexed_sysex.h"
#include "UI.h"
// The menu wrapper library
#include <LiquidMenu.h>

#ifdef LCD_U8X8_SPI
#include <U8x8lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#endif

#ifdef LCD_I2C
#include "LiquidCrystal_I2C.h"
#endif

#ifdef LCD_DISPLAY // selecting sounds by encoder, button and display

#ifdef LCD_U8X8_SPI
U8X8_SSD1322_NHD_256X64_4W_HW_SPI disp(LCD_CS_PIN, LCD_DC_PIN, LCD_RESET_PIN);  // Enable U8G2_16BIT in u8g2.h
#define DisplayClass U8X8_SSD1322_NHD_256X64_4W_HW_SPI
#endif

#ifdef LCD_I2C
LiquidCrystal_I2C disp(LCD_I2C_ADDRESS, LCD_CHARS, LCD_LINES);
#define DisplayClass LiquidCrystal_I2C
#endif

unsigned int period_check = 100;
elapsedMillis lastMs_check = 0;

unsigned int period_mainScreen = 5000;
elapsedMillis lastMs_mainScreen = 0;

elapsedMillis long_button_pressed;

unsigned short encoderReading[NUM_ENCODERS] = {0, 0};
unsigned short lastEncoderReading[NUM_ENCODERS] = {0, 0};

/*
void handle_ui_old(void)
{
  if (ui_back_to_main >= UI_AUTO_BACK_MS && (ui_state != UI_MAIN && ui_state != UI_EFFECTS_FILTER && ui_state != UI_EFFECTS_DELAY))
  {
    enc[0].write(map(configuration.vol * 100, 0, 100, 0, ENC_VOL_STEPS));
    enc_val[0] = enc[0].read();
    ui_show_main();
  }

  if (autostore >= AUTOSTORE_MS && (ui_main_state == UI_MAIN_VOICE_SELECTED || ui_main_state == UI_MAIN_BANK_SELECTED))
  {
    switch (ui_main_state)
    {
      case UI_MAIN_VOICE_SELECTED:
        ui_main_state = UI_MAIN_VOICE;
        break;
      case UI_MAIN_BANK_SELECTED:
        ui_main_state = UI_MAIN_BANK;
        break;
    }
  }

  for (uint8_t i = 0; i < NUM_ENCODER; i++)
  {
    but[i].update();

    if (but[i].fallingEdge())
      long_button_pressed = 0;

    if (but[i].risingEdge())
    {
      uint32_t button_released = long_button_pressed;

      if (button_released > LONG_BUTTON_PRESS)
      {
        // long pressing of button detected
#ifdef DEBUG
        Serial.print(F("Long button pressing detected for button "));
        Serial.println(i, DEC);
#endif

        switch (i)
        {
          case 0: // long press for left button
            break;
          case 1: // long press for right button
            switch (ui_state)
            {
              case UI_MAIN:
                ui_main_state = UI_MAIN_FILTER_RES;
                enc[i].write(effect_filter_resonance);
                enc_val[i] = enc[i].read();
                ui_show_effects_filter();
                break;
              case UI_EFFECTS_FILTER:
                ui_main_state = UI_MAIN_DELAY_TIME;
                enc[i].write(effect_delay_time);
                enc_val[i] = enc[i].read();
                ui_show_effects_delay();
                break;
              case UI_EFFECTS_DELAY:
                ui_main_state = UI_MAIN_VOICE;
                enc[i].write(configuration.voice);
                enc_val[i] = enc[i].read();
                ui_show_main();
                break;
            }
            break;
        }
      }
      else
      {
        // Button pressed
        switch (i)
        {
          case 0: // left button pressed
            switch (ui_state)
            {
              case UI_MAIN:
                enc[i].write(map(configuration.vol * 100, 0, 100, 0, ENC_VOL_STEPS));
                enc_val[i] = enc[i].read();
                ui_show_volume();
                break;
              case UI_VOLUME:
                enc[i].write(configuration.midi_channel);
                enc_val[i] = enc[i].read();
                ui_show_midichannel();
                break;
              case UI_MIDICHANNEL:
                enc[i].write(map(configuration.vol * 100, 0, 100, 0, ENC_VOL_STEPS));
                enc_val[i] = enc[i].read();
                ui_show_main();
                break;
            }
            break;
          case 1: // right button pressed
            switch (ui_state)
            {
              case UI_MAIN:
                switch (ui_main_state)
                {
                  case UI_MAIN_BANK:
                  case UI_MAIN_BANK_SELECTED:
                    ui_main_state = UI_MAIN_VOICE;
                    enc[i].write(configuration.voice);
                    enc_val[i] = enc[i].read();
                    break;
                  case UI_MAIN_VOICE:
                  case UI_MAIN_VOICE_SELECTED:
                    ui_main_state = UI_MAIN_BANK;
                    enc[i].write(configuration.bank);
                    enc_val[i] = enc[i].read();
                    break;
                }
                ui_show_main();
                break;
              case UI_EFFECTS_FILTER:
              case UI_EFFECTS_DELAY:
                switch (ui_main_state)
                {
                  case UI_MAIN_FILTER_RES:
                    ui_main_state = UI_MAIN_FILTER_CUT;
                    enc[i].write(effect_filter_cutoff);
                    enc_val[i] = enc[i].read();
                    ui_show_effects_filter();
                    break;
                  case UI_MAIN_FILTER_CUT:
                    ui_main_state = UI_MAIN_FILTER_RES;
                    enc[i].write(effect_filter_resonance);
                    enc_val[i] = enc[i].read();
                    ui_show_effects_filter();
                    break;
                  case UI_MAIN_DELAY_TIME:
                    ui_main_state = UI_MAIN_DELAY_FEEDBACK;
                    enc[i].write(effect_delay_feedback);
                    enc_val[i] = enc[i].read();
                    ui_show_effects_delay();
                    break;
                  case UI_MAIN_DELAY_VOLUME:
                    ui_main_state = UI_MAIN_DELAY_TIME;
                    enc[i].write(effect_delay_time);
                    enc_val[i] = enc[i].read();
                    ui_show_effects_delay();
                    break;
                  case UI_MAIN_DELAY_FEEDBACK:
                    ui_main_state = UI_MAIN_DELAY_VOLUME;
                    enc[i].write(effect_delay_volume);
                    enc_val[i] = enc[i].read();
                    ui_show_effects_delay();
                    break;
                }
                break;
            }
        }
      }
#ifdef DEBUG
      Serial.print(F("Button "));
      Serial.println(i, DEC);
#endif
    }

    if (enc[i].read() == enc_val[i])
      continue;
    else
    {
      switch (i)
      {
        case 0: // left encoder moved
          float tmp;
          switch (ui_state)
          {
            case UI_MAIN:
            case UI_VOLUME:
              if (enc[i].read() <= 0)
                enc[i].write(0);
              else if (enc[i].read() >= ENC_VOL_STEPS)
                enc[i].write(ENC_VOL_STEPS);
              enc_val[i] = enc[i].read();
              //set_volume(float(map(enc[i].read(), 0, ENC_VOL_STEPS, 0, 100)) / 100, configuration.pan);
              tmp = (float(map(enc[i].read(), 0, ENC_VOL_STEPS, 0, 100)) / 100) - configuration.vol;
              soften_volume.diff = tmp / SOFTEN_VALUE_CHANGE_STEPS;
              soften_volume.steps = SOFTEN_VALUE_CHANGE_STEPS;
#ifdef DEBUG
              Serial.print(F("Setting soften volume from: "));
              Serial.print(configuration.vol, 5);
              Serial.print(F("/Volume step: "));
              Serial.print(soften_volume.steps);
              Serial.print(F("/Volume diff: "));
              Serial.println(soften_volume.diff, 5);
#endif
              eeprom_write();
              ui_show_volume();
              break;
            case UI_MIDICHANNEL:
              if (enc[i].read() <= 0)
                enc[i].write(0);
              else if (enc[i].read() >= 16)
                enc[i].write(16);
              configuration.midi_channel = enc[i].read();
              eeprom_write();
              ui_show_midichannel();
              break;
          }
          break;
        case 1: // right encoder moved
          switch (ui_state)
          {
            case UI_VOLUME:
              ui_state = UI_MAIN;
              disp.clear();
              enc[1].write(configuration.voice);
              eeprom_write();
              ui_show_main();
              break;
            case UI_MAIN:
              switch (ui_main_state)
              {
                case UI_MAIN_BANK:
                  ui_main_state = UI_MAIN_BANK_SELECTED;
                case UI_MAIN_BANK_SELECTED:
                  if (enc[i].read() <= 0)
                    enc[i].write(0);
                  else if (enc[i].read() > max_loaded_banks - 1)
                    enc[i].write(max_loaded_banks - 1);
                  configuration.bank = enc[i].read();
                  get_voice_names_from_bank(configuration.bank);
                  load_sysex(configuration.bank, configuration.voice);
                  eeprom_write();
                  break;
                case UI_MAIN_VOICE:
                  ui_main_state = UI_MAIN_VOICE_SELECTED;
                case UI_MAIN_VOICE_SELECTED:
                  if (enc[i].read() <= 0)
                  {
                    if (configuration.bank > 0)
                    {
                      enc[i].write(MAX_VOICES - 1);
                      configuration.bank--;
                      get_voice_names_from_bank(configuration.bank);
                    }
                    else
                      enc[i].write(0);
                  }
                  else if (enc[i].read() > MAX_VOICES - 1)
                  {
                    if (configuration.bank < MAX_BANKS - 1)
                    {
                      enc[i].write(0);
                      configuration.bank++;
                      get_voice_names_from_bank(configuration.bank);
                    }
                    else
                      enc[i].write(MAX_VOICES - 1);
                  }
                  configuration.voice = enc[i].read();
                  load_sysex(configuration.bank, configuration.voice);
                  eeprom_write();
                  break;
              }
              ui_show_main();
              break;
            case UI_EFFECTS_FILTER:
              switch (ui_main_state)
              {
                case UI_MAIN_FILTER_RES:
                  if (enc[i].read() <= 0)
                    enc[i].write(0);
                  else if (enc[i].read() >= ENC_FILTER_RES_STEPS)
                    enc[i].write(ENC_FILTER_RES_STEPS);
                  enc_val[i] = enc[i].read();
                  effect_filter_resonance = enc[i].read();
                  tmp = 1.0 - (float(map(enc[i].read(), 0, ENC_FILTER_RES_STEPS, 0, 100)) / 100) - dexed->fx.Reso;
                  soften_filter_res.diff = tmp / SOFTEN_VALUE_CHANGE_STEPS;
                  soften_filter_res.steps = SOFTEN_VALUE_CHANGE_STEPS;
#ifdef DEBUG
                  Serial.print(F("Setting soften filter-resonance from: "));
                  Serial.print(dexed->fx.Reso, 5);
                  Serial.print(F("/Filter-Resonance step: "));
                  Serial.print(soften_filter_res.steps);
                  Serial.print(F("/Filter-Resonance diff: "));
                  Serial.println(soften_filter_res.diff, 5);
#endif
                  break;
                case UI_MAIN_FILTER_CUT:
                  if (enc[i].read() <= 0)
                    enc[i].write(0);
                  else if (enc[i].read() >= ENC_FILTER_CUT_STEPS)
                    enc[i].write(ENC_FILTER_CUT_STEPS);
                  enc_val[i] = enc[i].read();
                  effect_filter_cutoff = enc[i].read();
                  tmp = 1.0 - (float(map(enc[i].read(), 0, ENC_FILTER_CUT_STEPS, 0, 100)) / 100) - dexed->fx.Cutoff;
                  soften_filter_cut.diff = tmp / SOFTEN_VALUE_CHANGE_STEPS;
                  soften_filter_cut.steps = SOFTEN_VALUE_CHANGE_STEPS;
#ifdef DEBUG
                  Serial.print(F("Setting soften filter-cutoff from: "));
                  Serial.print(dexed->fx.Cutoff, 5);
                  Serial.print(F("/Filter-Cutoff step: "));
                  Serial.print(soften_filter_cut.steps);
                  Serial.print(F("/Filter-Cutoff diff: "));
                  Serial.println(soften_filter_cut.diff, 5);
#endif
                  break;
              }
              ui_show_effects_filter();
              break;
            case UI_EFFECTS_DELAY:
              switch (ui_main_state)
              {
                case UI_MAIN_DELAY_TIME:
                  if (enc[i].read() <= 0)
                    enc[i].write(0);
                  else if (enc[i].read() > ENC_DELAY_TIME_STEPS)
                    enc[i].write(ENC_DELAY_TIME_STEPS);
                  effect_delay_time = enc[i].read();;
                  delay1.delay(0, mapfloat(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0.0, DELAY_MAX_TIME));
#ifdef DEBUG
                  Serial.print(F("Setting delay time to: "));
                  Serial.println(map(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0, DELAY_MAX_TIME));
#endif
                  break;
                case UI_MAIN_DELAY_FEEDBACK:
                  if (enc[i].read() <= 0)
                    enc[i].write(0);
                  else if (enc[i].read() > ENC_DELAY_FB_STEPS)
                    enc[i].write(ENC_DELAY_FB_STEPS);
                  effect_delay_feedback = enc[i].read();
                  mixer1.gain(1, mapfloat(float(effect_delay_feedback), 0, ENC_DELAY_FB_STEPS, 0.0, 1.0));
#ifdef DEBUG
                  Serial.print(F("Setting delay feedback to: "));
                  Serial.println(mapfloat(float(effect_delay_feedback), 0, ENC_DELAY_FB_STEPS, 0.0, 1.0));
#endif
                  break;
                case UI_MAIN_DELAY_VOLUME:
                  if (enc[i].read() <= 0)
                    enc[i].write(0);
                  else if (enc[i].read() > ENC_DELAY_VOLUME_STEPS)
                    enc[i].write(ENC_DELAY_VOLUME_STEPS);
                  effect_delay_volume = enc[i].read();
                  float tmp_vol = mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0);
                  //mixer2.gain(0, 1.0 - mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0)); // delay tap1 signal (with added feedback)
                  mixer2.gain(0, 1.0 - tmp_vol); // delay tap1 signal (with added feedback)
                  mixer2.gain(1, tmp_vol); // delay tap1 signal (with added feedback)
                  mixer2.gain(2, tmp_vol); // delay tap1 signal
#ifdef DEBUG
                  Serial.print(F("Setting delay volume to: "));
                  Serial.println(effect_delay_volume);
#endif
                  break;
              }
              ui_show_effects_delay();
              break;
          }
          break;
      }
#ifdef DEBUG
      Serial.print(F("Encoder "));
      Serial.print(i, DEC);
      Serial.print(F(": "));
      Serial.println(enc[i].read(), DEC);
#endif
    }
    enc_val[i] = enc[i].read();
  }
}
*/


char volume_value_text1[] = "    ";
char * get_ui_Volume (void) {
    sprintf(volume_value_text1, "%0.0f", configuration.vol*100+0.5);
    return  volume_value_text1;
}

char volume_value_text2[] = "                      ";

char * get_ui_VolumeBar (void) {
    int barlen = (int)(configuration.vol*20 + 0.5);
    int n = sprintf(volume_value_text2,"%.*s", barlen, "====================");
    sprintf(volume_value_text2 + n, "%.*s", 20-barlen, "                    ");
    return  volume_value_text2;
}

void update_volume(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newVolume = (configuration.vol* 100 + 0.5) + diff;
    if (newVolume > 100) newVolume = 100;
    if (newVolume < 0) newVolume = 0;
    //configuration.vol = newVolume / 100.0;
    float tmp = float(newVolume / 100.0) - configuration.vol;
    soften_volume.diff = tmp / SOFTEN_VALUE_CHANGE_STEPS;
    soften_volume.steps = SOFTEN_VALUE_CHANGE_STEPS;
#ifdef DEBUG
    Serial.print(F("Setting soften volume from: "));
    Serial.print(configuration.vol, 5);
    Serial.print(F("/Volume step: "));
    Serial.print(soften_volume.steps);
    Serial.print(F("/Volume diff: "));
    Serial.println(soften_volume.diff, 5);
#endif
    eeprom_write();
    Serial.println("Update volume");
}

LiquidLine<DisplayClass> volume_line1 (0, 0, "Volume ", get_ui_Volume);
LiquidLine<DisplayClass> volume_line2 (0, 1, "[", get_ui_VolumeBar, "]");

LiquidScreen<DisplayClass> volume_screen(volume_line1, volume_line2);

char midichannel_value_text1[] = "    ";
char * get_ui_MidiChannel (void) {
    if (configuration.midi_channel == MIDI_CHANNEL_OMNI) return "OMNI";
    else
        sprintf(midichannel_value_text1, "  %0d", configuration.midi_channel);
    return  midichannel_value_text1;
}


void update_midi(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newMidiChannel = configuration.midi_channel + diff;
    if (newMidiChannel < 0) newMidiChannel = 0;
    if(newMidiChannel > 16) newMidiChannel = 16;
    configuration.midi_channel = newMidiChannel;
    eeprom_write();
    Serial.println("Update midi");
    
}

LiquidLine<DisplayClass> midichan_line1(0, 0, "MIDI Channel");
LiquidLine<DisplayClass> midichan_line2(0, 1, get_ui_MidiChannel);

LiquidScreen<DisplayClass> midichan_screen(midichan_line1, midichan_line2);



char filterres_value_text1[] = "    ";
char * get_ui_FilterRes (void) {
    sprintf(filterres_value_text1, "  %0d", map(effect_filter_resonance, 0, ENC_FILTER_RES_STEPS, 0, 99));
    return  filterres_value_text1;
}

char filtercut_value_text1[] = "    ";
char * get_ui_FilterCut (void) {
    sprintf(filtercut_value_text1, "  %0d", map(effect_filter_cutoff, 0, ENC_FILTER_CUT_STEPS, 0, 99));
    return  filtercut_value_text1;
}


void update_res(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newRes = map(effect_filter_resonance, 0, ENC_FILTER_RES_STEPS, 0, 99) + diff;
    if (newRes > 99) newRes = 99;
    if (newRes < 0) newRes = 0;
    effect_filter_resonance = map(newRes, 0, 99, 0, ENC_FILTER_RES_STEPS);
    //menu.update();
    float tmp = 1.0 - (float(map(effect_filter_resonance, 0, ENC_FILTER_RES_STEPS, 0, 100)) / 100) - dexed->fx.Reso;
    soften_filter_res.diff = tmp / SOFTEN_VALUE_CHANGE_STEPS;
    soften_filter_res.steps = SOFTEN_VALUE_CHANGE_STEPS;
#ifdef DEBUG
    Serial.print(F("Setting soften filter-resonance from: "));
    Serial.print(dexed->fx.Reso, 5);
    Serial.print(F("/Filter-Resonance step: "));
    Serial.print(soften_filter_res.steps);
    Serial.print(F("/Filter-Resonance diff: "));
    Serial.println(soften_filter_res.diff, 5);
#endif
    Serial.println("Update resonance");
}

void update_cut(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newCut = map(effect_filter_cutoff, 0, ENC_FILTER_CUT_STEPS, 0, 99) + diff;
    if (newCut > 99) newCut = 99;
    if (newCut < 0) newCut = 0;
    effect_filter_cutoff = map(newCut, 0, 99, 0, ENC_FILTER_CUT_STEPS);
    float tmp = 1.0 - (float(map(effect_filter_cutoff, 0, ENC_FILTER_CUT_STEPS, 0, 100)) / 100) - dexed->fx.Cutoff;
    soften_filter_cut.diff = tmp / SOFTEN_VALUE_CHANGE_STEPS;
    soften_filter_cut.steps = SOFTEN_VALUE_CHANGE_STEPS;
#ifdef DEBUG
    Serial.print(F("Setting soften filter-cutoff from: "));
    Serial.print(dexed->fx.Cutoff, 5);
    Serial.print(F("/Filter-Cutoff step: "));
    Serial.print(soften_filter_cut.steps);
    Serial.print(F("/Filter-Cutoff diff: "));
    Serial.println(soften_filter_cut.diff, 5);
#endif
    Serial.println("Update cutoff");
}

LiquidLine<DisplayClass> filter_line1(0, 0, "Filter");
LiquidLine<DisplayClass> filter_line2(0, 1, " Res:", get_ui_FilterRes);
LiquidLine<DisplayClass> filter_line3(0, 2, " Cut:", get_ui_FilterCut);

LiquidScreen<DisplayClass> filter_screen(filter_line1, filter_line2, filter_line3);



char delay_value_text1[] = "     ";
char * get_ui_DelayT (void) {
    sprintf(delay_value_text1, " %0d", map(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0, DELAY_MAX_TIME));
    return  delay_value_text1;
}

char delay_value_text2[] = "    ";
char * get_ui_DelayFB (void) {
    sprintf(delay_value_text2, " %0d", map(effect_delay_feedback, 0, ENC_DELAY_FB_STEPS, 0, 99));
    return  delay_value_text2;
}


char delay_value_text3[] = "    ";
char * get_ui_DelayVol (void) {
    sprintf(delay_value_text3, " %0d", map(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0, 99));
    return  delay_value_text3;
}



void update_t(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newT = map(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0, DELAY_MAX_TIME) + diff;
    if (newT > DELAY_MAX_TIME) newT = DELAY_MAX_TIME;
    if (newT < 0) newT = 0;
    effect_delay_time = map(newT, 0, DELAY_MAX_TIME, 0, ENC_DELAY_TIME_STEPS);
    delay1.delay(0, mapfloat(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0.0, DELAY_MAX_TIME));
#ifdef DEBUG
    Serial.print(F("Setting delay time to: "));
    Serial.println(map(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0, DELAY_MAX_TIME));
#endif
    Serial.println("Update time");
}


void update_fb(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newFB = map(effect_delay_feedback, 0, ENC_DELAY_FB_STEPS, 0, 99) + diff;
    if (newFB > 99) newFB = 99;
    if (newFB < 0) newFB = 0;
    effect_delay_feedback = map(newFB, 0, 99, 0, ENC_DELAY_FB_STEPS);
    mixer1.gain(1, mapfloat(float(effect_delay_feedback), 0, ENC_DELAY_FB_STEPS, 0.0, 1.0));
#ifdef DEBUG
    Serial.print(F("Setting delay feedback to: "));
    Serial.println(mapfloat(float(effect_delay_feedback), 0, ENC_DELAY_FB_STEPS, 0.0, 1.0));
#endif
    Serial.println("Update feedback");
}


void update_d_vol(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newVol = map(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0, 99) + diff;
    if (newVol > 99) newVol = 99;
    if (newVol < 0) newVol = 0;
    effect_delay_volume = map(newVol, 0, 99, 0, ENC_DELAY_VOLUME_STEPS);
    float tmp_vol = mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0);
    //mixer2.gain(0, 1.0 - mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0)); // delay tap1 signal (with added feedback)
    mixer2.gain(0, 1.0 - tmp_vol); // delay tap1 signal (with added feedback)
    mixer2.gain(1, tmp_vol); // delay tap1 signal (with added feedback)
    mixer2.gain(2, tmp_vol); // delay tap1 signal
#ifdef DEBUG
    Serial.print(F("Setting delay volume to: "));
    Serial.println(effect_delay_volume);
#endif
    Serial.println("Update d volume");
}


LiquidLine<DisplayClass> delay_line1(0, 0, "Delay");
LiquidLine<DisplayClass> delay_line2(0, 1, "   T:", get_ui_DelayT, "ms");
LiquidLine<DisplayClass> delay_line3(0, 2, "  FB:", get_ui_DelayFB);
LiquidLine<DisplayClass> delay_line4(0, 3, " Vol:", get_ui_DelayVol);

LiquidScreen<DisplayClass> delay_screen(delay_line1, delay_line2, delay_line3, delay_line4);


char main_value_text1[] = "     ";
char * get_ui_BankN (void) {
    sprintf(main_value_text1, " %0d ", configuration.bank);
    return  main_value_text1;
}

char main_value_text2[BANK_NAME_LEN] = "";
char * get_ui_BankT (void) {
    strip_extension(bank_names[configuration.bank], main_value_text2);
    return  main_value_text2;
}


void update_bank(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newBank =  configuration.bank + diff;
    if (newBank >= max_loaded_banks) newBank = max_loaded_banks -1;
    if (newBank < 0) newBank = 0;
    configuration.bank = newBank;
    get_voice_names_from_bank(configuration.bank);
    load_sysex(configuration.bank, configuration.voice);
    eeprom_write();
    Serial.println("Update bank");
}


void update_voice(void) {
    int diff = lastEncoderReading[VALUE_ENCODER] - encoderReading[VALUE_ENCODER];
    int newVoice =  configuration.voice + diff;
    if (newVoice >= MAX_VOICES) newVoice = MAX_VOICES -1;
    if (newVoice < 0) newVoice = 0;
    configuration.voice = newVoice;
    load_sysex(configuration.bank, configuration.voice);
    eeprom_write();
    Serial.println("Update voice");
}
char main_value_text3[] = "     ";
char * get_ui_VoiceN (void) {
    sprintf(main_value_text3, " %0d ", configuration.voice +1);
    return  main_value_text3;
}

char main_value_text4[BANK_NAME_LEN] = "";
char * get_ui_VoiceT (void) {
    strcpy(main_value_text4, voice_names[configuration.voice]);
    return main_value_text4;
}


LiquidLine<DisplayClass> main_line1(0, 0, "Bank/Voice");
LiquidLine<DisplayClass> main_line2(0, 1, get_ui_BankN, get_ui_BankT);
LiquidLine<DisplayClass> main_line3(0, 2, get_ui_VoiceN, get_ui_VoiceT);

LiquidScreen<DisplayClass> main_screen(main_line1, main_line2, main_line3);

/*
 * The LiquidMenu object combines the LiquidScreen objects to form the
 * menu. Here it is only instantiated and the screens are added later
 * using menu.add_screen(someScreen_object);. This object is used to
 * control the menu, for example: menu.next_screen(), menu.switch_focus()...
 */
LiquidMenu<DisplayClass> menu(disp);


void setup_ui(void)
{
#ifdef LCD_I2C
    disp.init();
    disp.blink_off();
    disp.cursor_off();
    disp.backlight();
    disp.noAutoscroll();
    disp.clear();
    disp.display();
#else
    disp.begin();
    disp.clear();
    disp.setFont(u8x8_font_amstrad_cpc_extended_f);
#endif
    disp.printf("MicroDexed %s\n", VERSION);
    disp.printf("(c)parasiTstudio");


    volume_line1.attach_function(1, update_volume);
    volume_line2.attach_function(1, update_volume);
    
    midichan_line2.attach_function(1, update_midi);
    
    filter_line2.attach_function(1, update_res);
    filter_line3.attach_function(1, update_cut);
    
    delay_line2.attach_function(1, update_t);
    delay_line3.attach_function(1, update_fb);
    delay_line4.attach_function(1, update_d_vol);
    
    main_line2.attach_function(1, update_bank);
    main_line3.attach_function(1, update_voice);
    
    menu.add_screen(main_screen);
    
    menu.add_screen(volume_screen);
    menu.add_screen(midichan_screen);
    menu.add_screen(filter_screen);
    menu.add_screen(delay_screen);
    
}

void handle_ui(void)
{
    for (int x = 0; x < NUM_ENCODERS; x++) {
        encoderReading[x] = enc[x].read();
        but[x].update();
        if (but[x].fallingEdge())
            long_button_pressed = 0;
        
        if (but[x].risingEdge())
        {
            uint32_t button_released = long_button_pressed;
            
            if (button_released > LONG_BUTTON_PRESS)
            {
                
            }
            switch (x) {
                case 0:
                    Serial.println(F("SWM button pressed"));
                    menu.next_screen();
                    lastMs_mainScreen = millis();
                    break;
                case 1:
                    Serial.println(F("SEL button pressed"));
                    lastMs_mainScreen = millis();
                    break;
                default:
                    break;
            }
        }
    /*
     * Check if the analog value have changed
     * and update the display if it has.
     */
        if (encoderReading[x] != lastEncoderReading[x]) {

            switch(x) {
                case 0:
                    menu.switch_focus(encoderReading[x] > lastEncoderReading[x]);
                    break;
                case VALUE_ENCODER:
                    menu.call_function(1);
                    menu.update();
                    break;
            }
            lastEncoderReading[x] = encoderReading[x];
            lastMs_mainScreen = millis();
        }
        
       
    
    }
    // Periodic switching to the main screen.
    if (millis() - lastMs_mainScreen > period_mainScreen) {
        lastMs_mainScreen = millis();
        menu.change_screen(main_screen);
    }
}

void handle_ui_change(void)
{
    menu.update();
}

float mapfloat(float val, float in_min, float in_max, float out_min, float out_max)
{
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif
