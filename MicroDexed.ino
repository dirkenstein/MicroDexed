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
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <MIDI.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"
#include "midi_devices.hpp"
#include <limits.h>
#include "dexed.h"
#include "dexed_sysex.h"
#include "PluginFx.h"
#ifdef LCD_DISPLAY // selecting sounds by encoder, button and display
#include "UI.h"
#define BOUNCE_WITH_PROMPT_DETECTION
#include <Bounce.h>
#define ENCODER_DO_NOT_USE_INTERRUPTS
#include "Encoder4.h"

Encoder4 enc[2] = {Encoder4(ENC_L_PIN_A, ENC_L_PIN_B), Encoder4(ENC_R_PIN_A, ENC_R_PIN_B)};
int32_t enc_val[2] = {INITIAL_ENC_L_VALUE, INITIAL_ENC_R_VALUE};
Bounce but[2] = {Bounce(BUT_L_PIN, BUT_DEBOUNCE_MS), Bounce(BUT_R_PIN, BUT_DEBOUNCE_MS)};
elapsedMillis master_timer;
#endif

AudioPlayQueue           queue1;
AudioAnalyzePeak         peak1;
AudioEffectDelay         delay1;
AudioMixer4              mixer1;
AudioMixer4              mixer2;
AudioAmplifier           volume_r;
AudioAmplifier           volume_l;
#if defined(AUDIO_DEVICE_USB)
AudioOutputUSB           usb1;
#endif
AudioConnection          patchCord0(queue1, peak1);
AudioConnection          patchCord1(queue1, 0, mixer1, 0);
AudioConnection          patchCord2(queue1, 0, mixer2, 0);
AudioConnection          patchCord3(delay1, 0, mixer1, 1);
AudioConnection          patchCord4(delay1, 0, mixer2, 2);
AudioConnection          patchCord5(mixer1, delay1);
AudioConnection          patchCord6(mixer1, 0, mixer2, 1);
AudioConnection          patchCord7(mixer2, volume_r);
AudioConnection          patchCord8(mixer2, volume_l);
#if defined(AUDIO_DEVICE_USB)
AudioConnection          patchCord9(mixer2, 0, usb1, 0);
AudioConnection          patchCord10(mixer2, 0, usb1, 1);
#endif

#if defined(TEENSY_AUDIO_BOARD) || defined (I2S_AUDIO_ONLY)
AudioOutputI2S           i2s1;
AudioConnection          patchCord11(volume_r, 0, i2s1, 0);
AudioConnection          patchCord12(volume_l, 0, i2s1, 1);
#endif
#if defined(TEENSY_AUDIO_BOARD)
AudioControlSGTL5000     sgtl5000_1;
#elif defined(TGA_AUDIO_BOARD)
AudioOutputI2S           i2s1;
AudioConnection          patchCord11(volume_r, 0, i2s1, 0);
AudioConnection          patchCord12(volume_l, 0, i2s1, 1);
AudioControlWM8731master wm8731_1;
#elif !defined(I2S_AUDIO_ONLY)
AudioOutputPT8211        pt8211_1;
AudioConnection          patchCord11(volume_r, 0, pt8211_1, 0);
AudioConnection          patchCord12(volume_l, 0, pt8211_1, 1);
#endif

Dexed* dexed = new Dexed(SAMPLE_RATE);
bool sd_card_available = false;
uint32_t xrun = 0;
uint32_t overload = 0;
uint32_t peak = 0;
uint16_t render_time_max = 0;
uint8_t max_loaded_banks = 0;
char bank_name[BANK_NAME_LEN];
char voice_name[VOICE_NAME_LEN];
char bank_names[MAX_BANKS][BANK_NAME_LEN];
char voice_names[MAX_VOICES][VOICE_NAME_LEN];
uint8_t midi_timing_counter = 0; // 24 per qarter
elapsedMillis midi_timing_timestep;
uint16_t midi_timing_quarter = 0;

uint8_t wanted_volume = 0;
uint8_t effect_filter_cutoff = 0;
uint8_t effect_filter_resonance = 0;
uint8_t effect_delay_time = 0;
uint8_t effect_delay_feedback = 0;
uint8_t effect_delay_volume = 0;
bool effect_delay_sync = 0;
elapsedMicros fill_audio_buffer;
elapsedMillis control_rate;
elapsedMillis autostore;
uint8_t active_voices = 0;
#ifdef SHOW_CPU_LOAD_MSEC
elapsedMillis cpu_mem_millis;
#endif
config_t configuration = {0xffff, 0, 0, VOLUME, 0.5f, DEFAULT_MIDI_CHANNEL};
bool eeprom_update_flag = false;
value_change_t soften_volume = {0.0, 0};
value_change_t soften_filter_res = {0.0, 0};
value_change_t soften_filter_cut = {0.0, 0};

