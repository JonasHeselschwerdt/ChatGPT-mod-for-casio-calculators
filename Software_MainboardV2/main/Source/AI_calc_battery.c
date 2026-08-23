/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

battery.c: Battery management functions and variables

*/

/*

Important: BMS not fully tested yet.
Device can be powered by only USB / only battery / battery and USB (loadsharing)
Hotplugging the USB (during runtime) is possible, hotplugging the battery should be avoided (undefined behaviour)
After connecting the battery, SoC (battery %) can be inaccurate for a while
Do not try to detect overload conditions with Discharge rate of battery
Accuracy of BMS is limited due to standard fuel gauge model of MAX17048 used

*/

// Includes

#include "math.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "AI_calc_battery.h"
#include "AI_calc_keypad.h"
#include "AI_calc_device.h"
#include "AI_calc_maindisplay.h"




// Static variables

static i2c_master_dev_handle_t max17048_dev = NULL;

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle;
static adc_unit_t adc_unit;
static adc_channel_t adc_channel;

static uint8_t bmsStateLUT[16];

static uint8_t max17048_init_flag = 0;  

static bms_typeDef bms = {
    .cell_millivolts = 0,
    .cell_millidegrees = 0,
    .battery_percentage = 0,
    .battery_crate = 0,
    .charger_state = 0
};

static EventGroupHandle_t battery_events;
static TaskHandle_t battery_task_handle;






// Global variables

char* bms_error_strings[5] = {
    "                    ",
    " TEMPERATURE ERROR  ",
    " BAT SOC ERROR      ",
    " BAT VOLTAGE ERROR  ",
    " BMS INVALID STATE  "
};



// Static function declarations

static void init_bms_state_LUT(void);
static void bms_temp_adc_init(void);

static int32_t bms_get_battery_temp(void);
static uint8_t bms_get_charger_state(void);
static int32_t bms_ntc_millivolts_to_millidegrees(int millivolts);

static void max17048_write(uint8_t reg, uint16_t data);
static void max17048_read(uint8_t reg, uint16_t* data);

static uint8_t check_battery_condition(void);






// Static functions BMS

static void init_bms_state_LUT(void){

    // Init BMS-State LUT (Bit 0: ALRT Bit 1: PG Bit 2: STAT2 Bit 3: STAT1)

    // The two states below only occur when no battery is connected or there is a temperature fault
    // Temperature monitored by ESP32 ADC, so a Temp fault condition of the MCP73871 gets ignored
    bmsStateLUT[0b0000] = BMS_CHARGER_PRESENT;
    bmsStateLUT[0b0001] = BMS_CHARGER_PRESENT;

    bmsStateLUT[0b0010] = BMS_UNKNOWN;  // invalid State
    bmsStateLUT[0b0011] = BMS_UNKNOWN;  // invalid State
    bmsStateLUT[0b0100] = BMS_BAT_PRESENT|BMS_CHARGER_PRESENT|BMS_CHARGING;
    bmsStateLUT[0b0101] = BMS_BAT_PRESENT|BMS_CHARGER_PRESENT|BMS_CHARGING;
    bmsStateLUT[0b0110] = BMS_BAT_PRESENT;
    bmsStateLUT[0b0111] = BMS_BAT_PRESENT;
    bmsStateLUT[0b1000] = BMS_BAT_PRESENT|BMS_CHARGER_PRESENT;
    bmsStateLUT[0b1001] = BMS_BAT_PRESENT|BMS_CHARGER_PRESENT;
    bmsStateLUT[0b1010] = BMS_UNKNOWN;  // invalid State
    bmsStateLUT[0b1011] = BMS_UNKNOWN;  // invalid State
    bmsStateLUT[0b1100] = BMS_CHARGER_PRESENT;
    bmsStateLUT[0b1101] = BMS_CHARGER_PRESENT;
    bmsStateLUT[0b1110] = BMS_BAT_PRESENT;
    bmsStateLUT[0b1111] = BMS_BAT_PRESENT;
}


static void max17048_write(uint8_t reg, uint16_t data){

    uint8_t buf[3] = {reg, (uint8_t)(data>>8),(uint8_t)(data&0x00FF)};
    ESP_ERROR_CHECK(i2c_master_transmit(max17048_dev,buf,sizeof(buf),-1));
}

static void max17048_read(uint8_t reg, uint16_t* data){

    uint8_t buf[2];
    ESP_ERROR_CHECK(i2c_master_transmit(max17048_dev,&reg,1,-1));
    ESP_ERROR_CHECK(i2c_master_receive(max17048_dev,buf,sizeof(buf),-1));
    *data = (uint16_t)(buf[0]<<8)|buf[1];
}

