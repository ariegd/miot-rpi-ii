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
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "rssi_wifi_comp.h"

static const char *TAG = "RSSI_ThingsBoard";

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando sistema RSSI_ThingsBoard...");
    rssi_wifi_start();
}
