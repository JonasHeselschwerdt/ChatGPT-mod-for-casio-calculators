/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

sidedisplay.h: Functions and variables to control the OLED screen (using U8G2 and Lopaka)

*/


/*

This Code uses the U8G2 library, to get it to compile paste the 'csrc' directory 
of https://github.com/olikraus/u8g2 into components/u8g2

*/

// Includes 

#include "u8g2.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "AI_calc_sidedisplay.h"
#include "AI_calc_device.h"
#include "AI_calc_battery.h"
#include "AI_Calc_network.h"





// Static function declarations

static uint8_t cb_dep128064_i2c_transmit(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
static uint8_t cb_dep128064_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

static void dep128064_write_cmd(uint8_t cmd);
static void dep128064_start_display(void);

static void dep128064_screensaver_task_init(void);
static void dep128064_screensaver_task(void* arg);
static void dep128064_print_screensaver(void);
static void advance_screensaver(void);

static void generate_status_screen_bms(status_screen_TypeDef* status_screen, bms_typeDef* bms);
static void dep128064_print_status_screen(status_screen_TypeDef* status_screen);





// Static variables

u8g2_t u8g2;

static i2c_master_dev_handle_t dep128064_dev = NULL;

// Needed for i2c callbacks
static uint8_t i2c_buffer[256];         
static size_t i2c_buffer_len = 0;

// Screensaver variables
static EventGroupHandle_t screensaver_events;
static TaskHandle_t screensaver_task_handle;

static uint16_t screensaver_advance_interval_ms;
static const uint8_t screensaver_logo[] = { 0xfe,0x3f,0x20,0xff,0x3f,0x30,0xff,0x1f,        // Lopaka generated data
                                            0x38,0x00,0x1e,0x3c,0x00,0x1e,0x1c,0x00,
                                            0x1e,0x1e,0x00,0x1e,0x1e,0x00,0x1e,0x0f,
                                            0x00,0x1e,0x0f,0x00,0xff,0x7f,0x80,0xff,
                                            0x7f,0x80,0xff,0x3f,0x00,0x1c,0x1c,0x03,
                                            0x1c,0x1c,0x07,0x1c,0x0e,0x0f,0x1e,0x0e,
                                            0x9f,0x1f,0x07,0xfe,0x07,0x07,0xfc,0x03,
                                            0x03,0xf0,0x00,0x01};
static screensaver_TypeDef screensaver = {
    .x = 0,
    .y = 0,
    .speed_x = 1,
    .speed_y = 1,
    .size_x = 23,
    .size_y = 20
};

// Info screen (device status)

static status_screen_TypeDef status_screen;

// Lopaka generated screen data:

// Battery icons (from low to full)
static const uint8_t image_no_bat[] = {0xff,0xff,0x03,0x01,0x00,0x02,0x81,0x04,0x06,0x01,0x00,0x06,0x81,0x07,0x06,0x41,0x08,0x06,0x01,0x00,0x02,0xff,0xff,0x03};
static const uint8_t image_bat0_5_bits[] = {0xff,0xff,0x03,0x01,0x00,0x02,0x01,0x01,0x06,0x01,0x01,0x06,0x01,0x00,0x06,0x01,0x01,0x06,0x01,0x00,0x02,0xff,0xff,0x03};
static const uint8_t image_bat1_5_bits[] = {0xff,0xff,0x03,0x01,0x00,0x02,0x0d,0x00,0x06,0x0d,0x00,0x06,0x0d,0x00,0x06,0x0d,0x00,0x06,0x01,0x00,0x02,0xff,0xff,0x03};
static const uint8_t image_bat2_5_bits[] = {0xff,0xff,0x03,0x01,0x00,0x02,0x6d,0x00,0x06,0x6d,0x00,0x06,0x6d,0x00,0x06,0x6d,0x00,0x06,0x01,0x00,0x02,0xff,0xff,0x03};
static const uint8_t image_bat3_5_bits[] = {0xff,0xff,0x03,0x01,0x00,0x02,0x6d,0x03,0x06,0x6d,0x03,0x06,0x6d,0x03,0x06,0x6d,0x03,0x06,0x01,0x00,0x02,0xff,0xff,0x03};
static const uint8_t image_bat4_5_bits[] = {0xff,0xff,0x03,0x01,0x00,0x02,0x6d,0x1b,0x06,0x6d,0x1b,0x06,0x6d,0x1b,0x06,0x6d,0x1b,0x06,0x01,0x00,0x02,0xff,0xff,0x03};
static const uint8_t image_bat5_5_bits[] = {0xff,0xff,0x03,0x01,0x00,0x02,0x6d,0xdb,0x06,0x6d,0xdb,0x06,0x6d,0xdb,0x06,0x6d,0xdb,0x06,0x01,0x00,0x02,0xff,0xff,0x03};
static const uint8_t* battery_icons[] = {image_bat0_5_bits,image_bat1_5_bits,image_bat2_5_bits,image_bat3_5_bits,image_bat4_5_bits,image_bat5_5_bits};

static const uint8_t image_charging_icon_bits[] = {0xff,0x01,0xcf,0x01,0x07,0x03,0xc1,0x03,0x07,0x03,0xcf,0x01,0xff,0x01};

static const uint8_t image_clock_bits[] = {0x3c,0x76,0xf7,0xf7,0xef,0xdf,0x7e,0x3c};

static const uint8_t image_speechbubble_bits[] = {0x7e,0xff,0xff,0xff,0xff,0x7e,0x30,0x18};

static const uint8_t image_folder_open_bits[] = {0x0f,0x00,0xf9,0x00,0x01,0x01,0xf9,0x03,0x05,0x04,0x03,0x02,0x01,0x03,0xff,0x01};

static const uint8_t image_camera_bits[] = {0xff,0x03,0x09,0x02,0xff,0x03,0xcd,0x03,0xb7,0x03,0xb7,0x03,0xcf,0x03,0xff,0x03};

static const uint8_t image_key_bits[] = {0x00,0x02,0x00,0x05,0xff,0x05,0x5a,0x05,0x00,0x05,0x00,0x02};

static const uint8_t image_check_bits[] = {0x00,0x03,0x80,0x01,0xc0,0x00,0x63,0x00,0x36,0x00,0x1c,0x00,0x08,0x00};
static const uint8_t image_cross_small_bits[] = {0x03,0x03,0xce,0x01,0x78,0x00,0x30,0x00,0x78,0x00,0xce,0x01,0x03,0x03};

static const uint8_t image_robot_bits[] = {0x7e,0xff,0x81,0xa5,0x81,0xbd,0x81,0x7e};

// Wifi signal strenght from high to low
static const uint8_t image_wifi3_3_bits[] = {0xfe,0x01,0x01,0x02,0xfc,0x00,0x02,0x01,0x78,0x00,0x84,0x00,0x30,0x00,0x30,0x00};
static const uint8_t image_wifi2_3_bits[] = {0x7e,0x81,0x3c,0x42,0x18,0x18};
static const uint8_t image_wifi1_3_bits[] = {0x1e,0x21,0x0c,0x0c};
static const uint8_t image_wifi0_3_bits[] = {0xfe,0x01,0x01,0x02,0xfc,0x00,0x02,0x01,0x78,0x00,0x80,0x0a,0x30,0x04,0x30,0x0a};





// Static functions

static uint8_t cb_dep128064_i2c_transmit(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr){

    // Callback function for U8G2 I2C transmits
    // Oriented around u8x8_byte_sw_i2c() in u8x8_byte.c
    uint8_t* data;
    switch (msg){
        case U8X8_MSG_BYTE_SEND:
            // Append data into buffer
            data = (uint8_t *)arg_ptr;
            memcpy(
                &i2c_buffer[i2c_buffer_len],
                data,
                arg_int
            );
            i2c_buffer_len += arg_int;
            break;
        case U8X8_MSG_BYTE_INIT:
            // already initialized I2C externally
            break;
        case U8X8_MSG_BYTE_SET_DC:
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            // Start an I2C transfer, don't send anything yet
            // prepare the buffer
            i2c_buffer_len = 0;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            /*  I2C transfer can be finished by sending all
                the data in the buffer
                at the beginning of this buffer there is always the control byte
                0x40: only Data follows
                0x00: only Command(s) follow(s)
            */
            ESP_ERROR_CHECK(
                i2c_master_transmit(
                    dep128064_dev,
                    i2c_buffer,
                    i2c_buffer_len,
                    -1
                )
            );
            break;
        default:
            return 0;
    }
    return 1;
}

static uint8_t cb_dep128064_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr){

    // Callback functions for Timing delays
    // Doesnt seem to be used with I2C trasnfers, leaving it in here just in case though
    switch(msg){
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;
        default:
            return 0;
    }
    return 1;
}

