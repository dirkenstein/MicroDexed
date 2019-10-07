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

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include "midinotes.h"

// ATTENTION! For better latency you have to redefine AUDIO_BLOCK_SAMPLES from
// 128 to 64 in <ARDUINO-IDE-DIR>/cores/teensy3/AudioStream.h

// If you want to test the system with Linux and withous any keyboard and/or audio equipment, you can do the following:
// 1. In Arduino-IDE enable "Tools->USB-Type->Serial + MIDI + Audio"
// 2. Build the firmware with "MIDI_DEVICE_USB" enabled in config.h.
// 3. Afterconnecting to a Linux system there should be a MIDI an audio device available that is called "MicroMDAEPiano", so you can start the following:
// $ aplaymidi -p 20:0 <MIDI-File> # e.g. test.mid
// $ arecord -f cd -Dhw:1,0 /tmp/bla.wav

#define VERSION "0.9.5"

//*************************************************************************************************
//* DEVICE SETTINGS
//*************************************************************************************************

// MIDI
#define MIDI_DEVICE_DIN Serial1
//#define MIDI_DEVICE_USB 0
//#define MIDI_DEVICE_USB_HOST 0
//#define MIDI_DEVICE_NUMBER 0

// AUDIO
// If nothing is defined PT8211 is used as audio output device!
//#define TEENSY_AUDIO_BOARD 1
#define I2S_AUDIO_ONLY
//#define TGA_AUDIO_BOARD

//#define AUDIO_DEVICE_USB

//*************************************************************************************************
//* MIDI SETTINGS
//*************************************************************************************************

#define DEFAULT_MIDI_CHANNEL MIDI_CHANNEL_OMNI
#define MIDI_MERGE_THRU 1
#define DEFAULT_SYSEXBANK 0
#define DEFAULT_SYSEXSOUND 0

//*************************************************************************************************
//* DEXED AND EFECTS SETTINGS
//*************************************************************************************************
#define DEXED_ENGINE DEXED_ENGINE_MODERN // DEXED_ENGINE_MARKI // DEXED_ENGINE_OPL

// EFFECTS
#define FILTER_MAX_FREQ 10000

//*************************************************************************************************
//* AUDIO SETTINGS
//*************************************************************************************************
// https://rechneronline.de/funktionsgraphen/
#define VOLUME 0.8
#define VOLUME_CURVE 0.07
#ifndef TEENSY_AUDIO_BOARD
#if AUDIO_BLOCK_SAMPLES == 64
#define AUDIO_MEM 450
#else
#define AUDIO_MEM 225
#endif
#define DELAY_MAX_TIME 600.0
#define REDUCE_LOUDNESS 1
#else
#if AUDIO_BLOCK_SAMPLES == 64
#define AUDIO_MEM 900
#else
#define AUDIO_MEM 450
#endif
#define DELAY_MAX_TIME 1200.0
#define REDUCE_LOUDNESS 1
#endif
#define SAMPLE_RATE 44100
#define SOFTEN_VALUE_CHANGE_STEPS 20

//*************************************************************************************************
//* UI AND DATA-STORE SETTINGS
//*************************************************************************************************
#define CONTROL_RATE_MS 50
#define TIMER_UI_HANDLING_MS 100

//*************************************************************************************************
//* DEBUG OUTPUT SETTINGS
//*************************************************************************************************

#define DEBUG 1
#define SERIAL_SPEED 9600
#define SHOW_XRUN 1
#define SHOW_CPU_LOAD_MSEC 5000

//*************************************************************************************************
//* HARDWARE SETTINGS
//*************************************************************************************************

// Teensy Audio Shield:
//#define SDCARD_CS_PIN    10
//#define SDCARD_MOSI_PIN  7
//#define SDCARD_SCK_PIN   14
#define SGTL5000_LINEOUT_LEVEL 29
// Teensy 3.5 & 3.6 SD card
//#define SDCARD_CS_PIN    SS
//#define SDCARD_MOSI_PIN  11  // not actually used
//#define SDCARD_SCK_PIN   13  // not actually used


#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  11  // not actually used
#define SDCARD_SCK_PIN   13  // not actually used

// Encoder with button
#define ENC_VOL_STEPS 43
#define ENC_FILTER_RES_STEPS 100
#define ENC_FILTER_CUT_STEPS 100
#define ENC_DELAY_TIME_STEPS 50
#define ENC_DELAY_FB_STEPS 35
#define ENC_DELAY_VOLUME_STEPS 50
#define NUM_ENCODER 2
#define ENC_L_PIN_A  3
#define ENC_L_PIN_B  2
#define BUT_L_PIN    4
#define INITIAL_ENC_L_VALUE 0
#define ENC_R_PIN_A  6
#define ENC_R_PIN_B  5
#define BUT_R_PIN    8
#define INITIAL_ENC_R_VALUE 0
#define BUT_DEBOUNCE_MS 20
#define LONG_BUTTON_PRESS 500

// LCD Display
#define LCD_DISPLAY 1
#define LCD_U8X8_SPI 1
#define LCD_CS_PIN 9
#define LCD_DC_PIN 15
#define LCD_RESET_PIN 14
//#define LCD_I2C
// [I2C] SCL: Pin 19, SDA: Pin 18 (https://www.pjrc.com/teensy/td_libs_Wire.html)
#define LCD_I2C_ADDRESS 0x3f
#define LCD_CHARS 16
#define LCD_LINES 4
#define UI_AUTO_BACK_MS 3000
#define AUTOSTORE_MS 5000
#define AUTOSTORE_FAST_MS 50

// EEPROM address
#define EEPROM_START_ADDRESS 0

#define MAX_BANKS 100
#define MAX_VOICES 32 // voices per bank
#define BANK_NAME_LEN 13 // FAT12 filenames (plus '\0')
#define VOICE_NAME_LEN 11 // 10 (plus '\0')

//*************************************************************************************************
//* DO NO CHANGE ANYTHING BEYOND IF YOU DON'T KNOW WHAT YOU ARE DOING !!!
//*************************************************************************************************
// MIDI
#ifdef MIDI_DEVICE_USB
#define USBCON 1
#endif
#if defined(__MK66FX1M0__) // Teensy-3.6
// Teensy-3.6 settings
#define MIDI_DEVICE_USB_HOST 1
#define MAX_NOTES 16
#else
// Teensy-3.5 settings
#undef MIDI_DEVICE_USB_HOST
#define MAX_NOTES 11
#endif
#define TRANSPOSE_FIX 24

// Audio
#ifdef TGA_AUDIO_BOARD
#define REDUCE_LOUDNESS 2
#endif

// Some optimizations
#define USE_TEENSY_DSP 1
#define SUM_UP_AS_INT 1

// struct for holding the current configuration
struct config_t {
  uint32_t checksum;
  uint8_t bank;
  uint8_t voice;
  float vol;
  float pan;
  uint8_t midi_channel;
};

// struct for smoothing value changes
struct value_change_t {
  float diff;
  uint16_t steps;
};
#endif // CONFIG_H_INCLUDED
