idf_component_register(SRCS "mqtts_comp.c"
                    PRIV_REQUIRES mqtt esp_partition nvs_flash esp_netif app_update json esp_wifi
                    INCLUDE_DIRS "." "include")