static void dep128064_write_cmd(uint8_t cmd){

    // Only used for external config of the display, U8G2 uses the driver/i2c_master.h directly
        uint8_t buf[2] = {
        0x00,     
        cmd
    };
    ESP_ERROR_CHECK(i2c_master_transmit(dep128064_dev, buf, sizeof(buf), -1));
    // Give a little bit of time for command processing
    vTaskDelay(pdMS_TO_TICKS(5));
}



static void dep128064_start_display(void){

    // Init I2C
    i2c_device_config_t dep128064_config = {
        .device_address = DEP128064_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus,&dep128064_config,&dep128064_dev));

    gpio_set_level(SIDE_DISPLAY_N_RESET,1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Init sequence (is mostly the same as in the u8x8_d_ssd1306_128x64_noname_init_seq that U8G2 uses for the SH1106)
    // some commands that are only for the SSD1306 have been removed
    dep128064_write_cmd(DEP128064_DISPLAYONOFF_CTRL|DEP128064_OFF);

    dep128064_write_cmd(0xD5);  // set OSC-Freq DCLK divide ratio = 1, +15% to f_osc
    dep128064_write_cmd(0x80);

    dep128064_write_cmd(0xA8);  // set multiplex ration
    dep128064_write_cmd(0x3F);

    dep128064_write_cmd(0xD3);  // needs to be adjusted maybe
    dep128064_write_cmd(0x00);

    dep128064_write_cmd(0x40);  // Set display start line

    dep128064_write_cmd(0xA1);  // Segment remap

    dep128064_write_cmd(0xC8);  // Common output scan direction

    dep128064_write_cmd(0xDA);  // Common pads hardware configuration
    dep128064_write_cmd(0x12);

    dep128064_set_contrast(device.side_display_contrast);

    dep128064_write_cmd(0xD9);  // Discharge and precharge period set
    dep128064_write_cmd(0xF1);

    dep128064_write_cmd(0xDB);  // Vcom deselect level
    dep128064_write_cmd(0x40);

    dep128064_write_cmd(0x32);  // Vpp voltage to 8V (9V works too)

    dep128064_write_cmd(0xA4);  // Display on

    dep128064_write_cmd(0xA6);  // Normal display

    dep128064_power_ctrl(device.side_display_on);

    dep128064_screensaver_task_init();

}

