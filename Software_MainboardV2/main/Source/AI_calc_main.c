/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

main.c: App_main

*/

// Includes

#include <stdio.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "AI_calc_main.h"
#include "AI_calc_device.h"
#include "AI_calc_UI.h"
#include "AI_calc_keypad.h"
#include "AI_calc_maindisplay.h"
#include "AI_calc_battery.h"
#include "AI_calc_sidedisplay.h"
#include "AI_calc_network.h"
#include "AI_calc_camera.h"



/*
Code below is only for debugging/demonstration. Currently implemented functions are:
- Show screensaver on sidedisplay
- Show messages/animations on maindisplay
- Save wifi credentials and connect to wifi
- Show device informations (Wifi and battery) on sidedisplay
- Turn sidedisplay on/off
- Adjust display contrasts (side & maindisplay)
- Log pressed keys of keypad
- Take pictures with camera and host them on a local http server
- Adjust camera framesize and JPEG compression
*/

void app_main(void){
    // In the device_init() -> wifi_init -> wifi_manager_task() -> wifi_event_handler() a
    // http debug server for the camera is started
    // Type the IPv4 of the device into a browser to look at the latest picture
    device_init();
    UI_init();
    // Camera settings
    camera_set_jpeg_quality(6);
    camera_set_framesize(QSXGA_2560_1920_PX);
    if(device.debug_mode){
        dep128064_start_screensaver(100);
        // Debuggincode start Display
        char* debug_text[MAIN_DISPLAY_ROWS]={
            "                    ",
            SOFTWARE_VERSION_STRING,
            DEVICE_STATUS_STRING,
            "                    "    
        };
        dogm204_print_screen(debug_text);
        vTaskDelay(pdMS_TO_TICKS(2000));
        char *debug_message[MAIN_DISPLAY_ROWS] = {
            "====================",
            "    Created by      ",
            " @ElectrJonics on YT",
            "===================="
        };
        // Save Wifi credentials
        char test_ssid[] = "<Your SSID here>";
        char test_password[] = "<Your Password here>";
        wifi_add_login_credentials(test_ssid,test_password,0);
        dogm204_print_screen(debug_message);
        vTaskDelay(pdMS_TO_TICKS(2000));
        char* legal_notice[MAIN_DISPLAY_ROWS] = {
            " Independant Mod:   ",
            " Not affiliated     ",
            " with Casio Computer",
            " Co.,LTD            "
        };
        dogm204_print_screen(legal_notice);
        vTaskDelay(pdMS_TO_TICKS(2000));
        dogm204_start_loading_screen(" Loading...         ", 150);
        vTaskDelay(pdMS_TO_TICKS(5000));
        dogm204_end_loading_screen();
        char* info_text2[MAIN_DISPLAY_ROWS] = {
            " You can find all   ",
            " design files for   ",
            " this on my GitHub @",
            " JonasHeselschwerdt "
        };
        dogm204_print_screen(info_text2);
        vTaskDelay(pdMS_TO_TICKS(2000));
        char* fancy_text[MAIN_DISPLAY_ROWS]={
            EMPTY_LINE,
            "  This is a fancy   ",
            " swiping animation  ",
            EMPTY_LINE
        };
        dogm204_print_screen_fancy(fancy_text,3000);
        dep128064_end_screensaver();
        dep128064_clear_screen();
        // Debuggingcode start Keypad
        while(1){
            vTaskDelay(pdMS_TO_TICKS(UI_LOOP_DELAYTIME));
            if(!gpio_get_level(TCA8418_N_INTERRUPT)){
                update_pressed_keys();
                // Check shutdown condition
                if ((cur_pressed_keys[0].special_function == KEY_SHIFT_SPECIAL_FUNC) && (cur_pressed_keys[1].special_function == KEY_MENU_SPECIAL_FUNC)){
                    camera_end_debug_http_server();
                    vTaskDelay(pdMS_TO_TICKS(500));
                    powerlatch_shutdown();
                }
                // Press DEL Key to display battery stats
                if (cur_pressed_keys[0].special_function == KEY_BACK_SPECIAL_FUNC){
                    char line1[MAIN_DISPLAY_COLUMNS+1];
                    char line2[MAIN_DISPLAY_COLUMNS+1];
                    char line3[MAIN_DISPLAY_COLUMNS+1];
                    char line4[MAIN_DISPLAY_COLUMNS+1];
                    char* info_screen[MAIN_DISPLAY_ROWS] = {
                        line1,
                        line2,
                        line3,
                        line4
                    };
                    create_bms_info_screen(info_screen);
                    // New way of refreshing sidedisplay with getter functions (bms)
                    bms_typeDef battery_info;
                    get_bms_state(&battery_info);
                    // Wifi
                    wifi_manager_TypeDef wifi_info;
                    get_wifi_state(&wifi_info);
                    ESP_LOGI("Wifi name","%s",wifi_info.connected_wifi.ssid);
                    dep128064_refresh_status_screen(&battery_info, &wifi_info);
                    char* explanation[MAIN_DISPLAY_ROWS] = {
                        "====================",
                        " Printing device    ",
                        " Information        ",
                        "===================="
                    };
                    dogm204_print_screen(explanation);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    dogm204_print_screen(info_screen);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    strcpy(line1,"Connected to Wifi   ");
                    strcpy(line4,"                    ");
                    snprintf(line2,sizeof(line2),"%-20.20s",wifi_info.connected_wifi.ssid);
                    snprintf(line3,sizeof(line3),"IP:%3u.%3u.%3u.%3u  ",wifi_info.IPv4[0],wifi_info.IPv4[1],wifi_info.IPv4[2],wifi_info.IPv4[3]);
                    dogm204_print_screen(info_screen);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                }
                if (cur_pressed_keys[0].special_function == KEY_DOWN_SPECIAL_FUNC){
                    device.side_display_contrast -= 10;
                    ESP_LOGI("Device","Sidediscontr: %u",device.side_display_contrast);
                    dep128064_set_contrast(device.side_display_contrast);
                }
                if (cur_pressed_keys[0].special_function == KEY_UP_SPECIAL_FUNC){
                    device.side_display_contrast += 10;;
                    ESP_LOGI("Device","Sidediscontr: %u",device.side_display_contrast);
                    dep128064_set_contrast(device.side_display_contrast);
                }
                if (cur_pressed_keys[0].special_function == KEY_RIGHT_SPECIAL_FUNC){
                    device.main_display_contrast += 3;
                    ESP_LOGI("Device","Maindicontr: %u",device.main_display_contrast);
                    dogm204_set_contrast(device.main_display_contrast);
                }
                if (cur_pressed_keys[0].special_function == KEY_LEFT_SPECIAL_FUNC){
                    device.main_display_contrast -= 3;
                    ESP_LOGI("Device","Maindicontr: %u",device.main_display_contrast);
                    dogm204_set_contrast(device.main_display_contrast);
                }
                if (cur_pressed_keys[0].special_function == KEY_ENTER_SPECIAL_FUNC){
                    device.side_display_toggle_mode = 1;
                    dep128064_power_toggle();
                    // Camera test
                    camera_take_picture();
                }
                // Log pressed keys
                uint64_t cur_time = esp_timer_get_time() / 1000;
                for (uint8_t i=0; i<10;i++){
                    if (cur_pressed_keys[i].normal_meaning != '\0'){
                        ESP_LOGI("Keypad","Key %d: %c | pressed since %llu ms",(i+1),cur_pressed_keys[i].normal_meaning, (cur_time - cur_pressed_keys[i].press_timestamp));
                    }
                    if (cur_pressed_keys[i].special_function == KEY_NOT_DEFINED){
                        break;
                    }
                }
            }
        }
        // Debuggingcode end
    }
    else{
        // Device main loop
    }
}
