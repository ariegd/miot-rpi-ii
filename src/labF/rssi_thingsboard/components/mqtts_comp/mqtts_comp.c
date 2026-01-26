#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>
#include "esp_mac.h"
#include "sdkconfig.h"

// Para el  actualizar el tiempo
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"

#include "cJSON.h" // <--- NECESARIO
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_crt_bundle.h" // <--- IMPORTANTE: Necesario para esp_crt_bundle_attach

// Variables para manejar el estado
static char thingsboard_token[128] = {0};
static bool is_provisioning_mode = false;

// Credenciales de provisionamiento (Vienen del Kconfig)
#define TB_PROV_KEY     CONFIG_TB_PROVISION_KEY
#define TB_PROV_SECRET  CONFIG_TB_PROVISION_SECRET

static const char *TAG = "mqtts_example";

// Variable global para guardar el handle del cliente
static esp_mqtt_client_handle_t global_client = NULL;

// Variable global para el intervalo (por defecto 5000 ms / 5 seg)
static int intervalo_envio = 5000;

static esp_err_t save_token_to_nvs(const char *token) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(my_handle, "tb_token", token);
    if (err == ESP_OK) err = nvs_commit(my_handle);
    nvs_close(my_handle);
    return err;
}

static esp_err_t load_token_from_nvs(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) return err;

    size_t required_size = sizeof(thingsboard_token);
    err = nvs_get_str(my_handle, "tb_token", thingsboard_token, &required_size);
    nvs_close(my_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Token cargado de NVS: %s", thingsboard_token);
    } else {
        ESP_LOGW(TAG, "No se encontró token en NVS. Se requiere provisionamiento.");
        memset(thingsboard_token, 0, sizeof(thingsboard_token));
    }
    return err;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Conectado.");
            
            if (is_provisioning_mode) {
                ESP_LOGI(TAG, "PROVISIONING: Iniciando secuencia...");
                esp_mqtt_client_subscribe(client, "/provision/response", 1);
                
                // --- CAMBIO: GENERAR NOMBRE BASADO EN MAC ---
                uint8_t mac[6] = {0};
                esp_efuse_mac_get_default(mac); // Obtiene la MAC base del chip
                char dynamic_name[32];
                sprintf(dynamic_name, "ESP32_%02X%02X%02X", mac[3], mac[4], mac[5]); // Ej: ESP32_A1B2C3
                // --------------------------------------------
                
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "deviceName", dynamic_name);
                cJSON_AddStringToObject(root, "provisionDeviceKey", TB_PROV_KEY);
                cJSON_AddStringToObject(root, "provisionDeviceSecret", TB_PROV_SECRET);
                char *post_data = cJSON_PrintUnformatted(root);
                
                esp_mqtt_client_publish(client, "/provision/request", post_data, 0, 1, 0);
                free(post_data);
                cJSON_Delete(root);
            } else {
                ESP_LOGI(TAG, "OPERACIÓN: Listo para telemetría.");
                // SUSCRIPCIÓN CRÍTICA: Para recibir cambios del Dashboard en tiempo real
                esp_mqtt_client_subscribe(client, "v1/devices/me/attributes", 1);
                // SOLICITUD INICIAL: Para leer el valor actual al arrancar
                esp_mqtt_client_publish(client, "v1/devices/me/attributes/request/1", "{\"sharedKeys\":\"intervalo_envio\"}", 0, 1, 0);
            }
            break;

        case MQTT_EVENT_DATA:
            if (is_provisioning_mode && strncmp(event->topic, "/provision/response", event->topic_len) == 0) {
                cJSON *json = cJSON_Parse(event->data);
                cJSON *status = cJSON_GetObjectItem(json, "status");
                if (cJSON_IsString(status) && (strcmp(status->valuestring, "SUCCESS") == 0)) {
                    cJSON *creds = cJSON_GetObjectItem(json, "credentialsValue");
                    save_token_to_nvs(creds->valuestring);
                    esp_restart(); 
                }
                cJSON_Delete(json);
            } 
            else if (!is_provisioning_mode) {
                ESP_LOGI(TAG, "Datos recibidos en tópico: %.*s", event->topic_len, event->topic);
                
                // 1. CORRECCIÓN DE SEGURIDAD: Crear un buffer con terminación NULL
                char *json_string = (char *)malloc(event->data_len + 1);
                if (json_string == NULL) {
                    ESP_LOGE(TAG, "Fallo al asignar memoria para JSON");
                    break;
                }
                memcpy(json_string, event->data, event->data_len);
                json_string[event->data_len] = '\0'; // Asegurar terminación

                // 2. Parsear el string seguro
                cJSON *root = cJSON_Parse(json_string);
                if (root) {
                    cJSON *intervalItem = NULL;

                    // 3. Lógica robusta: Buscar "intervalo_envio" donde sea que esté
                    // Intento A: Actualización directa (Push desde widget Shared Attribute)
                    intervalItem = cJSON_GetObjectItem(root, "intervalo_envio");
                    
                    // Intento B: Respuesta a request (dentro de "shared")
                    if (!intervalItem) {
                        cJSON *shared = cJSON_GetObjectItem(root, "shared");
                        if (shared) {
                            intervalItem = cJSON_GetObjectItem(shared, "intervalo_envio");
                        }
                    }

                    // 4. Validar y aplicar
                    if (intervalItem && cJSON_IsNumber(intervalItem)) {
                        intervalo_envio = intervalItem->valueint;
                        ESP_LOGW(TAG, "--> ¡CONFIGURACIÓN ACTUALIZADA! Nuevo intervalo: %d ms", intervalo_envio);
                        
                        // Opcional: Forzar un envío inmediato para confirmar
                        // xTaskNotifyGive(mi_handle_de_tarea); 
                    } else {
                        // Log para depuración: Ver qué llegó realmente si no lo entendimos
                        ESP_LOGD(TAG, "JSON recibido no contiene 'intervalo_envio' o formato incorrecto: %s", json_string);
                    }
                    cJSON_Delete(root);
                } else {
                    ESP_LOGE(TAG, "Error parseando JSON");
                }
                
                free(json_string); // IMPORTANTE: Liberar memoria
            }
            break;

        default:
            break;
    }
}

