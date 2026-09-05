/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

camera.c: Functions related to the camera and image processing (using esp_camera component)

*/

// Include 

#include "esp_camera.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_littlefs.h"


#include "AI_calc_camera.h"
#include "AI_calc_device.h"






// Static variables

static nvs_handle_t camera_nvs_handle;

// Settings
static uint8_t camera_on;                           // boolean
static framesize_t camera_current_framesize;
static uint8_t camera_current_framesize_code;       // only a helper variable
static uint8_t camera_current_jpeg_quality;

static framesize_t framesizes[4];

// Framebuffer for esp_camera component
static camera_fb_t *pic;

// Only for debugging through a http server
static httpd_handle_t camera_http_server = NULL;





// Static function declarations

static void camera_nvs_init(void);
static void nvs_get_cam_settings(framesize_t* framesize, uint8_t* jpeg_quality);
static void nvs_save_cam_settings(uint8_t framesize_code, uint8_t jpeg_quality);
static esp_err_t camera_directory_get_size(size_t* directory_size, uint8_t* file_cnt);
static esp_err_t ov5640_init(void);

static esp_err_t camera_http_get_handler(httpd_req_t *req);             // Only for debugging
static esp_err_t camera_image_http_get_handler(httpd_req_t *req);       // Only for debugging





// Static functions

static void camera_nvs_init(void){

    ESP_ERROR_CHECK(nvs_open("camera", NVS_READWRITE, &camera_nvs_handle));
}

static void nvs_get_cam_settings(framesize_t* framesize, uint8_t* jpeg_quality){

    // Returns default values if invalid values are saved
    if (nvs_get_u8(camera_nvs_handle,"jpeg_quality",jpeg_quality) != ESP_OK){
        // Set to default
        *jpeg_quality = DEFAULT_JPEG_QUALITY;
    }
    if (nvs_get_u8(camera_nvs_handle,"framesize",&camera_current_framesize_code) != ESP_OK){
        // Set to default
        *framesize  = framesizes[DEFAULT_FRAMESIZE_CODE];
    }
    else if (camera_current_framesize_code < sizeof(framesizes)){
        *framesize = framesizes[camera_current_framesize_code];
    }
    else{
        // Set to default
        *framesize  = framesizes[DEFAULT_FRAMESIZE_CODE];
    }
}

static void nvs_save_cam_settings(uint8_t framesize_code, uint8_t jpeg_quality){

    nvs_set_u8(camera_nvs_handle,"jpeg_quality",jpeg_quality);
    nvs_set_u8(camera_nvs_handle,"framesize",framesize_code);
}

static esp_err_t ov5640_init(void){

    // Do not call in the camera_init() function, only when taking a picture
    camera_config_t cam_cfg = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_N_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq_hz = (XCLK_EXT_FREQ_MHZ * 1000 * 1000),
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = camera_current_framesize,
        .jpeg_quality = camera_current_jpeg_quality,
        .fb_count = 1,       
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
    };
    esp_err_t ret = esp_camera_init(&cam_cfg);
    if (ret != ESP_OK){
        return ret;
    }
    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor == NULL){
        return ESP_FAIL;
    }
    // Without this the images appear mirrored along the Y-Axis:
    sensor->set_hmirror(sensor, 1);
    return ESP_OK;
}

static esp_err_t camera_directory_get_size(size_t* directory_size, uint8_t* file_cnt){

    // Helper function that returns:
    // How many JPGs are saved in /littlefs/cam/
    // How many bytes /littlefs/cam/ needs in total
    DIR* dir = opendir("/littlefs/cam");
    if (!dir){
        return ESP_FAIL;
    }
    struct dirent* entry;
    struct stat st;
    *directory_size = 0;
    *file_cnt = 0;
    while ((entry = readdir(dir)) != NULL){
        // Ignore sub directories (those are not supposed to exist anyways)
        if (entry->d_type == DT_DIR)
            continue;
        char filepath[300];
        snprintf(filepath, sizeof(filepath),"%s/%s", "/littlefs/cam", entry->d_name);
        if (stat(filepath, &st) != 0) {
            closedir(dir);
            return ESP_FAIL;
        }
        *directory_size += st.st_size;
        (*file_cnt)++;
    }
    closedir(dir);
    ESP_LOGI("Camera dir","Size: %d Pictures: %d",*directory_size,*file_cnt);
    return ESP_OK;
}