void setup()
{
  //while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.begin(SERIAL_SPEED);
#ifdef LCD_DISPLAY
  setup_ui();
  pinMode(BUT_L_PIN, INPUT_PULLUP);
  pinMode(BUT_R_PIN, INPUT_PULLUP);
#endif

  delay(220);
  Serial.println(F("MicroDexed based on https://github.com/asb2m10/dexed"));
  Serial.println(F("(c)2018,2019 H. Wirtz <wirtz@parasitstudio.de>"));
  Serial.println(F("https://codeberg.org/dcoredump/MicroDexed"));
  Serial.print(F("Version: "));
  Serial.println(VERSION);
  Serial.println(F("<setup start>"));

  initial_values_from_eeprom();

  setup_midi_devices();

  // start audio card
  AudioNoInterrupts();
  AudioMemory(AUDIO_MEM);

#ifdef TEENSY_AUDIO_BOARD
  sgtl5000_1.enable();
  sgtl5000_1.dacVolumeRamp();
  //sgtl5000_1.dacVolumeRampLinear();
  //sgtl5000_1.dacVolumeRampDisable();
  sgtl5000_1.unmuteHeadphone();
  sgtl5000_1.unmuteLineout();
  sgtl5000_1.autoVolumeDisable(); // turn off AGC
  sgtl5000_1.volume(0.5, 0.5); // Headphone volume
  sgtl5000_1.lineOutLevel(SGTL5000_LINEOUT_LEVEL);
  sgtl5000_1.audioPostProcessorEnable();
  sgtl5000_1.autoVolumeControl(1, 1, 1, 0.9, 0.01, 0.05);
  sgtl5000_1.autoVolumeEnable();
  sgtl5000_1.surroundSoundEnable();
  sgtl5000_1.surroundSound(7, 2); // Configures virtual surround width from 0 (mono) to 7 (widest). select may be set to 1 (disable), 2 (mono input) or 3 (stereo input).
  sgtl5000_1.enhanceBassEnable();
  sgtl5000_1.enhanceBass(1.0, 0.2, 1, 2); // Configures the bass enhancement by setting the levels of the original stereo signal and the bass-enhanced mono level which will be mixed together. The high-pass filter may be enabled (0) or bypassed (1).
  
  /* The cutoff frequency is specified as follows:
    value  frequency
    0      80Hz
    1     100Hz
    2     125Hz
    3     150Hz
    4     175Hz
    5     200Hz
    6     225Hz
  */
  //sgtl5000_1.eqBands(bass, mid_bass, midrange, mid_treble, treble);
  Serial.println(F("Teensy-Audio-Board enabled."));
#elif defined(TGA_AUDIO_BOARD)
  wm8731_1.enable();
  wm8731_1.volume(1.0);
  Serial.println(F("TGA board enabled."));
#elif defined(I2S_AUDIO_ONLY)
  Serial.println(F("I2S enabled."));
#else
  Serial.println(F("PT8211 enabled."));
#endif

  // start SD card
#ifndef __IMXRT1062__
  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
#endif
  if (!SD.begin(SDCARD_CS_PIN))
  {
    Serial.println(F("SD card not accessable."));
    strcpy(bank_name, "Default");
    strcpy(voice_name, "Default");
  }
  else
  {
    Serial.println(F("SD card found."));
    sd_card_available = true;

    // read all bank names
    max_loaded_banks = get_bank_names();
    strip_extension(bank_names[configuration.bank], bank_name);

    // read all voice name for actual bank
    get_voice_names_from_bank(configuration.bank);
#ifdef DEBUG
    Serial.print(F("Bank ["));
    Serial.print(bank_names[configuration.bank]);
    Serial.print(F("/"));
    Serial.print(bank_name);
    Serial.println(F("]"));
    for (uint8_t n = 0; n < MAX_VOICES; n++)
    {
      if (n < 10)
        Serial.print(F(" "));
      Serial.print(F("   "));
      Serial.print(n, DEC);
      Serial.print(F("["));
      Serial.print(voice_names[n]);
      Serial.println(F("]"));
    }
#endif

    // load default SYSEX data
    load_sysex(configuration.bank, configuration.voice);
  }

  // Init effects
  delay1.delay(0, mapfloat(effect_delay_feedback, 0, ENC_DELAY_TIME_STEPS, 0.0, DELAY_MAX_TIME));
  // mixer1 is the feedback-adding mixer, mixer2 the whole delay (with/without feedback) mixer
  mixer1.gain(0, 1.0); // original signal
  mixer1.gain(1, mapfloat(effect_delay_feedback, 0, ENC_DELAY_FB_STEPS, 0.0, 1.0)); // amount of feedback
  mixer2.gain(0, 1.0 - mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0)); // original signal
  mixer2.gain(1, mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0)); // delayed signal (including feedback)
  mixer2.gain(2, mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0)); // only delayed signal (without feedback)
  dexed->fx.Gain =  1.0;
  dexed->fx.Reso = 1.0 - float(effect_filter_resonance) / ENC_FILTER_RES_STEPS;
  dexed->fx.Cutoff = 1.0 - float(effect_filter_cutoff) / ENC_FILTER_CUT_STEPS;

  // set initial volume and pan (read from EEPROM)
  set_volume(configuration.vol, configuration.pan);

