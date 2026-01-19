idf_component_register(SRCS "rssi_thingsboard.c"
                                                INCLUDE_DIRS "."
                                                REQUIRES nvs_flash esp_event esp_netif mqtts_comp rssi_wifi_comp)