// Only for debugging:
static esp_err_t camera_http_get_handler(httpd_req_t *req){

    char filename[64];
    uint8_t pictures_cnt = 0;
    size_t directory_size;
    if (camera_directory_get_size(&directory_size, &pictures_cnt) != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req,"Could not read camera directory\n",HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    if (pictures_cnt == 0) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req,"No camera pictures available\n",HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
                            "<!DOCTYPE html>"
                            "<html><head><title>Camera</title></head><body>"
                            "<h1>Camera Pictures</h1>");
    for (uint8_t i = 0; i < MAX_SAVED_PICTURES; i++) {
        snprintf(filename,sizeof(filename),"/littlefs/cam/camera[%03u].jpg",i);
        FILE *file = fopen(filename, "rb");
        if (file == NULL)
            continue;
        fclose(file);
        char html[128];
        snprintf(html,sizeof(html),"<h3>camera[%03u].jpg</h3>""<img src=\"/camera/%03u\" style=\"max-width:100%%;\"><br><br>",i, i);
        httpd_resp_sendstr_chunk(req, html);
    }
    httpd_resp_sendstr_chunk(req,
        "</body></html>");
    return httpd_resp_send_chunk(req, NULL, 0);
}

// Only for debugging:
static esp_err_t camera_image_http_get_handler(httpd_req_t *req){

    char filename[64];
    // URI should be /camera/000 ... /camera/(MAX_SAVED_PICTURES-1)
    int picture_number = 0;
    if (sscanf(req->uri, "/camera/%d", &picture_number) != 1 ||
        picture_number < 0 ||
        picture_number >= MAX_SAVED_PICTURES) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_send(req, "Invalid picture\n", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    snprintf(filename,sizeof(filename),"/littlefs/cam/camera[%03d].jpg",picture_number);
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_send(req,"Picture not found\n",HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "image/jpeg");
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        esp_err_t ret = httpd_resp_send_chunk(req,buffer,bytes_read);
        if (ret != ESP_OK) {
            fclose(file);
            return ret;
        }
    }
    fclose(file);
    return httpd_resp_send_chunk(req, NULL, 0);
}






// Exported functions

void camera_init(void){

    // Does not activate camera, just inits some settings
    camera_on = 0;
    framesizes[VGA_640_480_PX] = FRAMESIZE_VGA;
    framesizes[XGA_1024_768_PX] = FRAMESIZE_XGA;
    framesizes[UXGA_1600_1200_PX] = FRAMESIZE_UXGA;
    framesizes[QSXGA_2560_1920_PX] = FRAMESIZE_QSXGA;
    camera_nvs_init();
    nvs_get_cam_settings(&camera_current_framesize, &camera_current_jpeg_quality);

}

