/*
  modes_esp.h - WS2812FX header file for ESP8266 and ESP32 microprocessors

  LICENSE

  The MIT License (MIT)

  Copyright (c) 2016  Harm Aldick

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.


  CHANGELOG

  2022-03-23   Separated from the original WS2812FX.h file
*/
#ifndef mode_esp_h
#define mode_esp_h

#define MODE_COUNT (sizeof(_modes)/sizeof(_modes[0]))
#define MODE_PTR(x) this->*_modes[x].mode_ptr
#define MODE_NAME(x) _modes[x].name

#define FX_MODE_STATIC                   0
#define FX_MODE_BLINK                    1
#define FX_MODE_BREATH                   2
#define FX_MODE_COLOR_WIPE               3
#define FX_MODE_COLOR_WIPE_INV           4 
#define FX_MODE_COLOR_WIPE_REV           5
#define FX_MODE_COLOR_WIPE_REV_INV       6
#define FX_MODE_COLOR_WIPE_RANDOM        7
#define FX_MODE_RANDOM_COLOR             8
#define FX_MODE_SINGLE_DYNAMIC           9
#define FX_MODE_MULTI_DYNAMIC           10
#define FX_MODE_RAINBOW                 11
#define FX_MODE_RAINBOW_CYCLE           12
#define FX_MODE_SCAN                    13
#define FX_MODE_DUAL_SCAN               14
#define FX_MODE_FADE                    15
#define FX_MODE_THEATER_CHASE           16
#define FX_MODE_THEATER_CHASE_RAINBOW   17
#define FX_MODE_RUNNING_LIGHTS          18
#define FX_MODE_TWINKLE                 19
#define FX_MODE_TWINKLE_RANDOM          20
#define FX_MODE_TWINKLE_FADE            21
#define FX_MODE_TWINKLE_FADE_RANDOM     22
#define FX_MODE_SPARKLE                 23
#define FX_MODE_FLASH_SPARKLE           24
#define FX_MODE_HYPER_SPARKLE           25
#define FX_MODE_STROBE                  26
#define FX_MODE_STROBE_RAINBOW          27
#define FX_MODE_MULTI_STROBE            28
#define FX_MODE_BLINK_RAINBOW           29
#define FX_MODE_CHASE_WHITE             30
#define FX_MODE_CHASE_COLOR             31
#define FX_MODE_CHASE_RANDOM            32
#define FX_MODE_CHASE_RAINBOW           33
#define FX_MODE_CHASE_FLASH             34
#define FX_MODE_CHASE_FLASH_RANDOM      35
#define FX_MODE_CHASE_RAINBOW_WHITE     36
#define FX_MODE_CHASE_BLACKOUT          37
#define FX_MODE_CHASE_BLACKOUT_RAINBOW  38
#define FX_MODE_COLOR_SWEEP_RANDOM      39
#define FX_MODE_RUNNING_COLOR           40
#define FX_MODE_RUNNING_RED_BLUE        41
#define FX_MODE_RUNNING_RANDOM          42
#define FX_MODE_LARSON_SCANNER          43
#define FX_MODE_COMET                   44
#define FX_MODE_FIREWORKS               45
#define FX_MODE_FIREWORKS_RANDOM        46
#define FX_MODE_MERRY_CHRISTMAS         47
#define FX_MODE_FIRE_FLICKER            48
#define FX_MODE_FIRE_FLICKER_SOFT       49
#define FX_MODE_FIRE_FLICKER_INTENSE    50
#define FX_MODE_CIRCUS_COMBUSTUS        51
#define FX_MODE_HALLOWEEN               52
#define FX_MODE_BICOLOR_CHASE           53
#define FX_MODE_TRICOLOR_CHASE          54
#define FX_MODE_TWINKLEFOX              55
#define FX_MODE_RAIN                    56
#define FX_MODE_BLOCK_DISSOLVE          57
#define FX_MODE_ICU                     58
#define FX_MODE_DUAL_LARSON             59
#define FX_MODE_RUNNING_RANDOM2         60
#define FX_MODE_FILLER_UP               61
#define FX_MODE_RAINBOW_LARSON          62
#define FX_MODE_RAINBOW_FIREWORKS       63
#define FX_MODE_TRIFADE                 64
#define FX_MODE_VU_METER                65
#define FX_MODE_HEARTBEAT               66
#define FX_MODE_BITS                    67
#define FX_MODE_MULTI_COMET             68
#define FX_MODE_FLIPBOOK                69
#define FX_MODE_POPCORN                 70
#define FX_MODE_OSCILLATOR              71
#define FX_MODE_CUSTOM                  72  // keep this for backward compatiblity
#define FX_MODE_CUSTOM_0                72  // custom modes need to go at the end
#define FX_MODE_CUSTOM_1                73
#define FX_MODE_CUSTOM_2                74
#define FX_MODE_CUSTOM_3                75
#define FX_MODE_CUSTOM_4                76
#define FX_MODE_CUSTOM_5                77
#define FX_MODE_CUSTOM_6                78
#define FX_MODE_CUSTOM_7                79

