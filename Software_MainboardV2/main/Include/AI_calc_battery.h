/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

battery.h: Battery management settings

*/

#ifndef BMS_H
#define BMS_H

/*

Important: BMS not fully tested yet.
Device can be powered by only USB / only battery / battery and USB (loadsharing)
Hotplugging the USB (during runtime) is possible, hotplugging the battery should be avoided (undefined behaviour)
After connecting the battery, SoC (battery %) can be inaccurate for a while
Do not try to detect overload conditions with Discharge rate of battery
Accuracy of BMS is limited due to standard fuel gauge model of MAX17048 used

*/


// Data Types

typedef struct{
    uint16_t cell_millivolts;           // mV
    int32_t cell_millidegrees;          // 'm°' Celsius
    uint8_t battery_percentage;         // %
    int32_t battery_crate;              // 'm%'/hr
    uint8_t charger_state;              // Bitmapped, see BMS defines below
}bms_typeDef;






// Global variables

extern char* bms_error_strings[5];




// Defines

#define MAX17048_I2C_ADDR 0x36




// Temperature related

#define BMS_ADC_FILTER_CYCLES 8         // Amount of ADC measurements per Temp reading (temp is average of all cycles)
#define BMS_NTC_BETA_VALUE 3435         // Most common beta value of temp monitor ntc thermistors
#define BMS_NTC_R_ZERO 10000            // in ohms (at room temperature)
#define BMS_NTC_T_ZERO 293.15           // aprox. room temperature in kelvin

#define BMS_VOLTAGEDIV_R 100000         // Ohms




// BMS State Bitmasks for bms_charger_state

#define BMS_CHARGING 0x01
#define BMS_CHARGER_PRESENT 0x02
#define BMS_BAT_PRESENT 0x08
#define BMS_UNKNOWN 0x80





// MAX17048 Settings

#define MAX17048_HBRT_THR 1     // Bat Discharge rate < MAX17048_HBRT_THR [%/hr] for > 6min -> enter low power hibernate mode
#define MAX17048_ACT_THR 50     // Change of Bat V > MAX17048_ACT_THR [mV] between MAX17048 ADC measurements -> exit hibernate mode
#define MAX17048_VRESET 2500    // Bat Voltage in mV at which the MAX17048 resets





// Battery Operation Limits

#define BMS_HIGH_TEMP_SHUTDOWN 50000        // in m°C, above this cell temp the device shut down
#define BMS_LOW_BAT_SOC_SHUTDOWN 5          // in %, below this SoC the device shuts down





// MAX17048 Registers

#define MAX_VCELL_REG 0x02
#define MAX_SOC_REG 0x04
#define MAX_MODE_REG 0x06
#define MAX_HIBRT_REG 0x0A
#define MAX_CONFIG_REG 0x0C
#define MAX_VALRT_REG 0x14
#define MAX_CRATE_REG 0x16
#define MAX_VRESETID_REG 0x18
#define MAX_STATUS_REG 0x1A






// bms_init return values

#define BMS_OK 0
#define BMS_TEMP_ERROR 1
#define BMS_SOC_ERROR 2
#define BMS_CV_ERROR 3
#define BMS_INVALID_STATE_ERROR 4






// For BMS task

#define BMS_ACTIVE_BIT BIT1
#define BMS_CHECK_INTERVAL 5000     // in ms






// Exported functions

uint8_t bms_init(void);
void create_bms_info_screen(char** info_screen);
void get_bms_state(bms_typeDef* bms);

#endif