#ifdef LCD_DISPLAY
  wanted_volume = map(configuration.vol * 100, 0, 100, 0, ENC_VOL_STEPS);
  //enc[0].write(map(configuration.vol * 100, 0, 100, 0, ENC_VOL_STEPS));
  enc_val[0] = enc[0].read();
  //enc[1].write(configuration.voice);
  enc_val[1] = enc[1].read();
  but[0].update();
  but[1].update();
#endif

#if defined (DEBUG) && defined (SHOW_CPU_LOAD_MSEC)
  // Initialize processor and memory measurements
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
#endif

#ifdef DEBUG
  Serial.print(F("Bank/Voice from EEPROM ["));
  Serial.print(configuration.bank, DEC);
  Serial.print(F("/"));
  Serial.print(configuration.voice, DEC);
  Serial.println(F("]"));
  show_patch();
#endif

  Serial.print(F("AUDIO_BLOCK_SAMPLES="));
  Serial.print(AUDIO_BLOCK_SAMPLES);
  Serial.print(F(" (Time per block="));
  Serial.print(1000000 / (SAMPLE_RATE / AUDIO_BLOCK_SAMPLES));
  Serial.println(F("ms)"));

#if defined (DEBUG) && defined (SHOW_CPU_LOAD_MSEC)
  show_cpu_and_mem_usage();
#endif

  AudioInterrupts();
  Serial.println(F("<setup end>"));
}