typedef struct Mode {
  const __FlashStringHelper* name;
  const __FlashStringHelper* category;
  uint16_t (WS2812FX::*mode_ptr)(void);
} mode;

// mode categories
const char cat_simple[]  = "Simple";
const char cat_wipe[]    = "Wipe";
const char cat_sweep[]   = "Sweep";
const char cat_special[] = "Special";
const char cat_custom[]  = "Custom";

// create GLOBAL names to allow WS2812FX to compile with sketches and other libs
// that store strings in (get rid of the "section type conflict with __c"
// errors once and for all. Amen.)
const char name_0[] = "Static";
const char name_1[] = "Blink";
const char name_2[] = "Breath";
const char name_3[] = "Color Wipe";
const char name_4[] = "Color Wipe Inverse";
const char name_5[] = "Color Wipe Reverse";
const char name_6[] = "Color Wipe Reverse Inverse";
const char name_7[] = "Color Wipe Random";
const char name_8[] = "Random Color";
const char name_9[] = "Single Dynamic";
const char name_10[] = "Multi Dynamic";
const char name_11[] = "Rainbow";
const char name_12[] = "Rainbow Cycle";
const char name_13[] = "Scan";
const char name_14[] = "Dual Scan";
const char name_15[] = "Fade";
const char name_16[] = "Theater Chase";
const char name_17[] = "Theater Chase Rainbow";
const char name_18[] = "Running Lights";
const char name_19[] = "Twinkle";
const char name_20[] = "Twinkle Random";
const char name_21[] = "Twinkle Fade";
const char name_22[] = "Twinkle Fade Random";
const char name_23[] = "Sparkle";
const char name_24[] = "Flash Sparkle";
const char name_25[] = "Hyper Sparkle";
const char name_26[] = "Strobe";
const char name_27[] = "Strobe Rainbow";
const char name_28[] = "Multi Strobe";
const char name_29[] = "Blink Rainbow";
const char name_30[] = "Chase White";
const char name_31[] = "Chase Color";
const char name_32[] = "Chase Random";
const char name_33[] = "Chase Rainbow";
const char name_34[] = "Chase Flash";
const char name_35[] = "Chase Flash Random";
const char name_36[] = "Chase Rainbow White";
const char name_37[] = "Chase Blackout";
const char name_38[] = "Chase Blackout Rainbow";
const char name_39[] = "Color Sweep Random";
const char name_40[] = "Running Color";
const char name_41[] = "Running Red Blue";
const char name_42[] = "Running Random";
const char name_43[] = "Larson Scanner";
const char name_44[] = "Comet";
const char name_45[] = "Fireworks";
const char name_46[] = "Fireworks Random";
const char name_47[] = "Merry Christmas";
const char name_48[] = "Fire Flicker";
const char name_49[] = "Fire Flicker (soft)";
const char name_50[] = "Fire Flicker (intense)";
const char name_51[] = "Circus Combustus";
const char name_52[] = "Halloween";
const char name_53[] = "Bicolor Chase";
const char name_54[] = "Tricolor Chase";
const char name_55[] = "TwinkleFOX";
const char name_56[] = "Rain";
const char name_57[] = "Block Dissolve";
const char name_58[] = "ICU";
const char name_59[] = "Dual Larson";
const char name_60[] = "Running Random2";
const char name_61[] = "Filler Up";
const char name_62[] = "Rainbow Larson";
const char name_63[] = "Rainbow Fireworks";
const char name_64[] = "Trifade";
const char name_65[] = "VU Meter";
const char name_66[] = "Heartbeat";
const char name_67[] = "Bits";
const char name_68[] = "Multi Comet";
const char name_69[] = "Flipbook";
const char name_70[] = "Popcorn";
const char name_71[] = "Oscillator";
const char name_72[] = "Custom 0"; // custom modes need to go at the end
const char name_73[] = "Custom 1";
const char name_74[] = "Custom 2";
const char name_75[] = "Custom 3";
const char name_76[] = "Custom 4";
const char name_77[] = "Custom 5";
const char name_78[] = "Custom 6";
const char name_79[] = "Custom 7";