static void dep128064_screensaver_task_init(void){

    screensaver_events = xEventGroupCreate();
    xTaskCreate(dep128064_screensaver_task,"Screensaver",2048,NULL,2,&screensaver_task_handle);

}
static void dep128064_screensaver_task(void* arg){

    while(1){
        xEventGroupWaitBits(screensaver_events,SCREENSAVER_ACTIVE_BIT,pdFALSE,pdTRUE,portMAX_DELAY);
        dep128064_print_screensaver();
        advance_screensaver();
        vTaskDelay(pdMS_TO_TICKS(screensaver_advance_interval_ms));
        if (!device.side_display_on){
            // Task ends itself automatically if side display is turned off
            xEventGroupClearBits(screensaver_events,SCREENSAVER_ACTIVE_BIT);
            dep128064_clear_screen();
        }
    }

}

static void dep128064_print_screensaver(void){

    if (device.side_display_on){
        u8g2_ClearBuffer(&u8g2);
        u8g2_SetBitmapMode(&u8g2, 1);
        u8g2_SetFontMode(&u8g2, 1);

        u8g2_DrawXBM(&u8g2, screensaver.x, screensaver.y, screensaver.size_x, screensaver.size_y, screensaver_logo);
        u8g2_SendBuffer(&u8g2);
    }
}

static void advance_screensaver(void){

    // Move screensaver logo
    screensaver.x += screensaver.speed_x;
    screensaver.y += screensaver.speed_y;
    // Reflections on side of screen
    if (((screensaver.x + screensaver.size_x) == SIDE_DISPLAY_PIX_X) || (screensaver.x == 0)){
        screensaver.speed_x *= -1;
    }
    if (((screensaver.y + screensaver.size_y) == SIDE_DISPLAY_PIX_Y) || (screensaver.y == 0)){
        screensaver.speed_y *= -1;
    }
}

