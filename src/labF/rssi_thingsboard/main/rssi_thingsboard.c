/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_log.h"
#include "rssi_wifi_comp.h"
#include "mqtts_comp.h"

#include "nvs_flash.h"    // Requerido para nvs_flash_init()
#include "esp_netif.h"    // Requerido para esp_netif_init()
#include "esp_event.h"    // Requerido para esp_event_loop_create_default()

static const char *TAG = "RSSI_ThingsBoard";

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando sistema RSSI_ThingsBoard...");

    // 1. Inicialización de NVS (Almacenamiento no volátil)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicialización de Red y Eventos (UNA SOLA VEZ AQUÍ)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3. Lanzar componentes
    rssi_wifi_start(); // Configura WiFi
    mqtts_start();     // Configura MQTT (debe esperar internamente a que haya IP)
}
