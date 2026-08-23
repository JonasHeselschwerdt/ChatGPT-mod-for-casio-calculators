/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

network.h: Wifi manager code

*/

// Inlcudes

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "AI_calc_network.h"
#include "AI_calc_camera.h"     // Only for debugging





// Static variables

static nvs_handle_t wifi_ssid_pass_handle;

static TaskHandle_t wifi_task_handle;

static wifi_login_TypeDef login_data[WIFI_MAX_STORED_LOGINDATA];
static wifi_manager_TypeDef wifi_manager;




// Static function declarations

static void wifi_nvs_init(void);
static esp_err_t wifi_load_login(wifi_login_TypeDef* login, uint8_t login_index);
static esp_err_t wifi_save_login(wifi_login_TypeDef* new_login, uint8_t login_index);

static void wifi_manager_task(void* arg);
static void wifi_event_handler(void *arg,esp_event_base_t event_base,int32_t event_id,void *event_data);

static esp_err_t wifi_try_connect_to(wifi_login_TypeDef* target_AP);
static esp_err_t evaluate_AP_scan(wifi_ap_record_t** found_APs, uint16_t* found_AP_count);







// Static functions

static void wifi_nvs_init(void){

    // NVS used to save SSID-Password pairs (up to WIFI_MAX_STORED_LOGINDATA)
    // Open namespace 'wifi'
    ESP_ERROR_CHECK(nvs_open("wifi", NVS_READWRITE, &wifi_ssid_pass_handle));
    wifi_manager.prefered_wifi_exists = 0;  // Always the case upon restart
    for (uint8_t i=0; i<WIFI_MAX_STORED_LOGINDATA; i++){
        wifi_load_login(&login_data[i],i);
    }
    
}

static void wifi_manager_task(void* arg){

    // added this delay because the code below seemed to mess with USB JTAG?
    // this delay gives some time to start a flash process during startup
    vTaskDelay(pdMS_TO_TICKS(4000));
    // Wifi, Netif and event handler init
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    // Forms a state machine together with wifi_event_handler()
    while(1){
        //ESP_LOGI("Wifi Task","Task loop beginning");
        if (!wifi_manager.connected){
            // Try to connect to wifi
            if (!wifi_manager.scanning && !wifi_manager.scan_done){
                // Start new scan
                //ESP_LOGI("Wifi Task","Scan starting");
                wifi_manager.scanning = 1;
                wifi_manager.scan_done = 0;
                wifi_scan_config_t scan_cfg = {
                    .ssid = NULL,
                    .bssid = NULL,
                    .channel = 0,
                    .show_hidden = false,
                    .scan_type = WIFI_SCAN_TYPE_ACTIVE,
                    .scan_time.active = {.min = 100,.max = 300}
                };
                esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
                if (err != ESP_OK){
                    // Scan failed
                    //ESP_LOGI("Wifi Task","Scan failed");
                    wifi_manager.scanning = 0;
                    vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_RETRY_INTERVAL));
                    continue;
                }
            }
            if (wifi_manager.scan_done){
                //ESP_LOGI("Wifi Task","Scan done");
                wifi_manager.scan_done = 0;
                wifi_ap_record_t* found_APs = NULL;
                uint16_t found_AP_count;
                uint8_t tried_connect = 0;
                if (evaluate_AP_scan(&found_APs,&found_AP_count) == ESP_OK){
                    // Try connecting to APs
                    if (wifi_manager.prefered_wifi_exists){
                        for (uint16_t i=0; i<found_AP_count; i++){
                            if (strcmp((const char*) found_APs[i].ssid,wifi_manager.prefered_wifi_login.ssid) == 0){
                                // Found prefered wifi AP
                                //ESP_LOGI("Wifi Task","Found prefered AP");
                                if (wifi_try_connect_to(&wifi_manager.prefered_wifi_login) == ESP_OK){
                                    // Connecting worked
                                    wifi_manager.connected_wifi = wifi_manager.prefered_wifi_login;
                                }
                                tried_connect = 1;
                                break;
                            }
                        }
                    }
                    if (!tried_connect){
                        // Prefered wifi was not available, try other ones
                        for (uint8_t i=0; i<WIFI_MAX_STORED_LOGINDATA; i++){
                            if (login_data[i].legit){
                                for (uint16_t j=0; j<found_AP_count; j++){
                                    if (strcmp((const char*) found_APs[j].ssid,login_data[i].ssid) == 0){
                                        // Found one of the other wifis
                                        //ESP_LOGI("Wifi Task","Found AP");
                                        if (wifi_try_connect_to(&login_data[i]) == ESP_OK){
                                            wifi_manager.connected_wifi = login_data[i];
                                        }
                                        tried_connect = 1;
                                        break;
                                    }
                                }
                            }
                            if (tried_connect){
                                break;
                            }
                        }
                    }
                    free(found_APs);
                    if (!wifi_manager.connected){
                        // No available wifi found, wait
                        //ESP_LOGI("Wifi Task","Didn't find AP that matches in scan results");
                        vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_RETRY_INTERVAL));
                        continue;
                    }
                }
            }
            //ESP_LOGI("Wifi Task", "Waiting for scan to finish");
            vTaskDelay(pdMS_TO_TICKS(WIFI_POLL_SCAN_FINISHED_INTERVAL));
        }
        else{
            // Connected, check rssi, don't do anything else
            esp_wifi_sta_get_rssi(&wifi_manager.wifi_rssi);
            //ESP_LOGI("Wifi Task","Still connected");
            vTaskDelay(pdMS_TO_TICKS(WIFI_STILL_CONNECTED_CHECK_INTERVAL));
            continue;
        }
    }
}

