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

#ifdef I2C_DISPLAY // selecting sounds by encoder, button and display

elapsedMillis ui_back_to_main;

void handle_ui(void)
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

void ui_show_main(void)
{
  if (ui_state != UI_MAIN)
  {
    disp.clear();
  }

  disp.show(0, 0, 2, configuration.bank);
  disp.show(0, 2, 1, " ");
  strip_extension(bank_names[configuration.bank], bank_name);

  if (ui_main_state == UI_MAIN_BANK || ui_main_state == UI_MAIN_BANK_SELECTED)
  {
    disp.show(0, 2, 1, "[");
    disp.show(0, 3, 8, bank_name);
    disp.show(0, 11, 1, "]");
  }
  else
  {
    disp.show(0, 2, 1, " ");
    disp.show(0, 3, 8, bank_name);
    disp.show(0, 11, 1, " ");
  }

  disp.show(1, 0, 2, configuration.voice + 1);
  disp.show(1, 2, 1, " ");
  if (ui_main_state == UI_MAIN_VOICE || ui_main_state == UI_MAIN_VOICE_SELECTED)
  {
    disp.show(1, 2, 1, "[");
    disp.show(1, 3, 10, voice_names[configuration.voice]);
    disp.show(1, 14, 1, "]");
  }
  else
  {
    disp.show(1, 2, 1, " ");
    disp.show(1, 3, 10, voice_names[configuration.voice]);
    disp.show(1, 14, 1, " ");
  }
  //disp.sendBuffer();
  ui_state = UI_MAIN;
}

void ui_show_volume(void)
{
  uint8_t pos;
  static uint8_t old_pos;

  ui_back_to_main = 0;

  // erase old marker and show new marker
  pos = map(configuration.vol * 100 + 0.5, 0, 100, 0, LCD_CHARS - 1);

  if (ui_state != UI_VOLUME)
  {
    disp.clear();
    disp.show(0, 0, LCD_CHARS - 1, "Volume");
    disp.show(1, pos, 1, "*");
    old_pos = pos;
  }

  // show value
  disp.show(0, LCD_CHARS - 3, 3, (configuration.vol * 100 + 0.5));

  if (pos != old_pos)
  {
    disp.show(1, pos, 1, "*");
    disp.show(1, old_pos, 1, " ");
    old_pos = pos;
  }
  //disp.sendBuffer();

  ui_state = UI_VOLUME;
}

void ui_show_midichannel(void)
{
  ui_back_to_main = 0;

  if (ui_state != UI_MIDICHANNEL)
  {
    disp.clear();
    disp.show(0, 0, LCD_CHARS, "MIDI Channel");
  }

  if (configuration.midi_channel == MIDI_CHANNEL_OMNI)
    disp.show(1, 0, 4, "OMNI");
  else
  {
    disp.show(1, 0, 2, configuration.midi_channel);
    if (configuration.midi_channel == 1)
      disp.show(1, 2, 2, "  ");
  }
  //disp.sendBuffer();

  ui_state = UI_MIDICHANNEL;
}

void ui_show_effects_filter(void)
{
  if (ui_state != UI_EFFECTS_FILTER)
  {
    disp.clear();
    disp.show(0, 0, LCD_CHARS, "Filter");
    disp.show(1, 0, 4, "Res:");
    disp.show(1, 8, 4, "Cut:");
  }

  disp.show(1, 5, 2, map(effect_filter_resonance, 0, ENC_FILTER_RES_STEPS, 0, 99));
  disp.show(1, 13, 2, map(effect_filter_cutoff, 0, ENC_FILTER_CUT_STEPS, 0, 99));

  if (ui_main_state == UI_MAIN_FILTER_RES)
  {
    disp.show(1, 4, 1, "[");
    disp.show(1, 7, 1, "]");
  }
  else
  {
    disp.show(1, 4, 1, " ");
    disp.show(1, 7, 1, " ");
  }

  if (ui_main_state == UI_MAIN_FILTER_CUT)
  {
    disp.show(1, 12, 1, "[");
    disp.show(1, 15, 1, "]");
  }
  else
  {
    disp.show(1, 12, 1, " ");
    disp.show(1, 15, 1, " ");
  }
  //disp.sendBuffer();

  ui_state = UI_EFFECTS_FILTER;
}

void ui_show_effects_delay(void)
{
  if (ui_state != UI_EFFECTS_DELAY)
  {
    disp.clear();
    disp.show(0, 0, 5, "Delay");
    disp.show(0, 6, 2, "T:");
    disp.show(0, 14, 2, "ms");
    disp.show(1, 0, 3, "FB:");
    disp.show(1, 8, 5, "Vol:");
  }

  disp.show(0, 9, 4, map(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0, DELAY_MAX_TIME));
  disp.show(1, 4, 2, map(effect_delay_feedback, 0, ENC_DELAY_FB_STEPS, 0, 99));
  disp.show(1, 13, 2, map(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0, 99));

  if (ui_main_state == UI_MAIN_DELAY_TIME)
  {
    disp.show(0, 8, 1, "[");
    disp.show(0, 13, 1, "]");
  }
  else
  {
    disp.show(0, 8, 1, " ");
    disp.show(0, 13, 1, " ");
  }

  if (ui_main_state == UI_MAIN_DELAY_FEEDBACK)
  {
    disp.show(1, 3, 1, "[");
    disp.show(1, 6, 1, "]");
  }
  else
  {
    disp.show(1, 3, 1, " ");
    disp.show(1, 6, 1, " ");
  }

  if (ui_main_state == UI_MAIN_DELAY_VOLUME)
  {
    disp.show(1, 12, 1, "[");
    disp.show(1, 15, 1, "]");
  }
  else
  {
    disp.show(1, 12, 1, " ");
    disp.show(1, 15, 1, " ");
  }
  //disp.sendBuffer();

  ui_state = UI_EFFECTS_DELAY;
}

float mapfloat(float val, float in_min, float in_max, float out_min, float out_max)
{
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
#endif