static void generate_status_screen_bms(status_screen_TypeDef* status_screen, bms_typeDef* bms){

    // Create displaydata to show device status (doesnt print)
    // Battery state
    status_screen->battery_bars = (bms->battery_percentage > 10)?((bms->battery_percentage / 20) + 1):0;
    snprintf(status_screen->battery_percentage,(sizeof(status_screen->battery_percentage)),"%3u%%",bms->battery_percentage);
    status_screen->status_charging = (bms->charger_state&BMS_CHARGER_PRESENT) != 0;
    status_screen->battery_connected = (bms->charger_state&BMS_BAT_PRESENT) != 0;
    status_screen->warning_low_bat = (bms->battery_percentage <= 10) != 0;
    if ((bms->battery_crate <= 0) || (!(bms->charger_state&BMS_CHARGING))){
        // Battery not charging, Crate takes a while to become negative, bms.charger_state changes immediately
        // If battery crate > -5%/h battery time left values are unrealistic, always assume at least -5%/h battery crate:
        uint16_t total_mins_left =  (bms->battery_crate<-5000)
                                    ?
                                    ((bms->battery_percentage*1000*60) / (bms->battery_crate*-1))
                                    :
                                    (bms->battery_percentage*60/5);
        snprintf(   status_screen->battery_time_left,
                    sizeof(status_screen->battery_time_left),
                    "%2uh%2umin",
                    total_mins_left / 60,
                    total_mins_left % 60);
    }
    else{
        // Battery charging
        strcpy(status_screen->battery_time_left,"Charging");
    }
}

static void generate_status_screen_wifi(status_screen_TypeDef* status_screen, wifi_manager_TypeDef* wifi){

    if (!wifi->connected){
        status_screen->wifi_signal_waves = 0;
        strcpy(status_screen->wifi_name,"Not connected  ");
    }
    else{
        if (wifi->wifi_rssi < -85){
            status_screen->wifi_signal_waves = 1;
        }
        else if (wifi->wifi_rssi < -70){
            status_screen->wifi_signal_waves = 2;
        }
        else{
            status_screen->wifi_signal_waves = 3;
        }
        // Wifi name
        if (strlen(wifi->connected_wifi.ssid) > 18){
            // SSID too long
            // Only a copy of SSID:
            wifi->connected_wifi.ssid[16] = '.';
            wifi->connected_wifi.ssid[17] = '.';
            wifi->connected_wifi.ssid[18] = '.';
            wifi->connected_wifi.ssid[19] = '\0';
        }
        strcpy(status_screen->wifi_name,wifi->connected_wifi.ssid);
    }
    // Rest not implemented yet: Assign default values
    strcpy(status_screen->ai_name,"ChatGPT   ");
    status_screen->api_provided = 0;
    strcpy(status_screen->ai_model,"GPT 5.5   ");
    strcpy(status_screen->storage_used," 67/ 67MB");
    strcpy(status_screen->camera_state,"Off");
}

