/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

UI.c: UI-Functions and Variables

*/

// Includes

#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_err.h"

#include "AI_calc_UI.h"





// Extern UI-Typedef

UI_TypeDef UI = {
    .autooff_tresh_mins = 255,     // set to max upon initialization
    .UI_mode = UI_MODE_SCRIBBLE
};





// Scribble Mode variables







// Chatview Mode variables







// Menu Mode UI variables and constants







// Little FS variables







// Static Scribble mode function declarations






// Static Chatview Mode function declarations







// Static Menu Mode function declarations







// Static LittleFS function declarations

static void littleFS_init(void);







// Static Scribble mode functions






// Static Chatview Mode functions







// Static Menu Mode functions







// Static LittleFS functions

static void littleFS_init(void){

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",       
        .partition_label = "littlefs",  // see partitions.csv
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("LFS", "LittleFS mount failed: %s",esp_err_to_name(ret));
        return;
    }
    size_t total = 0, used = 0;
    esp_littlefs_info("littlefs", &total, &used);
    ESP_LOGI("LFS", "LittleFS mounted: %d KB total, %d KB used", total / 1024, used / 1024);
}






// Generic extern UI-Functions

void UI_init(void){

    littleFS_init();
}





// Extern Scribble Mode functions







// Extern Chatview Mode functions







// Extern Menu Mode functions








// Extern LittleFS functions