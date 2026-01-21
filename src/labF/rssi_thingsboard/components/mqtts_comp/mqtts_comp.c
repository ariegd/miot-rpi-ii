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

//-------- 1. Cabeceras y Variables Globales --------
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
//-------- FIN 1. Cabeceras y Variables Globales --------

static const char *TAG = "mqtts_example";

// Variable global para guardar el handle del cliente
static esp_mqtt_client_handle_t global_client = NULL;
const char *mqtt_cert_ptr = NULL;

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

//-------- 2. Funciones de NVS (Guardar/Leer Token) --------
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
//-------- FIN 2. Funciones de NVS (Guardar/Leer Token) --------

//-------- 3. El nuevo mqtt_event_handler inteligente --------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Conectado.");
        
        if (is_provisioning_mode) {
            ESP_LOGI(TAG, "Iniciando secuencia de provisionamiento...");
            
            // 1. Suscribirse a la respuesta
            esp_mqtt_client_subscribe(client, "/provision/response", 1);

            // 2. Crear JSON de petición
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "deviceName", "ESP32_Auto_Gen"); // Opcional, o dejar que TB genere uno
            cJSON_AddStringToObject(root, "provisionDeviceKey", TB_PROV_KEY);
            cJSON_AddStringToObject(root, "provisionDeviceSecret", TB_PROV_SECRET);
            
            char *post_data = cJSON_PrintUnformatted(root);
            
            // 3. Publicar petición
            ESP_LOGI(TAG, "Enviando credenciales: %s", post_data);
            esp_mqtt_client_publish(client, "/provision/request", post_data, 0, 1, 0);
            
            free(post_data);
            cJSON_Delete(root);
        } else {
            ESP_LOGI(TAG, "Conexión normal establecida. Listo para enviar telemetría.");
            // Aquí te puedes suscribir a RPC o Atributos si quieres
        }
        break;

    case MQTT_EVENT_DATA:
        // Si recibimos datos en el tópico de respuesta de provisionamiento
        if (is_provisioning_mode && strncmp(event->topic, "/provision/response", event->topic_len) == 0) {
            ESP_LOGI(TAG, "Respuesta de provisionamiento recibida.");
            
            // Parsear JSON
            cJSON *json = cJSON_Parse(event->data);
            cJSON *status = cJSON_GetObjectItem(json, "status");
            
            if (cJSON_IsString(status) && (strcmp(status->valuestring, "SUCCESS") == 0)) {
                cJSON *creds = cJSON_GetObjectItem(json, "credentialsValue");
                if (cJSON_IsString(creds)) {
                    ESP_LOGI(TAG, "¡Provisionamiento EXITOSO! Token: %s", creds->valuestring);
                    
                    // Guardar en NVS
                    save_token_to_nvs(creds->valuestring);
                    strcpy(thingsboard_token, creds->valuestring);
                    
                    // Marcar flag para reiniciar cliente
                    provisioning_finished = true; 
                    
                    // Desconectar para reconectar con el nuevo token
                    esp_mqtt_client_disconnect(client); 
                }
            } else {
                ESP_LOGE(TAG, "Fallo en provisionamiento: %s", event->data);
            }
            cJSON_Delete(json);
        }
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Desconectado.");
        // Si acabamos de terminar el provisionamiento, reiniciamos el cliente inmediatamente
        if (provisioning_finished) {
            provisioning_finished = false;
            is_provisioning_mode = false;
            // No llamamos a mqtt_app_start aquí recursivamente, 
            // mejor dejar que el loop principal o una tarea maneje el reinicio,
            // pero para este ejemplo, reconfiguraremos abajo.
             esp_restart(); // La forma más limpia tras provisionar es un reinicio completo
        }
        break;

    default:
        break;
    }
}
//-------- FIN 3. El nuevo mqtt_event_handler inteligente --------

//-------- 4. La función mqtt_app_start definitiva --------
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
//-------- FIN 4. La función mqtt_app_start definitiva --------

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

void mqtts_task(void *pvParameters)
{
    ESP_LOGI(TAG, "----------------- Sincronizando Reloj ---------------------");
    obtener_hora_sntp(); // <--- Paso obligatorio para MQTTS

    ESP_LOGI(TAG, "----------------- Iniciando MQTT ---------------------");
    mqtt_app_start();
    
     vTaskDelete(NULL);
}

void mqtts_start(void)
{
    xTaskCreate(&mqtts_task, "mqtts_task", 4096, NULL, 1, NULL);
}
