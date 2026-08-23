/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

device.h: Hardware settings

*/

#ifndef DEVICE_H
#define DEVICE_H

// Includes

#include "driver/i2c_master.h"





// Device Typedef, used to store settings and information related to the hardware

typedef struct{
    uint8_t debug_mode;                 // boolean
    uint8_t main_display_contrast;      // 0...63
    uint8_t side_display_contrast;      // 0...255
    uint8_t side_display_on;            // boolean, if 0: all dep128064_print() commands are disabled
    uint8_t side_display_toggle_mode;   // boolean, if 1: dep128064_power_toggle() works
    char name[32];
}device_TypeDef;






// Extern device variables

extern i2c_master_bus_handle_t i2c_bus;

extern device_TypeDef device;



// GPIO-Defines and GPIO-state-defines

#define ESP_N_POWERLATCH 4

#define MAIN_DISPLAY_N_RESET 8  

#define TCA8418_N_INTERRUPT 16
#define TCA8418_N_RESET 17

#define SIDE_DISPLAY_N_RESET 18

#define CAMERA_POWER_ENABLE 38      // Rest of camera Pin defines in camera.h

#define BMS_ADC_TEMP 3
#define BMS_NTC_VOLTAGE_DIV_ACT 39

/*
To use GPIO 3 the Efuses need to be set correctly
EFUSE_STRAP_JTAG_SEL = EFUSE_DIS_USB_JTAG = EFUSE_DIS_PAD_JTAG = 0
This is the case by default, don't change these!
*/
 
#define FREEGPIO_8 40
#define FREEGPIO_7 41
#define FREEGPIO_6 42
#define FREEGPIO_5 43
#define FREEGPIO_4 44





// Needed for some ADC operations (in battery.c)

#define GPIO_OUTPUT_HIGH_MV 3000        // Output high voltage of ESP32 GPIO





// I2C-Defines

#define SDA 7
#define SCL 15
#define I2C_FREQ 400000             // in Hz, supported by all peripherals





// Exported functions

void powerlatch_shutdown(void);                 // can only be performed after all components have been initialized
void powerlatch_shutdown_immediately(void);     // can be performed always
void device_init(void);




#endif