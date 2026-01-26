/* MQTT over SSL para ThingsBoard - ARQUITECTURA PARALELA (NON-BLOCKING) */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_crt_bundle.h"

static const char *TAG = "TB_MQTTS";

// Flag global para saber si tenemos hora (simple y efectivo)
static bool time_synced = false;

// ------------------------------------------------------------------------------------------
// CERTIFICADO ROOT CA (USERTrust RSA Certification Authority)
// ------------------------------------------------------------------------------------------
static const char *thingsboard_root_ca = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIIF3jCCA8agAwIBAgIQAf1tMPyjyl56S549f8LLrDANBgkqhkiG9w0BAQwFADCB\n"
    "iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl\n"
    "cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRydXN0IE5ldHdvcmsxLjAsBgNV\n"
    "BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw\n"
    "MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV\n"
    "BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU\n"
    "aGUgVVNFUlRydXN0IE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy\n"
    "dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK\n"
    "AoICAQCAEmUXNg7D2wqWEInuxkhkyFsRrfvh6F+uLEl2tsEUB8YAf2j5y4mkqaUD\n"
    "Lb9ivJ6yGtOHOd8BSpPtW0SD83DbQAxdJLhcQ0hqFXAnBEjbV0t8Pq079kMh91ZK\n"
    "3hM9ogVGfJ1SVsoX6f3HVOV0H+IB5gLWh2LOWjUZdmSVq4gNBspqJffulPjNHcM9\n"
    "16tEVRKnhyGWg70w+uRcdzD0OZ2bdMm1svZLH8jqa+ncqkmkGLi4vrXpgVGp2l/P\n"
    "f180MEKxuk962YkNl1kU0uefQf1HkVDyB1tZOAdfiUBSvG+APL9jK7aWScUw9hLp\n"
    "3f02x84sN6rW9/X+d80T8L0ExkZ/e5reII6Nf8c2+0d24O9XFNxM04kqjoK24X8M\n"
    "36cpwhOT5k845yK8pxljy2us1LeE0UASKGk83e3FXj83WnFw9jI96i8yL1gE3uV4\n"
    "I5X1t01pLvl7k42jGeP9xOqOQ11V5y4E585TAa/9e3fA9S2S6c4W1U5tY731F2hW\n"
    "z+i933+P9f5Q1g51AW0mS2S9O7F4QL86yKxoMNMHkSSXl9s6T9aM6r8S/cSMY93p\n"
    "+F7g01q/3j5k0fDGe5Sua6r2O4e0y3Y88A/i87t63j4oF0EVwwIDAQABo0IwQDAd\n"
    "BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZ80wDgYDVR0PAQH/BAQDAgEGMA8G\n"
    "A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBACcJuyJ2G/wHVEkZ52i5\n"
    "kn8/q8m1f+Y2/reFT70eX378h6D92/r87pE95m6h4S0XIAwtqW6i8a2J6qj9v/1e\n"
    "I6F45o2t99/I08r9d6Xq5wSIVs8qS9L2gI7355BGlj2RF05r8l8oS8Fys89E25Lq\n"
    "TV5fL2eX2jJ4qW5FvK5W8f7m+1o7yBSvL64F/2uCO90wqq1tLG06szQkYJ7xR48Z\n"
    "m52F878y8l5+u4W6o84MApD47J57C5r5p+L4d56xU767d8S4Ius6siW7DGLz8w8o\n"
    "28WJSl9/o9YF1rrbIvW7h7F5Yc94y71FCjFtT6eMUAy+E78Z765S4J8xD/Nql/8a\n"
    "c8aJdTV3v52M2p/C7d5z90n05z5q5r5m5r5m5r5m5r5m5r5m5r5m5r5m5r5m5r5m\n"
    "5r5m\n"  // Limpiado de caracteres basura
    "-----END CERTIFICATE-----";

// ------------------------------------------------------------------------------------------
// TAREA SNTP (Corre en paralelo)
// ------------------------------------------------------------------------------------------
void sntp_task(void *pvParameters)
{
    ESP_LOGI(TAG, "[SNTP Task] Iniciando... Esperando IP...");
    // Esperar un poco a que el WiFi arranque
    vTaskDelay(pdMS_TO_TICKS(5000));

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    
    while (1) {
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year >= (2025 - 1900)) {
            time_synced = true;
            ESP_LOGI(TAG, "[SNTP Task] >>> HORA SINCRONIZADA: %s", asctime(&timeinfo));
            vTaskDelete(NULL); // Misión cumplida, eliminamos la tarea
        } else {
            ESP_LOGW(TAG, "[SNTP Task] Esperando hora... (Año actual detectado: %d)", timeinfo.tm_year + 1900);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

// ------------------------------------------------------------------------------------------
// CLIENTE MQTT
// ------------------------------------------------------------------------------------------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "[MQTT] >>> CONECTADO EXITOSAMENTE! <<<");
        esp_mqtt_client_publish(client, "v1/devices/me/telemetry", "{\"status\":\"online\"}", 0, 1, 0);
        break;
    case MQTT_EVENT_ERROR:
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "[MQTT] Error SSL: 0x%x. (TimeSynced: %d)", 
                     event->error_handle->esp_tls_last_esp_err, time_synced);
        }
        break;
    default:
        break;
    }
}

void mqtt_task(void *pvParameters)
{
    ESP_LOGI(TAG, "[MQTT Task] Esperando sincronizacion de hora...");
    
    // 1. Esperar a que SNTP tenga la hora correcta
    while (!time_synced) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        static int counter = 0;
        if (counter++ % 5 == 0) ESP_LOGI(TAG, "[MQTT Task] Aun esperando NTP...");
    }

    //ESP_LOGI(TAG, "[MQTT Task] Hora OK. Conectando con Certificate Bundle...");

    // 2. Configuración usando el Bundle Automático (Sin certificado manual)
    const esp_mqtt_client_config_t mqtt_cfg = {
        // CAMBIO: Apuntar a la instancia Cloud de EU
        .broker.address.uri = "mqtts://mqtt.eu.thingsboard.cloud:8883", 
        .broker.address.hostname = "mqtt.eu.thingsboard.cloud",
        
        // El bundle automático suele funcionar bien con ThingsBoard Cloud
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        
        // Credenciales del nuevo dispositivo en eu.thingsboard.cloud
        .credentials.client_id = "esp32_tb_node_01",
        .credentials.username = "4q52trlilp9caup5tnh7",
        .credentials.authentication.password = "",
        
        .buffer.size = 2048,
        .buffer.out_size = 2048
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    
    // La tarea termina y se auto-elimina, pero el cliente MQTT sigue corriendo en background
    vTaskDelete(NULL);
}

// ------------------------------------------------------------------------------------------
// MAIN
// ------------------------------------------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "------------------------------------------------");
    ESP_LOGI(TAG, "[APP] INICIO DEL SISTEMA v3.0");
    ESP_LOGI(TAG, "------------------------------------------------");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 1. CREAR LAS TAREAS ANTES DE CONECTAR
    // Esto garantiza que existan en memoria antes de que el WiFi intente bloquear algo
    xTaskCreate(sntp_task, "sntp_task", 4096, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 5120, NULL, 5, NULL);

    ESP_LOGI(TAG, "[APP] Tareas creadas. Intentando conectar WiFi...");

    // 2. CONECTAR WIFI (Al final)
    // Usamos connect, pero como las tareas ya corren, veremos logs de "Esperando..."
    ESP_ERROR_CHECK(example_connect());
    
    ESP_LOGI(TAG, "[APP] WiFi Conectado (IP obtenida).");
}
