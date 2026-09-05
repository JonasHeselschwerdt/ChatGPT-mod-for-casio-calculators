/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

camera.h: Camera settings (using esp_camera component)

*/

// Include

#include "esp_err.h"






// Camera Pins

#define CAM_PIN_PWDN -1         // not used -> hardwired LOW
#define CAM_PIN_N_RESET -1      // not used -> hardwired HIGH
#define CAM_PIN_XCLK -1         // supplied through Y1 on board
#define CAM_PIN_SIOD 6
#define CAM_PIN_SIOC 5

#define CAM_PIN_D7 9
#define CAM_PIN_D6 10
#define CAM_PIN_D5 11
#define CAM_PIN_D4 13
#define CAM_PIN_D3 21
#define CAM_PIN_D2 48
#define CAM_PIN_D1 47
#define CAM_PIN_D0 14
#define CAM_PIN_VSYNC 1
#define CAM_PIN_HREF 2
#define CAM_PIN_PCLK 12





// Needed in esp_camera component

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif




// External XCLK source (Y1)

#define XCLK_EXT_FREQ_MHZ 24





// Framesize codes for camera_set_framesize()
// Indices of the static framesizes-array, get saved as uint8_ts in NVS

#define VGA_640_480_PX 0
#define XGA_1024_768_PX 1
#define UXGA_1600_1200_PX 2
#define QSXGA_2560_1920_PX 3






// Default camera settings

#define DEFAULT_FRAMESIZE_CODE VGA_640_480_PX
#define DEFAULT_JPEG_QUALITY 12





// Flash memory restrictions

#define PICTURES_MAX_TOTAL_SIZE (3*1024*1024)
#define MAX_SAVED_PICTURES 4

/*
Important: Be very careful about changing PICTURES_MAX_TOTAL_SIZE
Do not set higher than 4 MiB
(only use 8MiB PSRAM models for this project)
*/

#if PICTURES_MAX_TOTAL_SIZE > (4*1024*1024)
#error "Camera directory should not be that large"
#endif







// Exported functions

void camera_init(void);

esp_err_t camera_take_picture(void);
esp_err_t delete_camera_directory(void);

void camera_set_jpeg_quality(uint8_t jpeg_quality);
void camera_set_framesize(uint8_t framesize_code);

esp_err_t get_saved_pictures_paths(size_t* directory_size, uint8_t* file_cnt, char (*pic_paths)[64]);

void camera_start_debug_http_server(void);      // Only for debugging
void camera_end_debug_http_server(void);        // Only for debugging