void loop()
{
  int16_t* audio_buffer; // pointer to AUDIO_BLOCK_SAMPLES * int16_t
  const uint16_t audio_block_time_us = 1000000 / (SAMPLE_RATE / AUDIO_BLOCK_SAMPLES);

  // Main sound calculation
  if (queue1.available() && fill_audio_buffer > audio_block_time_us - 10)
  {
    fill_audio_buffer = 0;

    audio_buffer = queue1.getBuffer();

    elapsedMicros t1;
    dexed->getSamples(AUDIO_BLOCK_SAMPLES, audio_buffer);
    if (t1 > audio_block_time_us) // everything greater 2.9ms is a buffer underrun!
      xrun++;
    if (t1 > render_time_max)
      render_time_max = t1;
    if (peak1.available())
    {
      if (peak1.read() > 0.99)
        peak++;
    }
#ifndef TEENSY_AUDIO_BOARD
    for (uint8_t i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
      audio_buffer[i] *= configuration.vol;
#endif
    queue1.playBuffer();
  }

  // EEPROM update handling
  if (autostore >= AUTOSTORE_MS && active_voices == 0 && eeprom_update_flag == true)
  {
    // only store configuration data to EEPROM when AUTOSTORE_MS is reached and no voices are activated anymore
    eeprom_update();
  }

  // MIDI input handling
  check_midi_devices();

  // CONTROL-RATE-EVENT-HANDLING
  if (control_rate > CONTROL_RATE_MS)
  {
    control_rate = 0;

    // Shutdown unused voices
    active_voices = dexed->getNumNotesPlaying();

    // check for value changes
    if (soften_volume.steps > 0)
    {
      // soften volume value
      soften_volume.steps--;
      set_volume(configuration.vol + soften_volume.diff, configuration.pan);
#ifdef DEBUG
      Serial.print(F("Volume: "));
      Serial.print(configuration.vol, 5);
      Serial.print(F(" Volume step: "));
      Serial.print(soften_volume.steps);
      Serial.print(F(" Volume diff: "));
      Serial.println(soften_volume.diff, 5);
#endif
    }
    if (soften_filter_res.steps > 0)
    {
      // soften filter resonance value
      soften_filter_res.steps--;
      dexed->fx.Reso = dexed->fx.Reso + soften_filter_res.diff;
#ifdef DEBUG
      Serial.print(F("Filter-Resonance: "));
      Serial.print(dexed->fx.Reso, 5);
      Serial.print(F(" Filter-Resonance step: "));
      Serial.print(soften_filter_res.steps);
      Serial.print(F(" Filter-Resonance diff: "));
      Serial.println(soften_filter_res.diff, 5);
#endif
    }
    if (soften_filter_cut.steps > 0)
    {
      // soften filter cutoff value
      soften_filter_cut.steps--;
      dexed->fx.Cutoff = dexed->fx.Cutoff + soften_filter_cut.diff;
#ifdef DEBUG
      Serial.print(F("Filter-Cutoff: "));
      Serial.print(dexed->fx.Cutoff, 5);
      Serial.print(F(" Filter-Cutoff step: "));
      Serial.print(soften_filter_cut.steps);
      Serial.print(F(" Filter-Cutoff diff: "));
      Serial.println(soften_filter_cut.diff, 5);
#endif
    }
  }

#ifdef LCD_DISPLAY
  // UI-HANDLING
  if (master_timer >= TIMER_UI_HANDLING_MS)
  {
    master_timer -= TIMER_UI_HANDLING_MS;

    handle_ui();
  }
#endif

#if defined (DEBUG) && defined (SHOW_CPU_LOAD_MSEC)
  if (cpu_mem_millis >= SHOW_CPU_LOAD_MSEC)
  {
    cpu_mem_millis -= SHOW_CPU_LOAD_MSEC;
    show_cpu_and_mem_usage();
  }
#endif
}

/******************************************************************************
   MIDI MESSAGE HANDLER
 ******************************************************************************/
void handleNoteOn(byte inChannel, byte inNumber, byte inVelocity)
{
  if (checkMidiChannel(inChannel))
  {
    dexed->keydown(inNumber, inVelocity);
  }
}

void handleNoteOff(byte inChannel, byte inNumber, byte inVelocity)
{
  if (checkMidiChannel(inChannel))
  {
    dexed->keyup(inNumber);
  }
}

void handleControlChange(byte inChannel, byte inCtrl, byte inValue)
{
  if (checkMidiChannel(inChannel))
  {
#ifdef DEBUG
    Serial.print(F("CC#"));
    Serial.print(inCtrl, DEC);
    Serial.print(F(":"));
    Serial.println(inValue, DEC);
#endif

    switch (inCtrl) {
      case 0:
        if (inValue < MAX_BANKS)
        {
          configuration.bank = inValue;
          handle_ui();
          handle_ui_change();
        }
        break;
      case 1:
        dexed->controllers.modwheel_cc = inValue;
        dexed->controllers.refresh();
        break;
      case 2:
        dexed->controllers.breath_cc = inValue;
        dexed->controllers.refresh();
        break;
      case 4:
        dexed->controllers.foot_cc = inValue;
        dexed->controllers.refresh();
        break;
      case 7: // Volume
        configuration.vol = float(inValue) / 0x7f;
        set_volume(configuration.vol, configuration.pan);
        break;
      case 10: // Pan
        configuration.pan = float(inValue) / 128;
        set_volume(configuration.vol, configuration.pan);
        break;
      case 32: // BankSelect LSB
        configuration.bank = inValue;
        break;
      case 64:
        dexed->setSustain(inValue > 63);
        if (!dexed->getSustain()) {
          for (uint8_t note = 0; note < dexed->getMaxNotes(); note++) {
            if (dexed->voices[note].sustained && !dexed->voices[note].keydown) {
              dexed->voices[note].dx7_note->keyup();
              dexed->voices[note].sustained = false;
            }
          }
        }
        break;
      case 103:  // CC 103: filter resonance
        effect_filter_resonance = map(inValue, 0, 127, 0, ENC_FILTER_RES_STEPS);
        dexed->fx.Reso = 1.0 - float(effect_filter_resonance) / ENC_FILTER_RES_STEPS;
        handle_ui();
        handle_ui_change();
        break;
      case 104:  // CC 104: filter cutoff
        effect_filter_cutoff = map(inValue, 0, 127, 0, ENC_FILTER_CUT_STEPS);
        dexed->fx.Cutoff = 1.0 - float(effect_filter_cutoff) / ENC_FILTER_CUT_STEPS;
        handle_ui();
        handle_ui_change();
        break;
      case 105:  // CC 105: delay time
        effect_delay_time = map(inValue, 0, 127, 0, ENC_DELAY_TIME_STEPS);
        delay1.delay(0, mapfloat(effect_delay_time, 0, ENC_DELAY_TIME_STEPS, 0.0, DELAY_MAX_TIME));
        handle_ui();
        handle_ui_change();
        break;
      case 106:  // CC 106: delay feedback
        effect_delay_feedback = map(inValue, 0, 127, 0, ENC_DELAY_FB_STEPS);
        mixer1.gain(1, mapfloat(float(effect_delay_feedback), 0, ENC_DELAY_FB_STEPS, 0.0, 1.0));
        handle_ui();
        handle_ui_change();
        break;
      case 107:  // CC 107: delay volume
        effect_delay_volume = map(inValue, 0, 127, 0, ENC_DELAY_VOLUME_STEPS);
        mixer2.gain(1, mapfloat(effect_delay_volume, 0, ENC_DELAY_VOLUME_STEPS, 0.0, 1.0)); // delay tap1 signal (with added feedback)
        handle_ui();
        handle_ui_change();
        break;
      case 120:
        dexed->panic();
        break;
      case 121:
        dexed->resetControllers();
        break;
      case 123:
        dexed->notesOff();
        break;
      case 126:
        dexed->setMonoMode(true);
        break;
      case 127:
        dexed->setMonoMode(false);
        break;
    }
  }
}

void handleAfterTouch(byte inChannel, byte inPressure)
{
  dexed->controllers.aftertouch_cc = inPressure;
  dexed->controllers.refresh();
}

void handlePitchBend(byte inChannel, int inPitch)
{
  dexed->controllers.values_[kControllerPitch] = inPitch + 0x2000; // -8192 to +8191 --> 0 to 16383
}

void handleProgramChange(byte inChannel, byte inProgram)
{
  if (inProgram < MAX_VOICES)
  {
    load_sysex(configuration.bank, inProgram);
    handle_ui();
    handle_ui_change();
  }
}

void handleSystemExclusive(byte * sysex, uint len)
{
  /*
    SYSEX MESSAGE: Parameter Change
    -------------------------------
       bits    hex  description

     11110000  F0   Status byte - start sysex
     0iiiiiii  43   ID # (i=67; Yamaha)
     0sssnnnn  10   Sub-status (s=1) & channel number (n=0; ch 1)
     0gggggpp  **   parameter group # (g=0; voice, g=2; function)
     0ppppppp  **   parameter # (these are listed in next section)
                     Note that voice parameter #'s can go over 128 so
                     the pp bits in the group byte are either 00 for
                     par# 0-127 or 01 for par# 128-155. In the latter case
                     you add 128 to the 0ppppppp byte to compute par#.
     0ddddddd  **   data byte
     11110111  F7   Status - end sysex
  */

#ifdef DEBUG
  Serial.print(F("SYSEX-Data["));
  Serial.print(len, DEC);
  Serial.print(F("]"));
  for (uint8_t i = 0; i < len; i++)
  {
    Serial.print(F(" "));
    Serial.print(sysex[i], DEC);
  }
  Serial.println();
#endif

  if (!checkMidiChannel((sysex[2] & 0x0f) + 1 ))
  {
#ifdef DEBUG
    Serial.println(F("SYSEX-MIDI-Channel mismatch"));
#endif
    return;
  }

  if (sysex[1] != 0x43) // check for Yamaha sysex
  {
#ifdef DEBUG
    Serial.println(F("E: SysEx vendor not Yamaha."));
#endif
    return;
  }

#ifdef DEBUG
  Serial.print(F("Substatus: ["));
  Serial.print((sysex[2] & 0x70) >> 4);
  Serial.println(F("]"));
#endif

  // parse parameter change
  if (len == 7)
  {
    if (((sysex[3] & 0x7c) >> 2) != 0 && ((sysex[3] & 0x7c) >> 2) != 2)
    {
#ifdef DEBUG
      Serial.println(F("E: Not a SysEx parameter or function parameter change."));
#endif
      return;
    }
    if (sysex[6] != 0xf7)
    {
#ifdef DEBUG
      Serial.println(F("E: SysEx end status byte not detected."));
#endif
      return;
    }

    sysex[4] &= 0x7f;
    sysex[5] &= 0x7f;

    uint8_t data_index;

    if (((sysex[3] & 0x7c) >> 2) == 0)
    {
      dexed->notesOff();
      dexed->data[sysex[4] + ((sysex[3] & 0x03) * 128)] = sysex[5]; // set parameter
      dexed->doRefreshVoice();
      data_index = sysex[4] + ((sysex[3] & 0x03) * 128);
    }
    else
    {
      dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET - 63 + sysex[4]] = sysex[5]; // set function parameter
      dexed->controllers.values_[kControllerPitchRange] = dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_PITCHBEND_RANGE];
      dexed->controllers.values_[kControllerPitchStep] = dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_PITCHBEND_STEP];
      dexed->controllers.wheel.setRange(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_MODWHEEL_RANGE]);
      dexed->controllers.wheel.setTarget(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_MODWHEEL_ASSIGN]);
      dexed->controllers.foot.setRange(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_FOOTCTRL_RANGE]);
      dexed->controllers.foot.setTarget(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_FOOTCTRL_ASSIGN]);
      dexed->controllers.breath.setRange(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_BREATHCTRL_RANGE]);
      dexed->controllers.breath.setTarget(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_BREATHCTRL_ASSIGN]);
      dexed->controllers.at.setRange(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_AT_RANGE]);
      dexed->controllers.at.setTarget(dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_AT_ASSIGN]);
      dexed->controllers.masterTune = (dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_MASTER_TUNE] * 0x4000 << 11) * (1.0 / 12);
      dexed->controllers.refresh();
      data_index = DEXED_GLOBAL_PARAMETER_OFFSET - 63 + sysex[4];
    }
