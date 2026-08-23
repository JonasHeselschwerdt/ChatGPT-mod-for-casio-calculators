/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

maindisplay.h: Settings for main display (text LCD)

*/

#ifndef MAINDIS_H
#define MAINDIS_H

// Includes


#include <stdint.h>


// I2C-Address

#define DOGM204_I2C_ADDR 0x3C





// Display restrictions

#define MAIN_DISPLAY_MAX_FRAMERATE 20       // in Hz
#define MAIN_DISPLAY_MIN_CYCLETIME_MS  ((1/MAIN_DISPLAY_MAX_FRAMERATE)*1000)





// Display dimension defines

#define MAIN_DISPLAY_ROWS 4             // do not set to a value lower than 2!
#define MAIN_DISPLAY_COLUMNS 20
#define MAIN_DISPLAY_CHARACTERS (MAIN_DISPLAY_COLUMNS*MAIN_DISPLAY_ROWS)

#define MAIN_DISPLAY_CHARACTERSIZE_Y 8
#define MAIN_DISPLAY_CHARACTERSIZE_X 5




// Dogm204 Command set and bitmasks, make sure IS and RE are set correctly through FUNCTION_SET before performing these

#define DOGM204_FUNCTION_SET 0x38
#define DOGM204_IS_BIT 0x01
#define DOGM204_RE_BIT 0x02

#define DOGM204_EXT_FUNCTION_SET 0x09
#define DOGM204_FANCY_CURSOR_BIT 0x02       // enables black/white inverting

#define DOGM204_ENTRY_MODE_SET 0x04
#define DOGM204_TOP_VIEW_BIT 0x01
#define DOGM204_BOTTOM_VIEW_BIT 0x02

#define DOGM204_BIAS_SET 0x1E               // Bias-values recommended by LCD manufacturer, do not change

#define DOGM204_INTERNAL_OSC 0x1B           // Internal Osc Frequency and Bias-value recommended by LCD manufacturer, do not change

#define DOGM204_FOLLOWER_CONTROL 0x6E       // Internal voltage regulator settings recommended by LCD manufacturer, do not change

// Both commands only used for contrast changes
#define DOGM204_POWER_CTRL 0x54             
#define DOGM204_CONTRAST_SET 0x70

#define DOGM204_CLEAR_SCREEN 0x01

// Dogm204 display settings 
#define DOGM204_DISPLAY_ONOFF_CTRL 0x0C
#define DOGM204_CURSOR_BLINK_BIT 0x01
#define DOGM204_CURSOR_NO_BLINK_BIT 0x00
#define DOGM204_CURSOR_ON_BIT 0x02
#define DOGM204_CURSOR_OFF_BIT 0x00
#define DOGM204_DISPLAY_ON_BIT 0x04
#define DOGM204_DISPLAY_OFF_BIT 0x00

#define DOGM204_RETURN_HOME 0x02

#define DOGM204_ROM_SELECT 0x72
#define DOGM204_ROM_SETTINGS 0x00
#define DOGM204_ROM_A_BIT 0x00
#define DOGM204_ROM_B_BIT 0x04
#define DOGM204_ROM_C_BIT 0x08

#define DOGM204_SET_DDRAM_ADDR 0x80

#define DOGM204_SET_CGRAM_ADDR 0x40







// Dogm204 cursor shift direction settings
// Cursor shift performed after write do Display RAM
#define DOGM204_CURSOR_RIGHT_SHIFT 0x02
#define DOGM204_CURSOR_LEFT_SHIFT 0x00






// Error codes for error printing on main display

#define ERROR_SOURCE_MAIN_DISPLAY   "MAIN DISPLAY ERROR  "
#define WARNING_SOURCE_MAIN_DISPLAY "MAIN DISPLAY WARNING"
#define ERROR_SOURCE_KEYPAD         "KEYPAD ERROR        "
#define ERROR_SOURCE_BMS            "BMS ERROR           "






// Fancy signs for the main display
// This is the index in MainDisplayLUT, NOT the DOGM204 character code, do not mix up!

#define DOGM204_FULLBLOCK 0xFF
#define DOGM204_HOLLOWBLOCK 0xFE        // custom
#define DOGM204_HALFHOLLOWBLOCK 0xFD    // custom
#define DOGM204_DEGREE_SIGN 0xFC        
#define DOGM204_BATTERY_SIGN 0xFB       // custom
#define DOGM204_DELTA_SIGN 0xFA





// Loading screen related

#define LOADING_ACTIVE_BIT BIT1






// Useful strings 

#define EMPTY_LINE "                    "       // same length as MAIN_DISPLAY_COLUMNS







// Exported functions

// Initialization
void dogm204_init(void);
// Settings
void dogm204_set_contrast(uint8_t contrast);
void dogm204_display_control(uint8_t display_settings);
void dogm204_set_cursor_shift_dir(uint8_t dir);
void dogm204_set_cursor_position(uint8_t x, uint8_t y);
// Print-type functions
void dogm204_clear_screen(void);
void dogm204_print_error_screen(char* error_message_string, char* error_source_string);
void dogm204_print_screen(char** screen_text);
void dogm204_print_screen_fancy(char** screen_text, uint16_t animation_time);
void dogm204_print_message(char** screen_text, uint16_t display_time);
void dogm204_start_loading_screen(char* loading_screen_string, uint16_t advance_interval);
void dogm204_end_loading_screen(void);


#endif