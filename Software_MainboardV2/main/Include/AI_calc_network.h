/*

ChatGPT Hardware Hack for calculators: Software V2

© 2026 Jonas Heselschwerdt
Licensed under CC BY-NC 4.0

network.h: Wifi manager settings

*/

#ifndef WIFI_H
#define WIFI_H


// Includes





// Settings

#define WIFI_POLL_SCAN_FINISHED_INTERVAL 200            // in ms
#define WIFI_SCAN_RETRY_INTERVAL 2000                   // in ms
#define WIFI_STILL_CONNECTED_CHECK_INTERVAL 5000        // in ms


#define WIFI_MAX_STORED_LOGINDATA 16         // SSID + Password pairs
#define WIFI_MAX_SSID_LENGTH 32
#define WIFI_MAX_PASSW_LENGTH 64




// Wifi login data

typedef struct{
    char ssid[WIFI_MAX_SSID_LENGTH+1];
    char passw[WIFI_MAX_PASSW_LENGTH+1];
    uint8_t legit;                              // if legit = 0: ignored in wifi connection logic
} wifi_login_TypeDef;





// Wifi manager typedef

typedef struct {
    uint8_t connected;                          // boolean
    uint8_t scanning;                           // boolean
    uint8_t scan_done;                          // boolean
    wifi_login_TypeDef connected_wifi;          // only legit when connected = 1
    uint8_t prefered_wifi_exists;               // boolean
    wifi_login_TypeDef prefered_wifi_login;     // has highest priority when scanning (only legit when prefered_wifi_exists = 1)
    int wifi_rssi;                              // in dBm, indicator of wifi signal quality, higher is better
    uint8_t IPv4[4];
}wifi_manager_TypeDef;








// Exported functions

void wifi_init(void);
esp_err_t wifi_add_login_credentials(char* new_ssid,char* new_pass, uint8_t index);
esp_err_t wifi_set_prefered_wifi(uint8_t index);
void get_wifi_state(wifi_manager_TypeDef* wifi);



#endif