#ifdef DEBUG
    Serial.print(F("SysEx"));
    if (((sysex[3] & 0x7c) >> 2) == 0)
      Serial.print(F(" function"));
    Serial.print(F(" parameter "));
    Serial.print(sysex[4], DEC);
    Serial.print(F(" = "));
    Serial.print(sysex[5], DEC);
    Serial.print(F(", data_index = "));
    Serial.println(data_index, DEC);
#endif
  }
  else if (len == 163)
  {
    int32_t bulk_checksum_calc = 0;
    int8_t bulk_checksum = sysex[161];

    // 1 Voice bulk upload
#ifdef DEBUG
    Serial.println(F("1 Voice bulk upload"));
#endif

    if (sysex[162] != 0xf7)
    {
#ifdef DEBUG
      Serial.println(F("E: Found no SysEx end marker."));
#endif
      return;
    }

    if ((sysex[3] & 0x7f) != 0)
    {
#ifdef DEBUG
      Serial.println(F("E: Not a SysEx voice bulk upload."));
#endif
      return;
    }

    if (((sysex[4] << 7) | sysex[5]) != 0x9b)
    {
#ifdef DEBUG
      Serial.println(F("E: Wrong length for SysEx voice bulk upload (not 155)."));
#endif
      return;
    }

    // checksum calculation
    for (uint8_t i = 0; i < 155 ; i++)
    {
      bulk_checksum_calc -= sysex[i + 6];
    }
    bulk_checksum_calc &= 0x7f;

    if (bulk_checksum_calc != bulk_checksum)
    {
#ifdef DEBUG
      Serial.print(F("E: Checksum error for one voice [0x"));
      Serial.print(bulk_checksum, HEX);
      Serial.print(F("/0x"));
      Serial.print(bulk_checksum_calc, HEX);
      Serial.println(F("]"));
#endif
      return;
    }

    // load sysex-data into voice memory
    uint8_t tmp_data[155];
    memset(tmp_data, 0, 155 * sizeof(uint8_t));

    for (uint8_t i = 0; i < 155 ; i++)
    {
      tmp_data[i] = sysex[i + 6];
    }
    strncpy(voice_name, (char *)&tmp_data[145], sizeof(voice_name) - 1);
    Serial.print(F("Voice ["));
    Serial.print(voice_name);
    Serial.print(F("] loaded."));
    
    dexed->loadSysexVoice(tmp_data);
  }