static int32_t bms_ntc_millivolts_to_millidegrees(int millivolts){

    uint16_t ntc_r;
    if (bms.charger_state & BMS_CHARGER_PRESENT){
        // Calculate R of NTC through Ohms law
        // MCP73871 passe 50 microamps through ntc
        ntc_r = (uint16_t)((uint32_t)millivolts * 1000 / 50);
    }
    else{
        // Calculate R of NTC
        // NTC part of a Voltage divider (see schematics)
        // Formula assumes GPIO HIGH Voltage = 3.3V
        ntc_r = (uint16_t)(((uint32_t)(millivolts*BMS_VOLTAGEDIV_R))/(GPIO_OUTPUT_HIGH_MV - millivolts));
    }
    // Calculate Temp out of R
    // using NTC Thermistor formula
    float degrees_float = (float)(BMS_NTC_BETA_VALUE / ((logf((float)ntc_r/BMS_NTC_R_ZERO))+(float)(BMS_NTC_BETA_VALUE/BMS_NTC_T_ZERO)));
    degrees_float = degrees_float - 273.15;
    return (int32_t)(degrees_float*1000);
}


static int32_t bms_get_battery_temp(void){

    int adc_raw_data_sum = 0;
    int adc_raw;
    int adc_avrg;
    int adc_millivolts;
    // Output in millidegrees
    if (bms.charger_state & BMS_CHARGER_PRESENT){
        // Constant current source of MCP73871 runs through NTC
        // Do multiple measurements to average out some noise
        for (uint8_t i=0; i<BMS_ADC_FILTER_CYCLES; i++){
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc_channel, &adc_raw));
            adc_raw_data_sum += adc_raw;
        }
        adc_avrg = adc_raw_data_sum / BMS_ADC_FILTER_CYCLES;
        // Turn average into millivolt value
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_avrg, &adc_millivolts));
        return (bms_ntc_millivolts_to_millidegrees(adc_millivolts));
    }
    else{
        // Activate the voltage divider first
        gpio_set_level(BMS_NTC_VOLTAGE_DIV_ACT,1);
        gpio_set_direction(BMS_NTC_VOLTAGE_DIV_ACT, GPIO_MODE_OUTPUT);
        vTaskDelay(pdMS_TO_TICKS(100));            // wait for C28 (see schematics) to charge up
        // Rest as above
        for (uint8_t i=0; i<BMS_ADC_FILTER_CYCLES; i++){
            ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc_channel, &adc_raw));
            adc_raw_data_sum += adc_raw;
        }
        adc_avrg = adc_raw_data_sum / BMS_ADC_FILTER_CYCLES;
        // Turn average into millivolt value
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_avrg, &adc_millivolts));
        // Disable the voltage divider before leaving function
        gpio_set_direction(BMS_NTC_VOLTAGE_DIV_ACT, GPIO_MODE_DISABLE);
        return (bms_ntc_millivolts_to_millidegrees(adc_millivolts));
    }
}

static uint8_t bms_get_charger_state(void){

    // Read Status Outputs of MAX17048 and MCP73871
    uint8_t bms_state = 0;
    bms_state |= tca8418_gpi_get_level(BMS_ALRT);   // not used, MAX17048 gets polled
    bms_state |= (tca8418_gpi_get_level(BMS_PG) << 1);
    bms_state |= (tca8418_gpi_get_level(BMS_STAT2) << 2);
    bms_state |= (tca8418_gpi_get_level(BMS_STAT1) << 3);
    return bmsStateLUT[bms_state];
}

static void bms_temp_adc_init(void){

    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(BMS_ADC_TEMP,&adc_unit,&adc_channel));

    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = adc_unit,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t adc_ch_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle,adc_channel,&adc_ch_config));

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = adc_unit,
        .chan = adc_channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle));
}

static void max17048init(void){

    init_bms_state_LUT();
    i2c_device_config_t max17048_config = {
        .device_address = MAX17048_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus,&max17048_config,&max17048_dev));
    // Check if there is a battery present
    uint8_t initial_bms_state = bms_get_charger_state();
    if (!(initial_bms_state&BMS_BAT_PRESENT)){
        max17048_init_flag = 0;
        return;
    }
    // Configure all register every time the device boots up
    /*
    Unconfigured/Default values/Not used:
    - MODE_REG
    - CONFIG_REG -> ALRTS ignored, MAX17048 used by polling
    - VALRT_REG -> ALRTS ignored
    - VERSION_REG
    - TABLE_REGs
    - CMD_REG
    */
    uint16_t hibrt_content = 0;
    hibrt_content |= ((uint16_t)((float)MAX17048_HBRT_THR/0.208)<<8);
    hibrt_content |= (uint16_t)((float)MAX17048_ACT_THR/1.25);
    max17048_write(MAX_HIBRT_REG,hibrt_content);
    max17048_write(MAX_VRESETID_REG,((MAX17048_VRESET/40)&0xFE00));
    // Set init flag
    max17048_init_flag = 1;
}

