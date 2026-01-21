/* MQTT over SSL Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_partition.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "certs.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include <sys/param.h>

#include "sdkconfig.h"

// Para el  actualizar el tiempo
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"

#include "cJSON.h" // <--- NECESARIO
#include "nvs.h"
#include "nvs_flash.h"

// Variables para manejar el estado
static char thingsboard_token[128] = {0};
static bool is_provisioning_mode = false;
static bool provisioning_finished = false;

// Credenciales de provisionamiento (Vienen del Kconfig)
#define TB_PROV_KEY     CONFIG_TB_PROVISION_KEY
#define TB_PROV_SECRET  CONFIG_TB_PROVISION_SECRET

static const char *TAG = "mqtts_example";

// Variable global para guardar el handle del cliente
static esp_mqtt_client_handle_t global_client = NULL;
const char *mqtt_cert_ptr = NULL;

//--------A. Variables Globales Nuevas --------
// Variable global para el intervalo (por defecto 5000 ms / 5 seg)
static int intervalo_envio = 5000;
//-------- FIN A. Variables Globales Nuevas --------

#if defined(CONFIG_BROKER_MOSQUITTO)
// OPCIÓN 1: Embebido directamente como texto en el código
#elif CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
static const uint8_t mqtt_custom_pem_start[]  = "-----BEGIN CERTIFICATE-----\n" CONFIG_BROKER_CERTIFICATE_OVERRIDE "\n-----END 
#else
extern const uint8_t mqtt_eclipseprojects_io_pem_start[]   asm("_binary_mqtt_eclipseprojects_io_pem_start");
extern const uint8_t mqtt_eclipseprojects_io_pem_end[]   asm("_binary_mqtt_eclipseprojects_io_pem_end");
#endif

//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//
static void send_binary(esp_mqtt_client_handle_t client)
{
    esp_partition_mmap_handle_t out_handle;
    const void *binary_address;
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &binary_address, &out_handle);
    // sending only the configured portion of the partition (if it's less than the partition size)
    int binary_size = MIN(CONFIG_BROKER_BIN_SIZE_TO_SEND, partition->size);
    int msg_id = esp_mqtt_client_publish(client, "/topic/binary", binary_address, binary_size, 0, 0);
    ESP_LOGI(TAG, "binary sent with msg_id=%d", msg_id);
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
 /*
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        //ThingsBoard no reconoce estos tópicos y desconecta al cliente al recibirlos.
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (strncmp(event->data, "send binary please", event->data_len) == 0) {
            ESP_LOGI(TAG, "Sending the binary");
            send_binary(client);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}
*/

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

