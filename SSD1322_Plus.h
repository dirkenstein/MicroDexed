/*
   MicroDexed

   MicroDexed is a port of the Dexed sound engine
   (https://github.com/asb2m10/dexed) for the Teensy-3.5/3.6 with audio shield.
   Dexed ist heavily based on https://github.com/google/music-synthesizer-for-android

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

#include <U8g2lib.h>


#ifndef SSD1322_PLUS_H_INCLUDED
#define SSD1322_PLUS_H_INCLUDED

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif


#define STRING_BUF_SIZE 21

class SSD1322_Plus : public U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI
{
  public:

    using U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI::U8G2_SSD1322_NHD_256X64_F_4W_HW_SPI;

    void show(uint8_t y, uint8_t x, uint8_t fs, char *str)
    {
      _show(y, x, fs, str, false, false);
    }

    void show(uint8_t y, uint8_t x, uint8_t fs, long num)
    {
      char _buf10[STRING_BUF_SIZE];

      _show(y, x, fs, itoa(num, _buf10, 10), true, true);
    }

  private:
    void _show(uint8_t pos_y, uint8_t pos_x, uint8_t field_size, char *str, bool justify_right, bool fill_zero)
    {
      {
        char tmp[STRING_BUF_SIZE];
        char *s = tmp;
        uint8_t l = strlen(str);

        memset(tmp, 0, sizeof(tmp));
        if (fill_zero == true)
          memset(tmp, '0', field_size);
        else
          memset(tmp, 0x20, field_size - 1); // blank

        if (l > field_size)
          l = field_size;

        if (justify_right == true)
          s += field_size - l;

        strncpy(s, str, l);

        setCursor(pos_x * getMaxCharWidth(), pos_y * getMaxCharHeight());
        print(tmp);

#ifdef DEBUG
        Serial.print(pos_y, DEC);
        Serial.print(F("/"));
        Serial.print(pos_x, DEC);
        Serial.print(F(": ["));
        Serial.print(tmp);
        Serial.println(F("]"));
#endif
      }
    }
};

#endif