static void mqtt_app_start(void)
{
    // 1. Intentar cargar token
    if (load_token_from_nvs() == ESP_OK) {
        is_provisioning_mode = false;
    } else {
        is_provisioning_mode = true;
    }

    const char *username_to_use;
    //const char *uri_to_use = "mqtts://demo.thingsboard.io:8883";
    //const char *uri_to_use = "mqtt://demo.thingsboard.io:1883";
    const char *uri_to_use = "mqtts://mqtt.eu.thingsboard.cloud:8883";

    if (is_provisioning_mode) {
        ESP_LOGW(TAG, "MODO: PROVISIONAMIENTO AUTOMÁTICO");
        username_to_use = "provision"; // Usuario obligatorio para provisionar
    } else {
        ESP_LOGI(TAG, "MODO: OPERACIÓN NORMAL");
        username_to_use = thingsboard_token; // Usamos el token guardado
    }

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = uri_to_use,
            .address.hostname = "mqtt.eu.thingsboard.cloud",
            // CAMBIO 2: Eliminar o comentar la parte del certificado
            //.verification.certificate = mqtt_cert_ptr,
            .verification.crt_bundle_attach = esp_crt_bundle_attach,
            .verification.skip_cert_common_name_check = true,
        },
        .credentials = {
            .username = username_to_use, // "provision" o el Token real
        },
    };

    global_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(global_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(global_client);
}

// NUEVA FUNCIÓN: Para que el WiFi la llame
void mqtt_enviar_telemetria(const char *topic, const char *data) {
    if (global_client != NULL) {
        int msg_id = esp_mqtt_client_publish(global_client, topic, data, 0, 1, 0);
        ESP_LOGI(TAG, "Enviando a ThingsBoard, msg_id=%d", msg_id);
    } else {
        ESP_LOGW(TAG, "MQTT no inicializado todavía.");
    }
}

static void obtener_hora_sntp(void)
{
    ESP_LOGI(TAG, "Inicializando SNTP...");
    
    // Usar esp_sntp_... para compatibilidad con versiones modernas de IDF
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    
    // Servidores NTP
    esp_sntp_setservername(0, "time.google.com"); 
    esp_sntp_setservername(1, "pool.ntp.org");
    
    esp_sntp_init();

    // Esperar a que la hora se sincronice
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Esperando respuesta NTP... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "ERROR: La hora sigue siendo 1970. Revisa la conexión UDP/NTP.");
    } else {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "Hora sincronizada correctamente: %s", strftime_buf);
    }
}

void mqtts_task(void *pvParameters)
{
    // 1. Sincronizar hora
    ESP_LOGI(TAG, "----------------- Sincronizando Reloj ---------------------");
    obtener_hora_sntp();

    // 2. Iniciar Cliente MQTT (Provisionamiento o Conexión Normal)
    ESP_LOGI(TAG, "----------------- Iniciando MQTT ---------------------");
    mqtt_app_start();

    // 3. Bucle infinito de telemetría (Solo si NO estamos provisionando)
    while (1) {
        // Si estamos conectados y NO estamos en modo provisionamiento
        if (!is_provisioning_mode && global_client != NULL) {
            
            // Simulación de dato (aquí leerías tu RSSI real)
            int rssi_dummy = -50 - (esp_random() % 20); 
            
            // Crear JSON
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "rssi", rssi_dummy);
            cJSON_AddNumberToObject(root, "intervalo_actual", intervalo_envio); // Para verificar en TB
            char *json_str = cJSON_PrintUnformatted(root);

            // Usar tu función para enviar
            mqtt_enviar_telemetria("v1/devices/me/telemetry", json_str);

            free(json_str); // Importante liberar memoria
            cJSON_Delete(root);
        }

        // Esperar según el intervalo configurado dinámicamente
        // Mínimo 1 segundo para evitar saturar si el config llega mal
        int espera = (intervalo_envio < 1000) ? 1000 : intervalo_envio;
        vTaskDelay(pdMS_TO_TICKS(espera));
    }
    
    vTaskDelete(NULL);
}

void mqtts_start(void)
{
    xTaskCreate(&mqtts_task, "mqtts_task", 4096, NULL, 1, NULL);
}