#ifdef DEBUG
  else
    Serial.println(F("E: SysEx parameter length wrong."));
#endif
}

void handleTimeCodeQuarterFrame(byte data)
{
  ;
}

void handleAfterTouchPoly(byte inChannel, byte inNumber, byte inVelocity)
{
  ;
}

void handleSongSelect(byte inSong)
{
  ;
}

void handleTuneRequest(void)
{
  ;
}

void handleClock(void)
{
  midi_timing_counter++;
  if (midi_timing_counter % 24 == 0)
  {
    midi_timing_quarter = midi_timing_timestep;
    midi_timing_counter = 0;
    midi_timing_timestep = 0;
    // Adjust delay control here
#ifdef DEBUG
    Serial.print(F("MIDI Clock: "));
    Serial.print(60000 / midi_timing_quarter, DEC);
    Serial.print(F("bpm ("));
    Serial.print(midi_timing_quarter, DEC);
    Serial.println(F("ms per quarter)"));
#endif
  }
}

void handleStart(void)
{
  ;
}

void handleContinue(void)
{
  ;
}

void handleStop(void)
{
  ;
}

void handleActiveSensing(void)
{
  ;
}

void handleSystemReset(void)
{
#ifdef DEBUG
  Serial.println(F("MIDI SYSEX RESET"));
#endif
  dexed->notesOff();
  dexed->panic();
  dexed->resetControllers();
}

/******************************************************************************
   MIDI HELPER
 ******************************************************************************/