// define static array of member function pointers.
// make sure the order of the _modes array elements matches the FX_MODE_* values
__attribute__ ((unused)) static mode _modes[] = {
  { FSH(name_0),  FSH(cat_simple),  &WS2812FX::mode_static },
  { FSH(name_1),  FSH(cat_simple),  &WS2812FX::mode_blink },
  { FSH(name_2),  FSH(cat_special), &WS2812FX::mode_breath },
  { FSH(name_3),  FSH(cat_wipe),    &WS2812FX::mode_color_wipe },
  { FSH(name_4),  FSH(cat_wipe),    &WS2812FX::mode_color_wipe_inv },
  { FSH(name_5),  FSH(cat_sweep),   &WS2812FX::mode_color_wipe_rev },
  { FSH(name_6),  FSH(cat_sweep),   &WS2812FX::mode_color_wipe_rev_inv },
  { FSH(name_7),  FSH(cat_wipe),    &WS2812FX::mode_color_wipe_random },
  { FSH(name_8),  FSH(cat_simple),  &WS2812FX::mode_random_color },
  { FSH(name_9),  FSH(cat_simple),  &WS2812FX::mode_single_dynamic },
  { FSH(name_10), FSH(cat_simple),  &WS2812FX::mode_multi_dynamic },
  { FSH(name_11), FSH(cat_simple),  &WS2812FX::mode_rainbow },
  { FSH(name_12), FSH(cat_wipe),    &WS2812FX::mode_rainbow_cycle },
  { FSH(name_13), FSH(cat_sweep),   &WS2812FX::mode_scan },
  { FSH(name_14), FSH(cat_sweep),   &WS2812FX::mode_dual_scan },
  { FSH(name_15), FSH(cat_sweep),   &WS2812FX::mode_fade },
  { FSH(name_16), FSH(cat_wipe),    &WS2812FX::mode_theater_chase },
  { FSH(name_17), FSH(cat_wipe),    &WS2812FX::mode_theater_chase_rainbow },
  { FSH(name_18), FSH(cat_wipe),    &WS2812FX::mode_running_lights },
  { FSH(name_19), FSH(cat_simple),  &WS2812FX::mode_twinkle },
  { FSH(name_20), FSH(cat_simple),  &WS2812FX::mode_twinkle_random },
  { FSH(name_21), FSH(cat_simple),  &WS2812FX::mode_twinkle_fade },
  { FSH(name_22), FSH(cat_simple),  &WS2812FX::mode_twinkle_fade_random },
  { FSH(name_23), FSH(cat_simple),  &WS2812FX::mode_sparkle },
  { FSH(name_24), FSH(cat_simple),  &WS2812FX::mode_flash_sparkle },
  { FSH(name_25), FSH(cat_simple),  &WS2812FX::mode_hyper_sparkle },
  { FSH(name_26), FSH(cat_simple),  &WS2812FX::mode_strobe },
  { FSH(name_27), FSH(cat_simple),  &WS2812FX::mode_strobe_rainbow },
  { FSH(name_28), FSH(cat_simple),  &WS2812FX::mode_multi_strobe },
  { FSH(name_29), FSH(cat_simple),  &WS2812FX::mode_blink_rainbow },
  { FSH(name_30), FSH(cat_wipe),    &WS2812FX::mode_chase_white },
  { FSH(name_31), FSH(cat_wipe),    &WS2812FX::mode_chase_color },
  { FSH(name_32), FSH(cat_wipe),    &WS2812FX::mode_chase_random },
  { FSH(name_33), FSH(cat_wipe),    &WS2812FX::mode_chase_rainbow },
  { FSH(name_34), FSH(cat_wipe),    &WS2812FX::mode_chase_flash },
  { FSH(name_35), FSH(cat_wipe),    &WS2812FX::mode_chase_flash_random },
  { FSH(name_36), FSH(cat_wipe),    &WS2812FX::mode_chase_rainbow_white },
  { FSH(name_37), FSH(cat_wipe),    &WS2812FX::mode_chase_blackout },
  { FSH(name_38), FSH(cat_wipe),    &WS2812FX::mode_chase_blackout_rainbow },
  { FSH(name_39), FSH(cat_sweep),   &WS2812FX::mode_color_sweep_random },
  { FSH(name_40), FSH(cat_wipe),    &WS2812FX::mode_running_color },
  { FSH(name_41), FSH(cat_wipe),    &WS2812FX::mode_running_red_blue },
  { FSH(name_42), FSH(cat_wipe),    &WS2812FX::mode_running_random },
  { FSH(name_43), FSH(cat_sweep),   &WS2812FX::mode_larson_scanner },
  { FSH(name_44), FSH(cat_wipe),    &WS2812FX::mode_comet },
  { FSH(name_45), FSH(cat_simple),  &WS2812FX::mode_fireworks },
  { FSH(name_46), FSH(cat_simple),  &WS2812FX::mode_fireworks_random },
  { FSH(name_47), FSH(cat_wipe),    &WS2812FX::mode_merry_christmas },
  { FSH(name_48), FSH(cat_simple),  &WS2812FX::mode_fire_flicker },
  { FSH(name_49), FSH(cat_simple),  &WS2812FX::mode_fire_flicker_soft },
  { FSH(name_50), FSH(cat_simple),  &WS2812FX::mode_fire_flicker_intense },
  { FSH(name_51), FSH(cat_wipe),    &WS2812FX::mode_circus_combustus },
  { FSH(name_52), FSH(cat_wipe),    &WS2812FX::mode_halloween },
  { FSH(name_53), FSH(cat_wipe),    &WS2812FX::mode_bicolor_chase },
  { FSH(name_54), FSH(cat_wipe),    &WS2812FX::mode_tricolor_chase },
  { FSH(name_55), FSH(cat_special), &WS2812FX::mode_twinkleFOX },
  { FSH(name_56), FSH(cat_special), &WS2812FX::mode_rain },
  { FSH(name_57), FSH(cat_simple),  &WS2812FX::mode_block_dissolve },
  { FSH(name_58), FSH(cat_special), &WS2812FX::mode_icu },
  { FSH(name_59), FSH(cat_sweep),   &WS2812FX::mode_dual_larson },
  { FSH(name_60), FSH(cat_wipe),    &WS2812FX::mode_running_random2 },
  { FSH(name_61), FSH(cat_special), &WS2812FX::mode_filler_up },
  { FSH(name_62), FSH(cat_sweep),   &WS2812FX::mode_rainbow_larson },
  { FSH(name_63), FSH(cat_simple),  &WS2812FX::mode_rainbow_fireworks },
  { FSH(name_64), FSH(cat_simple),  &WS2812FX::mode_trifade },
  { FSH(name_65), FSH(cat_special), &WS2812FX::mode_vu_meter },
  { FSH(name_66), FSH(cat_special), &WS2812FX::mode_heartbeat},
  { FSH(name_67), FSH(cat_special), &WS2812FX::mode_bits},
  { FSH(name_68), FSH(cat_wipe),    &WS2812FX::mode_multi_comet},
  { FSH(name_69), FSH(cat_special), &WS2812FX::mode_flipbook},
  { FSH(name_70), FSH(cat_special), &WS2812FX::mode_popcorn},
  { FSH(name_71), FSH(cat_special), &WS2812FX::mode_oscillator},
  { FSH(name_72), FSH(cat_custom),  &WS2812FX::mode_custom_0 },
  { FSH(name_73), FSH(cat_custom),  &WS2812FX::mode_custom_1 },
  { FSH(name_74), FSH(cat_custom),  &WS2812FX::mode_custom_2 },
  { FSH(name_75), FSH(cat_custom),  &WS2812FX::mode_custom_3 },
  { FSH(name_76), FSH(cat_custom),  &WS2812FX::mode_custom_4 },
  { FSH(name_77), FSH(cat_custom),  &WS2812FX::mode_custom_5 },
  { FSH(name_78), FSH(cat_custom),  &WS2812FX::mode_custom_6 },
  { FSH(name_79), FSH(cat_custom),  &WS2812FX::mode_custom_7 }
};
#endif