static uint8_t check_battery_condition(void){

    bms.charger_state = bms_get_charger_state();
    if (bms.charger_state & BMS_UNKNOWN){
        return BMS_INVALID_STATE_ERROR;
    }
    // Check if battery is even connected
    if (!(bms.charger_state & BMS_BAT_PRESENT)){
        // Device must be powered by USB
        // Reset MAX17048 init flag
        max17048_init_flag = 0;
        // Return default values
        bms.battery_percentage = 0;
        bms.cell_millidegrees = 0;
        bms.cell_millivolts = 0;
        bms.battery_crate = 0;
        return BMS_OK;
    }
    // Battery connected, check if Max17048 already initialized
    if (!max17048_init_flag){
        // Configure all registers
        /*
        Unconfigured/Default values/Not used:
        - MODE_REG
        - CONFIG_REG -> ALRTS ignored, MAX17048 used by polling
        - VALRT_REG -> ALRTS ignored
        - VERSION_REG
        - TABLE_REGs
        - CMD_REG
        */
        uint16_t hibrt_content = 0;
        hibrt_content |= ((uint16_t)((float)MAX17048_HBRT_THR/0.208)<<8);
        hibrt_content |= (uint16_t)((float)MAX17048_ACT_THR/1.25);
        max17048_write(MAX_HIBRT_REG,hibrt_content);
        max17048_write(MAX_VRESETID_REG,((MAX17048_VRESET/40)&0xFE00));
        // Set init flag
        max17048_init_flag = 1;
    }
    // Temperature
    bms.cell_millidegrees = bms_get_battery_temp();
    if (bms.cell_millidegrees > BMS_HIGH_TEMP_SHUTDOWN){
        return BMS_TEMP_ERROR;
    }
    // Cell Voltage
    uint16_t vcell_content;
    max17048_read(MAX_VCELL_REG,&vcell_content);
    uint32_t vcell_uV = (uint32_t)(vcell_content*625)/8;        // equals * 78.125 [µV]
    bms.cell_millivolts = (uint16_t)(vcell_uV/1000);
    // SoC (State of charge)
    uint16_t soc_content;
    max17048_read(MAX_SOC_REG,&soc_content);
    if ((soc_content >> 8) <= 100){
        // Valid SoC
        bms.battery_percentage = (soc_content >> 8);
    }
    else{
        // Invalid value
        bms.battery_percentage = 0;
        return BMS_INVALID_STATE_ERROR;
    }
    if (bms.battery_percentage < BMS_LOW_BAT_SOC_SHUTDOWN){
        return BMS_SOC_ERROR;
    }
    // Discharge/Charge rate
    int16_t crate_content;
    max17048_read(MAX_CRATE_REG,(uint16_t*)&crate_content);
    bms.battery_crate = crate_content * 208;           // 0.208% * LSB of  MAX_CRATE_REG = Crate
    return BMS_OK;
}

static void bms_monitoring_task(void *args){

    // Monitors battery information continouosly
    // Shuts down device if necessary
    while(1){
        xEventGroupWaitBits(battery_events,BMS_ACTIVE_BIT,pdFALSE,pdTRUE,portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(BMS_CHECK_INTERVAL));
        if (check_battery_condition() != ESP_OK){
            powerlatch_shutdown_immediately();
        }
        // ESP_LOGI("BMS-Task","CV[mV]:%d|SOC:%d|TEMP:%d",bms.cell_millivolts,bms.battery_percentage,bms.cell_millidegrees);
    }
}







// Exported functions

void create_bms_info_screen(char** info_screen){

    uint8_t bat = (bms.charger_state&BMS_BAT_PRESENT) != 0;
    uint8_t charger_pres = (bms.charger_state&BMS_CHARGER_PRESENT) != 0;
    uint8_t charging = (bms.charger_state&BMS_CHARGING) != 0;

    snprintf(info_screen[0],(MAIN_DISPLAY_COLUMNS+1),"Con.: Bat:%1u |VBus:%1u ",bat,charger_pres); 
    snprintf(info_screen[1],(MAIN_DISPLAY_COLUMNS+1),"Charging:%1u|         ",charging);
    snprintf(
        info_screen[2],
        (MAIN_DISPLAY_COLUMNS+1),
        "Bat: %1.3fV| %4d %cC",
        (((float)bms.cell_millivolts)/1000),
        (int8_t)(bms.cell_millidegrees/1000), 
        (char)DOGM204_DEGREE_SIGN
    );
    snprintf(
        info_screen[3],
        (MAIN_DISPLAY_COLUMNS+1),
        "%c:%3u%%|%c%c:%6.1f%%/hr",
        (char)DOGM204_BATTERY_SIGN,bms.battery_percentage,
        (char)DOGM204_DELTA_SIGN,
        (char)DOGM204_BATTERY_SIGN,
        ((float)bms.battery_crate/1000)
    );
}

void get_bms_state(bms_typeDef* info_bms){

    // Getter function for static bms variable
    info_bms->cell_millivolts = bms.cell_millivolts;
    info_bms->cell_millidegrees = bms.cell_millidegrees;         
    info_bms->battery_percentage = bms.battery_percentage;        
    info_bms->battery_crate = bms.battery_crate;             
    info_bms->charger_state = bms.charger_state;
}

uint8_t bms_init(void){

    bms_temp_adc_init();
    max17048init();
    // Init battery task
    battery_events = xEventGroupCreate();
    xTaskCreate(bms_monitoring_task,"BMS",2048,NULL,2,&battery_task_handle);
    xEventGroupSetBits(battery_events,BMS_ACTIVE_BIT);
    return (check_battery_condition());
}