bool checkMidiChannel(byte inChannel)
{
  // check for MIDI channel
  if (configuration.midi_channel == MIDI_CHANNEL_OMNI)
  {
    return (true);
  }
  else if (inChannel != configuration.midi_channel)
  {
#ifdef DEBUG
    Serial.print(F("Ignoring MIDI data on channel "));
    Serial.print(inChannel);
    Serial.print(F("(listening on "));
    Serial.print(configuration.midi_channel);
    Serial.println(F(")"));
#endif
    return (false);
  }
  return (true);
}

/******************************************************************************
   VOLUME HELPER
 ******************************************************************************/

void set_volume(float v, float p)
{
  configuration.vol = v;
  configuration.pan = p;

#ifdef DEBUG
  Serial.print(F("Setting volume: VOL="));
  Serial.print(v, DEC);
  Serial.print(F("["));
  Serial.print(configuration.vol, DEC);
  Serial.print(F("] PAN="));
  Serial.print(F("["));
  Serial.print(configuration.pan, DEC);
  Serial.print(F("] "));
  Serial.print(pow(configuration.vol * sinf(configuration.pan * PI / 2), VOLUME_CURVE), 3);
  Serial.print(F("/"));
  Serial.println(pow(configuration.vol * cosf( configuration.pan * PI / 2), VOLUME_CURVE), 3);
#endif

  dexed->fx.Gain = v;

  // http://files.csound-tutorial.net/floss_manual/Release03/Cs_FM_03_ScrapBook/b-panning-and-spatialization.html
  volume_r.gain(sinf(p * PI / 2));
  volume_l.gain(cosf(p * PI / 2));
}

// https://www.dr-lex.be/info-stuff/volumecontrols.html#table1
inline float logvol(float x)
{
  return (0.001 * expf(6.908 * x));
}


/******************************************************************************
   EEPROM HELPER
 ******************************************************************************/

void initial_values_from_eeprom(void)
{
  uint32_t checksum;
  config_t tmp_conf;

  EEPROM_readAnything(EEPROM_START_ADDRESS, tmp_conf);
  checksum = crc32((byte*)&tmp_conf + 4, sizeof(tmp_conf) - 4);

#ifdef DEBUG
  Serial.print(F("EEPROM checksum: 0x"));
  Serial.print(tmp_conf.checksum, HEX);
  Serial.print(F(" / 0x"));
  Serial.print(checksum, HEX);
#endif

  if (checksum != tmp_conf.checksum)
  {
#ifdef DEBUG
    Serial.print(F(" - mismatch -> initializing EEPROM!"));
#endif
    eeprom_update();
  }
  else
  {
    EEPROM_readAnything(EEPROM_START_ADDRESS, configuration);
    Serial.print(F(" - OK, loading!"));
  }
#ifdef DEBUG
  Serial.println();
#endif
}

void eeprom_write(void)
{
  autostore = 0;
  eeprom_update_flag = true;
}

void eeprom_update(void)
{
  eeprom_update_flag = false;
  configuration.checksum = crc32((byte*)&configuration + 4, sizeof(configuration) - 4);
  EEPROM_writeAnything(EEPROM_START_ADDRESS, configuration);
  Serial.println(F("Updating EEPROM with configuration data"));
}

