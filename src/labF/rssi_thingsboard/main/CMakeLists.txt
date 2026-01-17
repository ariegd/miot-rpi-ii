idf_component_register(SRCS "rssi_thingsboard.c"
                       PRIV_REQUIRES spi_flash 
                       INCLUDE_DIRS ""
                       REQUIRES rssi_wifi_comp)