esp_err_t camera_take_picture(void){

    // If this function returns ESP_OK the captured image is succesfully saved in LittleFS
    camera_on = 1;
    ESP_LOGI("Camera","Taking pic, Framesizecode: %d",camera_current_framesize_code);
    ESP_LOGI("Camera","JPEG Quality: %d",camera_current_jpeg_quality);
    gpio_set_level(CAMERA_POWER_ENABLE,1);
    vTaskDelay(pdMS_TO_TICKS(500));
    if (ov5640_init() != ESP_OK){
        // Camera Init error, shut down camera again
        gpio_set_level(CAMERA_POWER_ENABLE,0);
        camera_on = 0;
        return ESP_FAIL;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    pic = esp_camera_fb_get();
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!pic){
        // Frame capture error, deinit camera and shutdown camera
        ESP_LOGI("Camera","Frame capture error, shutting camera down");
        // PSRAM DMA does not appear to work with JPEGs (struggles to find SOI & EOI):
        ESP_LOGI("Camera","Try turning off CONFIG_CAMERA_PSRAM_DMA through menuconfig");
        esp_camera_deinit();
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(CAMERA_POWER_ENABLE,0);
        camera_on = 0;
        return ESP_FAIL;
    }
    // Capture successful, safe to flash memory (need LittleFS initialized)
    size_t cam_dir_size;
    uint8_t pictures_cnt;
    if (camera_directory_get_size(&cam_dir_size,&pictures_cnt) != ESP_OK){
        ESP_LOGI("Camera","Error in littleFS cam directory?");
        esp_camera_fb_return(pic);
        esp_camera_deinit();
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(CAMERA_POWER_ENABLE,0);
        camera_on = 0;
        return ESP_FAIL;
    }
    // First Check if there is enough space in flash
    if (((pic->len + cam_dir_size) >= PICTURES_MAX_TOTAL_SIZE) || (pictures_cnt >= MAX_SAVED_PICTURES)){
        // Flash memory full
        ESP_LOGI("Camera","Not enough space any more in Flash");
        esp_camera_fb_return(pic);
        esp_camera_deinit();
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(CAMERA_POWER_ENABLE,0);
        camera_on = 0;
        return ESP_FAIL;
    }
    // There is still space:
    char new_file_name[64];
    snprintf(new_file_name,sizeof(new_file_name),"/littlefs/cam/camera[%03u].jpg",(pictures_cnt));
    FILE *f = fopen(new_file_name, "wb");     
    if (!f){
        ESP_LOGI("Camera","LittleFS error, can't save picture");
        esp_camera_fb_return(pic);
        esp_camera_deinit();
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(CAMERA_POWER_ENABLE,0);
        camera_on = 0;
        return ESP_FAIL;
    }
    size_t new_pic_size = fwrite(pic->buf, 1, pic->len, f);
    fclose(f);
    ESP_LOGI("Camera", "Saved %zu / %zu bytes", new_pic_size, pic->len);
    // Deinit after successfull camera operation
    esp_camera_fb_return(pic);
    esp_camera_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(CAMERA_POWER_ENABLE,0);
    camera_on = 0;
    return ESP_OK;
}

void camera_set_jpeg_quality(uint8_t jpeg_quality){

    camera_current_jpeg_quality = (jpeg_quality<64)?jpeg_quality:DEFAULT_JPEG_QUALITY;
    nvs_save_cam_settings(camera_current_framesize_code,camera_current_jpeg_quality);
}

void camera_set_framesize(uint8_t framesize_code){

    camera_current_framesize_code = framesize_code;
    camera_current_framesize =  (framesize_code<sizeof(framesizes))
                                ?
                                framesizes[framesize_code]
                                :
                                framesizes[DEFAULT_FRAMESIZE_CODE];
    nvs_save_cam_settings(camera_current_framesize_code,camera_current_jpeg_quality);
}

esp_err_t delete_camera_directory(void){

    DIR* dir = opendir("/littlefs/cam");
    if (!dir) {
        return ESP_FAIL;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore subdirectories
        if (entry->d_type == DT_DIR){
            continue;
        }
        char filepath[300];
        snprintf(filepath,sizeof(filepath),"/littlefs/cam/%s",entry->d_name);
        if (unlink(filepath) != 0) {
            closedir(dir);
            return ESP_FAIL;
        }
    }
    closedir(dir);
    return ESP_OK;
}

esp_err_t get_saved_pictures_paths(size_t* directory_size, uint8_t* file_cnt, char (*pic_paths)[64]){

    if (camera_directory_get_size(directory_size, file_cnt) != ESP_OK){
        return ESP_FAIL;
    }
    for (uint8_t i=0; i<(*file_cnt); i++){
        snprintf(pic_paths[i],sizeof(pic_paths[i]),"/littlefs/cam/camera[%03u].jpg",i);
    }
    return ESP_OK;
}

// Only for debugging:
void camera_start_debug_http_server(void){

    if (camera_http_server != NULL) {
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    httpd_uri_t camera_uri = {
        .uri      = "/",
        .method   = HTTP_GET,
        .handler  = camera_http_get_handler,
        .user_ctx = NULL
    };
    httpd_uri_t camera_image_uri = {
        .uri       = "/camera/*",
        .method    = HTTP_GET,
        .handler   = camera_image_http_get_handler,
        .user_ctx  = NULL
    };
    ESP_ERROR_CHECK(httpd_start(&camera_http_server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(camera_http_server,&camera_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(camera_http_server,&camera_image_uri));
    ESP_LOGI("Camera HTTP", "Server started");
}

// Only for debugging
void camera_end_debug_http_server(void){

    if (camera_http_server != NULL) {
        httpd_stop(camera_http_server);
        camera_http_server = NULL;
        ESP_LOGI("Camera HTTP", "Server stopped");
    } 
}