static void dep128064_print_status_screen(status_screen_TypeDef* status_screen){

    if (!device.side_display_on){
        return;
    }
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_SetFontMode(&u8g2, 1);
    // Draw battery sign
    if (status_screen->battery_connected){
        u8g2_DrawXBM(&u8g2, 3, 1, 19, 8, battery_icons[status_screen->battery_bars]);
    }
    else{
        u8g2_DrawXBM(&u8g2, 3, 1, 19, 8, image_no_bat);
    }
    // Battery percentage
    u8g2_SetFont(&u8g2, u8g2_font_t0_11b_tr);
    if (status_screen->battery_connected){
        u8g2_DrawStr(&u8g2, 26, 9, status_screen->battery_percentage);
        u8g2_DrawXBM(&u8g2, 69, 1, 8, 8, image_clock_bits);
        u8g2_DrawStr(&u8g2, 79, 9, status_screen->battery_time_left);
    }
    // Charging info
    if (status_screen->status_charging){
        u8g2_DrawXBM(&u8g2, 52, 2, 10, 7, image_charging_icon_bits);
    }
    // Wifi sign
    if (status_screen->wifi_signal_waves == 0){
        u8g2_DrawXBM(&u8g2, 3, 15, 12, 8, image_wifi0_3_bits);
    }
    else if (status_screen->wifi_signal_waves == 1){
        u8g2_DrawXBM(&u8g2, 5, 19, 6, 4, image_wifi1_3_bits);
    }
    else if (status_screen->wifi_signal_waves == 2){
        u8g2_DrawXBM(&u8g2, 4, 17, 8, 6, image_wifi2_3_bits);
    }
    else{
        u8g2_DrawXBM(&u8g2, 3, 15, 10, 8, image_wifi3_3_bits);
    }
    // Wifi name
    u8g2_DrawStr(&u8g2, 16, 23, status_screen->wifi_name);
    // AI Name
    u8g2_DrawXBM(&u8g2, 4, 28, 8, 8, image_robot_bits);
    u8g2_DrawStr(&u8g2, 16, 36, status_screen->ai_name);
    u8g2_DrawXBM(&u8g2, 3, 40, 8, 8, image_speechbubble_bits);
    u8g2_DrawStr(&u8g2, 16, 48, status_screen->ai_model);
    // API key state
    u8g2_DrawStr(&u8g2, 96, 36, "API");
    u8g2_DrawXBM(&u8g2, 114, 29, 11, 6, image_key_bits);
    if (status_screen->api_provided){
        u8g2_DrawXBM(&u8g2, 85, 29, 10, 7, image_check_bits);
    }
    else{
        u8g2_DrawXBM(&u8g2, 85, 29, 10, 7, image_cross_small_bits);
    }
    u8g2_DrawLine(&u8g2, 2, 12, 123, 12);
    // Camera state
    u8g2_DrawStr(&u8g2, 107, 62, status_screen->camera_state);
    u8g2_DrawLine(&u8g2, 2, 25, 124, 25);
    u8g2_DrawXBM(&u8g2, 93, 54, 10, 8, image_camera_bits);
    u8g2_DrawLine(&u8g2, 3, 51, 124, 51);
    // File system state
    u8g2_DrawXBM(&u8g2, 3, 54, 11, 8, image_folder_open_bits);
    u8g2_DrawStr(&u8g2, 16, 62, status_screen->storage_used);
    u8g2_SendBuffer(&u8g2);
}







// Exported functions

void dep128064_init(void){
    // Initialize the display
    dep128064_start_display();
    // Initialize U8G2
    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2,U8G2_R0,cb_dep128064_i2c_transmit,cb_dep128064_delay);
    // Only setup U8G2, in this code a custom Init Sequence dep128064_start_display() 
    // for the DEP128064 Display is implemented for better control over the hardware:
    u8g2_InitInterface(&u8g2);
}

void dep128064_set_contrast(uint8_t contrast){

    // No need to check contrast values as they can go from 0...255
    // The higher this value the higher the current consumption
    dep128064_write_cmd(DEP128064_CONTRAST_SET);
    dep128064_write_cmd(contrast);
}

void dep128064_clear_screen(void){

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_SetFontMode(&u8g2, 1);

    u8g2_SendBuffer(&u8g2);
}

void dep128064_power_ctrl(uint8_t display_on){

    // only use device.side_display_on as argument
    if (!display_on){
        dep128064_write_cmd(DEP128064_DISPLAYONOFF_CTRL|DEP128064_OFF);
    }
    else{
        dep128064_write_cmd(DEP128064_DISPLAYONOFF_CTRL|DEP128064_ON);
    }
}

void dep128064_power_toggle(void){

    if (device.side_display_toggle_mode){
        device.side_display_on ^= 0x01;     // toggle
        dep128064_power_ctrl(device.side_display_on);
    }
}

void dep128064_start_screensaver(uint16_t advance_interval){

    
    screensaver_advance_interval_ms = advance_interval;
    screensaver_advance_interval_ms=    (screensaver_advance_interval_ms>SIDE_DISPLAY_MIN_CYCLETIME_MS)
                                        ? 
                                        screensaver_advance_interval_ms
                                        :
                                        SIDE_DISPLAY_MIN_CYCLETIME_MS;
    screensaver.x = 0;     
    screensaver.y = 0;     
    dep128064_print_screensaver();       
    xEventGroupSetBits(screensaver_events,SCREENSAVER_ACTIVE_BIT);
}

void dep128064_end_screensaver(void){

    dep128064_clear_screen();
    vTaskDelay(pdMS_TO_TICKS(500));
    xEventGroupClearBits(screensaver_events,SCREENSAVER_ACTIVE_BIT);
}


void dep128064_refresh_status_screen(bms_typeDef* bms,wifi_manager_TypeDef* wifi){

    if (device.side_display_on){
        generate_status_screen_bms(&status_screen, bms);
        generate_status_screen_wifi(&status_screen, wifi);
        dep128064_print_status_screen(&status_screen);
    }
}