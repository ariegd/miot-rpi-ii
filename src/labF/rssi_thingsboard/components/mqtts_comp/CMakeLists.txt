idf_component_register(SRCS "mqtts_comp.c" "certs.c"
                    PRIV_REQUIRES mqtt esp_partition nvs_flash esp_netif app_update
                    INCLUDE_DIRS "." "include"
                    EMBED_TXTFILES "certs/mqtt_eclipseprojects_io.pem")
