idf_component_register(SRCS "rssi_wifi_comp.c"
                    PRIV_REQUIRES esp_wifi nvs_flash
                    INCLUDE_DIRS "include")
