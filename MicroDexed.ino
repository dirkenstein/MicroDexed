/*
   MicroDexed

   MicroDexed is a port of the Dexed sound engine
   (https://github.com/asb2m10/dexed) for the Teensy-3.5/3.6 with audio shield

   (c)2018 H. Wirtz <wirtz@parasitstudio.de>

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

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <MIDI.h>
#include "dexed.h"
#include "dexed_sysex.h"
#include "config.h"
#ifdef USE_ONBOARD_USB_HOST
#include <USBHost_t36.h>
#endif

// GUItool: begin automatically generated code
AudioPlayQueue           queue1;         //xy=84,294
AudioOutputI2S           i2s1;           //xy=961,276
AudioConnection          patchCord2(queue1, 0, i2s1, 0);
AudioConnection          patchCord3(queue1, 0, i2s1, 1);
AudioControlSGTL5000     sgtl5000_1;     //xy=507,403
// GUItool: end automatically generated code

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
Dexed* dexed = new Dexed(SAMPLE_RATE);
IntervalTimer sched;
#ifdef USE_ONBOARD_USB_HOST
USBHost usb_host;
MIDIDevice midi_usb(usb_host);
#endif

bool sd_card_available = false;
bool master_key_enabled = false;
#ifdef TEST_MIDI
IntervalTimer sched_note_on;
IntervalTimer sched_note_off;
uint8_t _voice_counter = 0;
#endif

void setup()
{
  //while (!Serial) ; // wait for Arduino Serial Monitor
  Serial.begin(SERIAL_SPEED);
  delay(200);
  Serial.println(F("MicroDexed based on https://github.com/asb2m10/dexed"));
  Serial.println(F("(c)2018 H. Wirtz"));
  Serial.println(F("setup start"));

  // strat up USB host
#ifdef USE_ONBOARD_USB_HOST
  usb_host.begin();
#endif

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!SD.begin(SDCARD_CS_PIN))
  {
    Serial.println(F("SD card not accessable"));
  }
  else
  {
    Serial.println(F("SD card found."));
    sd_card_available = true;
  }

  MIDI.begin(MIDI_CHANNEL_OMNI);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(AUDIO_MEM);

  sgtl5000_1.enable();
  sgtl5000_1.volume(VOLUME);

#ifdef SHOW_CPU_LOAD_MSEC
  // Initialize processor and memory measurements
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
#endif

  //dexed->activate();
  //dexed->setMaxNotes(MAX_NOTES);
  //dexed->setEngineType(DEXED_ENGINE);

  sched.begin(cleanup, SHOW_CPU_LOAD_MSEC * 1000);

  Serial.print(F("AUDIO_BLOCK_SAMPLES="));
  Serial.println(AUDIO_BLOCK_SAMPLES);

#ifdef TEST_MIDI
  Serial.println(F("MIDI test enabled"));
  sched_note_on.begin(note_on, 2000000);
  sched_note_off.begin(note_off, 6333333);
#endif

  Serial.println(F("setup end"));
  cpu_and_mem_usage();

#ifdef TEST_MIDI
  //dexed->data[DEXED_VOICE_OFFSET+DEXED_LFO_PITCH_MOD_DEP] = 99;           // full pitch mod depth
  //dexed->data[DEXED_VOICE_OFFSET+DEXED_LFO_PITCH_MOD_SENS] = 99;          // full pitch mod sense
  //dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET+DEXED_MODWHEEL_ASSIGN] = 7;   // mod wheel assign all
  //dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET+DEXED_FOOTCTRL_ASSIGN] = 7;   // foot ctrl assign all
  //dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET+DEXED_BREATHCTRL_ASSIGN] = 7; // breath ctrl assign all
  //dexed->data[DEXED_GLOBAL_PARAMETER_OFFSET+AT_ASSIGN] = 7;               // at ctrl assign all
  //queue_midi_event(0xb0, 1, 99); // test mod wheel
  //queue_midi_event(0xb0, 2, 99); // test breath ctrl
  //queue_midi_event(0xb0, 4, 99); // test food switch
  //queue_midi_event(0xd0, 4, 99); // test at
  //queue_midi_event(0xe0, 0xff, 0xff); // test pitch bend
#endif
}

void loop()
{
  int16_t* audio_buffer; // pointer to 128 * int16_t (=256 bytes!)
  bool break_for_calculation = false;

  while (42 == 42) // DON'T PANIC!
  {
#ifdef USE_ONBOARD_USB_HOST
    usb_host.Task();
#endif
    audio_buffer = queue1.getBuffer();
    if (audio_buffer == NULL)
    {
      Serial.println(F("E: audio_buffer allocation problems!"));
    }

#ifdef USE_ONBOARD_USB_HOST
    while (midi_usb.read())
    {
      break_for_calculation = queue_midi_event(midi_usb.getType(), midi_usb.getData1(), midi_usb.getData2());
      if (break_for_calculation == true)
        break;
    }
    if (!break_for_calculation)
    {
#endif
      while (MIDI.read())
      {
        if (MIDI.getType() == 0xF0) // SysEX
        {
          handle_sysex_parameter(MIDI.getSysExArray(), MIDI.getSysExArrayLength());
        }
        else
        {
          break_for_calculation = queue_midi_event(MIDI.getType(), MIDI.getData1(), MIDI.getData2());
          if (break_for_calculation == true)
            break;
        }
      }
#ifdef USE_ONBOARD_USB_HOST
    }
#endif

    if (!queue1.available())
      continue;

#if defined(SHOW_DEXED_TIMING) || defined(SHOW_XRUN)
    elapsedMicros t1;
#endif
    dexed->getSamples(AUDIO_BLOCK_SAMPLES, audio_buffer);
#ifdef SHOW_XRUN
    uint32_t t2 = t1;
    if (t2 > 2900) // everything greater 2.9ms is a buffer underrun!
      Serial.println(F("XRUN"));
#endif
#ifdef SHOW_DEXED_TIMING
    Serial.println(t1, DEC);
#endif
    queue1.playBuffer();
  }
}

#ifdef TEST_MIDI
void note_on(void)
{
  randomSeed(analogRead(A0));
  queue_midi_event(0x90, TEST_NOTE, random(TEST_VEL_MIN, TEST_VEL_MAX));
  // 1
  queue_midi_event(0x90, TEST_NOTE + 5, random(TEST_VEL_MIN, TEST_VEL_MAX));       // 2
  queue_midi_event(0x90, TEST_NOTE + 8, random(TEST_VEL_MIN, TEST_VEL_MAX));       // 3
  queue_midi_event(0x90, TEST_NOTE + 12, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 4
  queue_midi_event(0x90, TEST_NOTE + 17, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 5
  queue_midi_event(0x90, TEST_NOTE + 20, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 6
  queue_midi_event(0x90, TEST_NOTE + 24, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 7
  queue_midi_event(0x90, TEST_NOTE + 29, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 8
  queue_midi_event(0x90, TEST_NOTE + 32, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 9
  queue_midi_event(0x90, TEST_NOTE + 37, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 10
  queue_midi_event(0x90, TEST_NOTE + 40, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 11
  queue_midi_event(0x90, TEST_NOTE + 44, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 12
  queue_midi_event(0x90, TEST_NOTE + 49, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 13
  queue_midi_event(0x90, TEST_NOTE + 52, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 14
  queue_midi_event(0x90, TEST_NOTE + 57, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 15
  queue_midi_event(0x90, TEST_NOTE + 60, random(TEST_VEL_MIN, TEST_VEL_MAX));      // 16
}

void note_off(void)
{
  queue_midi_event(0x80, TEST_NOTE, 0);           // 1
  queue_midi_event(0x80, TEST_NOTE + 5, 0);       // 2
  queue_midi_event(0x80, TEST_NOTE + 8, 0);       // 3
  queue_midi_event(0x80, TEST_NOTE + 12, 0);      // 4
  queue_midi_event(0x80, TEST_NOTE + 17, 0);      // 5
  queue_midi_event(0x80, TEST_NOTE + 20, 0);      // 6
  queue_midi_event(0x80, TEST_NOTE + 24, 0);      // 7
  queue_midi_event(0x80, TEST_NOTE + 29, 0);      // 8
  queue_midi_event(0x80, TEST_NOTE + 32, 0);      // 9
  queue_midi_event(0x80, TEST_NOTE + 37, 0);      // 10
  queue_midi_event(0x80, TEST_NOTE + 40, 0);      // 11
  queue_midi_event(0x80, TEST_NOTE + 44, 0);      // 12
  queue_midi_event(0x80, TEST_NOTE + 49, 0);      // 13
  queue_midi_event(0x80, TEST_NOTE + 52, 0);      // 14
  queue_midi_event(0x80, TEST_NOTE + 57, 0);      // 15
  queue_midi_event(0x80, TEST_NOTE + 60, 0);      // 16

  //bool success=true;
  //bool success = load_sysex("ROM1A.SYX", (++_voice_counter)-1);
  //bool success=load_sysex("ROM1B.SYX", (++_voice_counter)-1);
  //bool success=load_sysex("RITCH01-32.SYX", (++_voice_counter)-1);
  //bool success=load_sysex("RITCH33-64.SYX", (++_voice_counter)-1);
  bool success = load_sysex("RITCH0~1.SYX", (++_voice_counter) - 1);
  if (success == false)
    Serial.println(F("E: Cannot load SYSEX data"));
  else
    show_patch();
}
#endif

bool queue_midi_event(uint8_t type, uint8_t data1, uint8_t data2)
{
  if (master_key_enabled == true)
  {
    if (data1 >= 24 && data1 <= 56)
    {
      if (!load_sysex("RITCH0~1.SYX", data1 - 24))
      {
        Serial.print("E: cannot load voice number ");
        Serial.println(data1 - 24, DEC);
      }
    }
    master_key_enabled = false;
    Serial.println("Master key disabled");
  }
  else
  {
    if (type == 0x80 && data1 == MASTER_KEY_MIDI) // ignore Master key up
      return (false);
    else if (type == 0x90 && data1 == MASTER_KEY_MIDI) // Master key pressed
    {
      master_key_enabled = true;
      Serial.println("Master key enabled");
    }
    else
      return (dexed->processMidiMessage(type, data1, data2));
  }
  return (false);
}

void handle_sysex_parameter(const uint8_t* sysex, uint8_t len)
{
  // parse parameter change
  if (len == 7)
  {
    if (sysex[1] != 0x43) // check for Yamaha sysex
    {
      Serial.println("E: SysEx vendor not Yamaha.");
      return;
    }
    if ((sysex[3] & 0x7c) != 0)
    {
      Serial.println("E: Not a parameter change.");
      return;
    }
    if (sysex[6] != 0xf7)
    {
      Serial.println("E: SysEx end status byte not detected.");
      return;
    }
    dexed->data[sysex[4]] = sysex[5]; // set parameter
    Serial.print("SysEx parameter ");
    Serial.print(sysex[4], DEC);
    Serial.print("=");
    Serial.println(sysex[5], DEC);
  }
  else
    Serial.println("E: SysEx parameter length wrong.");
}

void cleanup(void)
{
  if (master_key_enabled == true)
  {
    master_key_enabled = false;
    Serial.println("Auto disabling master key");
  }

#ifdef SHOW_CPU_LOAD_MSEC
  cpu_and_mem_usage();
#endif
}

#ifdef SHOW_CPU_LOAD_MSEC
void cpu_and_mem_usage(void)
{
  Serial.print(F("CPU:"));
  Serial.print(AudioProcessorUsage(), DEC);
  Serial.print(F("   CPU MAX:"));
  Serial.print(AudioProcessorUsageMax(), DEC);
  Serial.print(F("  MEM:"));
  Serial.print(AudioMemoryUsage(), DEC);
  Serial.print(F("   MEM MAX:"));
  Serial.print(AudioMemoryUsageMax(), DEC);
  Serial.println();
  AudioProcessorUsageMaxReset();
  AudioMemoryUsageMaxReset();
}
#endif

#ifdef DEBUG
void show_patch(void)
{
  uint8_t i;
  char voicename[11];

  memset(voicename, 0, sizeof(voicename));
  for (i = 0; i < 6; i++)
  {
    Serial.print(F("OP"));
    Serial.print(6 - i, DEC);
    Serial.println(F(":"));
    Serial.println(F("R1|R2|R3|R4|L1|L2|L3|L4 LEV_SCL_BRK_PT|SCL_LEFT_DEPTH|SCL_RGHT_DEPTH"));
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
    Serial.println(F("SCL_L_CURVE|SCL_R_CURVE|RT_SCALE| AMS | KVS |OUT_LEV|OP_MOD|FRQ_C|FRQ_F|DETUNE"));
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
  Serial.println(F("PR1|PR2|PR3|PR4|PL1|PL2|PL3|PL4"));
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