static void wifi_event_handler(void *arg,esp_event_base_t event_base,int32_t event_id,void *event_data){

    // Event handler callback fucntion
    if (event_base == WIFI_EVENT){
        switch (event_id){
            case WIFI_EVENT_STA_START:
                wifi_manager.connected = 0;
                break;
            case WIFI_EVENT_SCAN_DONE:
                wifi_manager.scanning = 0;
                wifi_manager.scan_done = 1;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                wifi_manager.connected = 0;
            default:
                break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        wifi_manager.connected = 1;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_manager.IPv4[0] = event->ip_info.ip.addr & 0xFF;
        wifi_manager.IPv4[1] = (event->ip_info.ip.addr >> 8) & 0xFF;
        wifi_manager.IPv4[2] = (event->ip_info.ip.addr >> 16) & 0xFF;
        wifi_manager.IPv4[3] = (event->ip_info.ip.addr >> 24) & 0xFF;
        // Only for cameradebugging:
        camera_start_debug_http_server();
    }
}

static esp_err_t wifi_try_connect_to(wifi_login_TypeDef* target_AP){

    // For connecting to scanned APs that match a saved SSID
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, target_AP->ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, target_AP->passw, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    return ESP_OK;
}

static esp_err_t evaluate_AP_scan(wifi_ap_record_t** found_APs, uint16_t* found_AP_count){

    if ((esp_wifi_scan_get_ap_num(found_AP_count) != ESP_OK) || (*found_AP_count == 0)){
        return ESP_FAIL;
    }
    // Allocate memory
    *found_APs = (wifi_ap_record_t*)malloc(*found_AP_count * sizeof(wifi_ap_record_t));
    if (*found_APs == NULL){
        return ESP_FAIL;
    }
    // Save all found APs
    if (esp_wifi_scan_get_ap_records(found_AP_count,*found_APs) != ESP_OK){
        free(*found_APs);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t wifi_save_login(wifi_login_TypeDef* new_login, uint8_t login_index){

    if (login_index >= WIFI_MAX_STORED_LOGINDATA){
        return ESP_FAIL;
    }
    char ssid_key[7];
    char passw_key[7];
    char login_legit_key[8];
    snprintf(ssid_key,sizeof(ssid_key),"ssid%u",login_index);
    snprintf(passw_key,sizeof(passw_key),"pass%u",login_index);
    snprintf(login_legit_key,sizeof(login_legit_key),"legit%u",login_index);
    if (nvs_set_str(wifi_ssid_pass_handle,ssid_key,new_login->ssid) != ESP_OK){
        nvs_set_u8(wifi_ssid_pass_handle,login_legit_key,0);
        return ESP_FAIL;
    }
    if (nvs_set_str(wifi_ssid_pass_handle,passw_key,new_login->passw) != ESP_OK){
        nvs_set_u8(wifi_ssid_pass_handle,login_legit_key,0);
        return ESP_FAIL;
    }
    nvs_set_u8(wifi_ssid_pass_handle,login_legit_key,new_login->legit);
    if (nvs_commit(wifi_ssid_pass_handle != ESP_OK)){
        return ESP_FAIL;
    }
    login_data[login_index] = *new_login;
    return ESP_OK;
}

static esp_err_t wifi_load_login(wifi_login_TypeDef* login, uint8_t login_index){

    if (login_index >= WIFI_MAX_STORED_LOGINDATA){
        return ESP_FAIL;
    }
    char ssid_key[7];
    char passw_key[7];
    char login_legit_key[8];
    size_t ssid_length = (WIFI_MAX_SSID_LENGTH+1);
    size_t passw_length = (WIFI_MAX_PASSW_LENGTH+1);
    snprintf(ssid_key,sizeof(ssid_key),"ssid%u",login_index);
    snprintf(passw_key,sizeof(passw_key),"pass%u",login_index);
    snprintf(login_legit_key,sizeof(login_legit_key),"legit%u",login_index);
    if (nvs_get_str(wifi_ssid_pass_handle,ssid_key,login->ssid,&ssid_length) != ESP_OK){
        login->legit = 0;
        return ESP_FAIL;
    }
    if (nvs_get_str(wifi_ssid_pass_handle,passw_key,login->passw,&passw_length) != ESP_OK){
        login->legit = 0;
        return ESP_FAIL;
    }
    if (nvs_get_u8(wifi_ssid_pass_handle,login_legit_key,&login->legit) != ESP_OK){
        login->legit = 0;
        return ESP_FAIL;
    }
    return ESP_OK;
}







// Exported functions


void wifi_init(void){

    wifi_nvs_init();
    // Wifi manager initial state
    wifi_manager.connected = 0;
    xTaskCreate(wifi_manager_task,"Wifi",8192,NULL,2,&wifi_task_handle);
}

esp_err_t wifi_add_login_credentials(char* new_ssid,char* new_pass, uint8_t index){

    wifi_login_TypeDef new_login;
    strcpy(new_login.ssid,new_ssid);
    strcpy(new_login.passw,new_pass);
    new_login.legit = 1;
    return (wifi_save_login(&new_login,index));
}

esp_err_t wifi_set_prefered_wifi(uint8_t index){

    // If multiple supported wifis are nearby, one can be set as prefered
    // This one has the highest priority when searching for available APs
    if (index >= WIFI_MAX_STORED_LOGINDATA){
        return ESP_FAIL;
    }
    if (login_data[index].legit){
        wifi_manager.prefered_wifi_exists = 1;
        wifi_manager.prefered_wifi_login = login_data[index];
        return ESP_OK;
    }
    else{
        return ESP_FAIL;
    }
}

void get_wifi_state(wifi_manager_TypeDef* wifi){

    // Getter fucntion for static wifi_manager
    wifi->connected = wifi_manager.connected;
    wifi->scanning = wifi_manager.scanning;
    wifi->scan_done = wifi_manager.scan_done;
    strcpy(wifi->connected_wifi.ssid,wifi_manager.connected_wifi.ssid);
    strcpy(wifi->connected_wifi.passw,wifi_manager.connected_wifi.passw);
    wifi->connected_wifi.legit = wifi_manager.connected_wifi.legit;
    wifi->prefered_wifi_exists = wifi_manager.prefered_wifi_exists;
    strcpy(wifi->prefered_wifi_login.ssid,wifi_manager.prefered_wifi_login.ssid);
    strcpy(wifi->prefered_wifi_login.passw,wifi_manager.prefered_wifi_login.passw);
    wifi->prefered_wifi_login.legit = wifi_manager.prefered_wifi_login.legit;
    wifi->wifi_rssi = wifi_manager.wifi_rssi;
    for (uint8_t i=0;i<4;i++){
        wifi->IPv4[i] = wifi_manager.IPv4[i];
    }
}