uint32_t crc32(byte * calc_start, uint16_t calc_bytes) // base code from https://www.arduino.cc/en/Tutorial/EEPROMCrc
{
  const uint32_t crc_table[16] =
  {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };
  uint32_t crc = ~0L;

  for (byte* index = calc_start ; index < (calc_start + calc_bytes) ; ++index)
  {
    crc = crc_table[(crc ^ *index) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (*index >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }

  return (crc);
}

/******************************************************************************
   DEBUG HELPER
 ******************************************************************************/

#if defined (DEBUG) && defined (SHOW_CPU_LOAD_MSEC)
void show_cpu_and_mem_usage(void)
{
  Serial.print(F("CPU: "));
  Serial.print(AudioProcessorUsage(), 2);
  Serial.print(F("%   CPU MAX: "));
  Serial.print(AudioProcessorUsageMax(), 2);
  Serial.print(F("%  MEM: "));
  Serial.print(AudioMemoryUsage(), DEC);
  Serial.print(F("   MEM MAX: "));
  Serial.print(AudioMemoryUsageMax(), DEC);
  Serial.print(F("   RENDER_TIME_MAX: "));
  Serial.print(render_time_max, DEC);
  Serial.print(F("   XRUN: "));
  Serial.print(xrun, DEC);
  Serial.print(F("   OVERLOAD: "));
  Serial.print(overload, DEC);
  Serial.print(F("   PEAK: "));
  Serial.print(peak, DEC);
  Serial.print(F(" BLOCKSIZE: "));
  Serial.print(AUDIO_BLOCK_SAMPLES, DEC);
  Serial.print(F(" ACTIVE_VOICES: "));
  Serial.print(active_voices, DEC);
  Serial.println();
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
  render_time_max = 0;
}
#endif

#ifdef DEBUG
void show_patch(void)
{
  uint8_t i;
  char voicename[VOICE_NAME_LEN];

  memset(voicename, 0, sizeof(voicename));
  for (i = 0; i < 6; i++)
  {
    Serial.print(F("OP"));
    Serial.print(6 - i, DEC);
    Serial.println(F(": "));
    Serial.println(F("R1 | R2 | R3 | R4 | L1 | L2 | L3 | L4 LEV_SCL_BRK_PT | SCL_LEFT_DEPTH | SCL_RGHT_DEPTH"));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_R1], DEC);
    Serial.print(F(" "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_R2], DEC);
    Serial.print(F(" "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_R3], DEC);
    Serial.print(F(" "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_R4], DEC);
    Serial.print(F(" "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_L1], DEC);
    Serial.print(F(" "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_L2], DEC);
    Serial.print(F(" "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_L3], DEC);
    Serial.print(F(" "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_EG_L4], DEC);
    Serial.print(F("        "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_LEV_SCL_BRK_PT], DEC);
    Serial.print(F("             "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_SCL_LEFT_DEPTH], DEC);
    Serial.print(F("             "));
    Serial.println(dexed->data[(i * 21) + DEXED_OP_SCL_RGHT_DEPTH], DEC);
    Serial.println(F("SCL_L_CURVE | SCL_R_CURVE | RT_SCALE | AMS | KVS | OUT_LEV | OP_MOD | FRQ_C | FRQ_F | DETUNE"));
    Serial.print(F("      "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_SCL_LEFT_CURVE], DEC);
    Serial.print(F("         "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_SCL_RGHT_CURVE], DEC);
    Serial.print(F("         "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_OSC_RATE_SCALE], DEC);
    Serial.print(F("        "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_AMP_MOD_SENS], DEC);
    Serial.print(F("     "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_KEY_VEL_SENS], DEC);
    Serial.print(F("      "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_OUTPUT_LEV], DEC);
    Serial.print(F("      "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_OSC_MODE], DEC);
    Serial.print(F("    "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_FREQ_COARSE], DEC);
    Serial.print(F("     "));
    Serial.print(dexed->data[(i * 21) + DEXED_OP_FREQ_FINE], DEC);
    Serial.print(F("     "));
    Serial.println(dexed->data[(i * 21) + DEXED_OP_OSC_DETUNE], DEC);
  }
  Serial.println(F("PR1 | PR2 | PR3 | PR4 | PL1 | PL2 | PL3 | PL4"));
  Serial.print(F(" "));
  for (i = 0; i < 8; i++)
  {
    Serial.print(dexed->data[DEXED_VOICE_OFFSET + i], DEC);
    Serial.print(F("  "));
  }
  Serial.println();
  Serial.print(F("ALG: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_ALGORITHM], DEC);
  Serial.print(F("FB: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_FEEDBACK], DEC);
  Serial.print(F("OKS: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_OSC_KEY_SYNC], DEC);
  Serial.print(F("LFO SPD: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_LFO_SPEED], DEC);
  Serial.print(F("LFO_DLY: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_LFO_DELAY], DEC);
  Serial.print(F("LFO PMD: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_LFO_PITCH_MOD_DEP], DEC);
  Serial.print(F("LFO_AMD: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_LFO_AMP_MOD_DEP], DEC);
  Serial.print(F("LFO_SYNC: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_LFO_SYNC], DEC);
  Serial.print(F("LFO_WAVEFRM: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_LFO_WAVE], DEC);
  Serial.print(F("LFO_PMS: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_LFO_PITCH_MOD_SENS], DEC);
  Serial.print(F("TRNSPSE: "));
  Serial.println(dexed->data[DEXED_VOICE_OFFSET + DEXED_TRANSPOSE], DEC);
  Serial.print(F("NAME: "));
  strncpy(voicename, (char *)&dexed->data[DEXED_VOICE_OFFSET + DEXED_NAME], sizeof(voicename) - 1);
  Serial.print(F("["));
  Serial.print(voicename);
  Serial.println(F("]"));
  for (i = DEXED_GLOBAL_PARAMETER_OFFSET; i <= DEXED_GLOBAL_PARAMETER_OFFSET + DEXED_MAX_NOTES; i++)
  {
    Serial.print(i, DEC);
    Serial.print(F(": "));
    Serial.println(dexed->data[i]);
  }

  Serial.println();
}
#endif
