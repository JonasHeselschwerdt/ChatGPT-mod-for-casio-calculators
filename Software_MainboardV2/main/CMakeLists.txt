idf_component_register(SRCS "Source/AI_calc_main.c"
                            "Source/AI_calc_UI.c"
                            "Source/AI_calc_keypad.c"
                            "Source/AI_calc_maindisplay.c"
                            "Source/AI_calc_device.c"
                            "Source/AI_calc_sidedisplay.c"
                            "Source/AI_calc_battery.c"
                            "Source/AI_calc_network.c"
                            "Source/AI_calc_camera.c"
                    INCLUDE_DIRS "Include"
                    REQUIRES driver
                    REQUIRES esp_timer
                    REQUIRES esp_adc
                    REQUIRES u8g2
                    REQUIRES nvs_flash
                    REQUIRES esp_wifi
                    REQUIRES esp_psram
                    REQUIRES esp_http_server)