//-------- B. Actualizar mqtt_event_handler --------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Conectado.");
        
        if (is_provisioning_mode) {
            // --- LÓGICA DE PROVISIONAMIENTO (Igual que tenías) ---
            ESP_LOGI(TAG, "PROVISIONING: Iniciando secuencia...");
            esp_mqtt_client_subscribe(client, "/provision/response", 1);
            
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "deviceName", "ESP32_Lab_Final");
            cJSON_AddStringToObject(root, "provisionDeviceKey", TB_PROV_KEY);
            cJSON_AddStringToObject(root, "provisionDeviceSecret", TB_PROV_SECRET);
            char *post_data = cJSON_PrintUnformatted(root);
            
            esp_mqtt_client_publish(client, "/provision/request", post_data, 0, 1, 0);
            free(post_data);
            cJSON_Delete(root);

        } else {
            // --- LÓGICA DE OPERACIÓN NORMAL ---
            ESP_LOGI(TAG, "OPERACIÓN: Listo para telemetría.");
            
            // 1. Suscribirse a cambios de atributos (para recibir intervalo_envio dinámicamente)
            esp_mqtt_client_subscribe(client, "v1/devices/me/attributes", 1);
            
            // 2. Solicitar los valores actuales al arrancar (por si cambiaron mientras estaba apagado)
            esp_mqtt_client_publish(client, "v1/devices/me/attributes/request/1", "{\"sharedKeys\":\"intervalo_envio\"}", 0, 1, 0);
        }
        break;

    case MQTT_EVENT_DATA:
        // --- RESPUESTA DE PROVISIONAMIENTO ---
        if (is_provisioning_mode && strncmp(event->topic, "/provision/response", event->topic_len) == 0) {
            // ... (Tu código de parseo de provisionamiento se mantiene igual aquí) ...
            // ... Parsear JSON, guardar token en NVS y reiniciar ...
            cJSON *json = cJSON_Parse(event->data);
            cJSON *status = cJSON_GetObjectItem(json, "status");
             if (cJSON_IsString(status) && (strcmp(status->valuestring, "SUCCESS") == 0)) {
                cJSON *creds = cJSON_GetObjectItem(json, "credentialsValue");
                save_token_to_nvs(creds->valuestring);
                esp_restart(); // Reinicio limpio
             }
             cJSON_Delete(json);
        }
        
        // --- RECEPCIÓN DE ATRIBUTOS DE CONFIGURACIÓN ---
        // Esto ocurre cuando cambias el valor en el Dashboard o al recibir la respuesta inicial
        else if (!is_provisioning_mode) {
            ESP_LOGI(TAG, "Datos recibidos en tópico: %.*s", event->topic_len, event->topic);
            
            cJSON *root = cJSON_Parse(event->data);
            if (root) {
                // A veces TB envía {"shared": {"intervalo_envio": 5000}} o directo {"intervalo_envio": 5000}
                // Buscamos "intervalo_envio" directamente o dentro de "shared"
                cJSON *intervalItem = cJSON_GetObjectItem(root, "intervalo_envio");
                
                if (!intervalItem) {
                    cJSON *shared = cJSON_GetObjectItem(root, "shared");
                    if (shared) intervalItem = cJSON_GetObjectItem(shared, "intervalo_envio");
                }

                if (cJSON_IsNumber(intervalItem)) {
                    intervalo_envio = intervalItem->valueint;
                    ESP_LOGW(TAG, "NUEVO CONFIG RECIBIDA: intervalo_envio = %d ms", intervalo_envio);
                }
                cJSON_Delete(root);
            }
        }
        break;

    default:
        break;
    }
}
//-------- FIN B. Actualizar mqtt_event_handler --------

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
    const char *uri_to_use = "mqtt://demo.thingsboard.io:1883";

    if (is_provisioning_mode) {
        ESP_LOGW(TAG, "MODO: PROVISIONAMIENTO AUTOMÁTICO");
        username_to_use = "provision"; // Usuario obligatorio para provisionar
    } else {
        ESP_LOGI(TAG, "MODO: OPERACIÓN NORMAL");
        username_to_use = thingsboard_token; // Usamos el token guardado
    }

    // Usamos el certificado de ThingsBoard definido en certs.c
    mqtt_cert_ptr = mqtt_cert_thingsboard_ptr;

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = uri_to_use,
            // CAMBIO 2: Eliminar o comentar la parte del certificado
            //.verification.certificate = mqtt_cert_ptr,
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

/*
static void mqtt_app_start(void)
{
// ASIGNACIÓN EN TIEMPO DE EJECUCIÓN:
    #if defined(CONFIG_BROKER_MOSQUITTO)
        mqtt_cert_ptr = mqtt_cert_mosquitto_ptr; // Esto ahora es legal y funciona
    #elif defined(CONFIG_BROKER_THINGSBOARD)
        mqtt_cert_ptr = mqtt_cert_thingsboard_ptr;
    #elif CONFIG_BROKER_CERTIFICATE_OVERRIDDEN == 1
        mqtt_cert_ptr = (const char *)mqtt_custom_pem_start;
    #else
        mqtt_cert_ptr = (const char *)mqtt_eclipseprojects_io_pem_start;
    #endif

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
    #if defined(CONFIG_BROKER_MOSQUITTO)
            .address.uri = "mqtts://test.mosquitto.org:8883",// Esto ahora es legal y funciona
    #elif defined(CONFIG_BROKER_THINGSBOARD)
            .address.uri ="mqtts://demo.thingsboard.io:8883",
    #else
            .address.uri = CONFIG_BROKER_URI,
    #endif
            .verification.skip_cert_common_name_check = true,             //<--es probable que el nombre del host no coincida.
            .verification.certificate = mqtt_cert_ptr
        },
    #if defined(CONFIG_BROKER_THINGSBOARD)
        .credentials = {
              //.username = CONFIG_THINGSBOARD_ACCESS_TOKEN, // <--- OBLIGATORIO
              .username = "odpvh2wr539x57lmevg7", // El token de tu imagen
          },
    #endif
        .network.timeout_ms = 10000, // <-- redes ruidosas
    };

    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    //esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    // esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    // esp_mqtt_client_start(client);
    
    global_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(global_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(global_client);
}
*/

// Esta es la función que correrá en paralelo
void mqtt_fix_task(void* pvParameters) {
    ESP_LOGI("FIX", "Esperando a que el servidor local esté accesible...");
    vTaskDelay(pdMS_TO_TICKS(5000)); 
    
    ESP_LOGI("FIX", "Iniciando conexión MQTT Local sobre SSL...");
    mqtt_app_start();
    
    vTaskDelete(NULL);
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

//-------- C. Tarea de Envío de Telemetría (mqtts_task) --------
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
//-------- FIN C. Tarea de Envío de Telemetría (mqtts_task) --------

void mqtts_start(void)
{
    xTaskCreate(&mqtts_task, "mqtts_task", 4096, NULL, 1, NULL);
}
