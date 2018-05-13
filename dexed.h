/**

   Copyright (c) 2013-2015 Pascal Gauthier.

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

#ifndef DEXED_H_INCLUDED
#define DEXED_H_INCLUDED

#include <Arduino.h>
#include "controllers.h"
#include "dx7note.h"
#include "lfo.h"
#include "synth.h"
#include "fm_core.h"
//#include "PluginFx.h"
#include "EngineMkI.h"
#include "EngineOpl.h"

#define PARAM_CHANGE_LEVEL 10 // when a sound change is recognized

struct ProcessorVoice {
  uint8_t midi_note;
  uint8_t velocity;
  bool keydown;
  bool sustained;
  bool live;
  Dx7Note *dx7_note;
};

enum DexedEngineResolution {
  DEXED_ENGINE_MODERN,	// 0
  DEXED_ENGINE_MARKI,		// 1
  DEXED_ENGINE_OPL		// 2
};

// GLOBALS

//==============================================================================

class Dexed
{
  public:
    Dexed(uint8_t rate);
    ~Dexed();
    void activate(void);
    void deactivate(void);
    uint8_t getEngineType();
    void setEngineType(uint8_t tp);
    bool isMonoMode(void);
    void setMonoMode(bool mode);
    void set_params(void);
    void GetSamples(uint16_t n_samples, int16_t* buffer);
    bool ProcessMidiMessage(uint8_t cmd, uint8_t data1, uint8_t data2);

    Controllers controllers;
    VoiceStatus voiceStatus;

  protected:
    //void onParam(uint8_t param_num,float param_val);
    void keyup(uint8_t pitch);
    void keydown(uint8_t pitch, uint8_t velo);
    void panic(void);
    void notes_off(void);

    static const uint8_t MAX_ACTIVE_NOTES = 32;
    uint8_t max_notes = MAX_ACTIVE_NOTES;
    ProcessorVoice voices[MAX_ACTIVE_NOTES];
    uint8_t currentNote;
    bool sustain;
    bool monoMode;
    bool refreshVoice;
    uint8_t engineType;
    //PluginFx fx;
    Lfo lfo;
    FmCore* engineMsfa;
    EngineMkI* engineMkI;
    EngineOpl* engineOpl;
    float* outbuf_;
    uint32_t bufsize_;
    float extra_buf_[_N_];
    uint32_t extra_buf_size_;

  private:
    uint16_t _rate;
    uint8_t _k_rate_counter;
    uint8_t _param_change_counter;

    uint8_t data[173] = {
      95, 29, 20, 50, 99, 95, 00, 00, 41, 00, 19, 00, 00, 03, 00, 06, 79, 00, 01, 00, 14, // OP
      95, 20, 20, 50, 99, 95, 00, 00, 00, 00, 00, 00, 00, 03, 00, 00, 99, 00, 01, 00, 00, // OP
      95, 29, 20, 50, 99, 95, 00, 00, 00, 00, 00, 00, 00, 03, 00, 06, 89, 00, 01, 00, 07, // OP
      95, 20, 20, 50, 99, 95, 00, 00, 00, 00, 00, 00, 00, 03, 00, 02, 99, 00, 01, 00, 07, // OP
      95, 50, 35, 78, 99, 75, 00, 00, 00, 00, 00, 00, 00, 03, 00, 07, 58, 00, 14, 00, 07, // OP
      96, 25, 25, 67, 99, 75, 00, 00, 00, 00, 00, 00, 00, 03, 00, 02, 99, 00, 01, 00, 10, // OP
      94, 67, 95, 60, 50, 50, 50, 50,
      04, 06, 00,
      34, 33, 00, 00, 00, 04,
      03, 24,
      00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
      01, 00, 99, 00, 99, 00, 99, 00, 99, 00,
      00,
      01, 01, 01, 01, 01, 01,
      16
    }; // INIT
    
};

#endif  // PLUGINPROCESSOR_H_INCLUDED
