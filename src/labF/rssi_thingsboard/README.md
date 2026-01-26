| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

#  Práctica Final RPI-II

## Error: app partition is too small
* El error `Error: app partition is too small` indica que el binario compilado (0x103530 bytes) es ligeramente más grande que el espacio asignado en la tabla de particiones por defecto para la aplicación (0x100000 bytes o 1 MB). Esto es común al habilitar SSL/TLS y usar librerías como cJSON, ya que aumentan el tamaño del firmware.

* Crea un archivo llamado `partitions.csv` en la raíz de tu proyecto.

## Conectarse a eu.thingsboard.cloud
¡Excelente cambio de rumbo! Conectarse a eu.thingsboard.cloud es una decisión inteligente, ya que la instancia Cloud suele ser más estable que la de demo. Ahora si funciona el MQTTS.
```
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
```

## Error limpiar los nodos en el ThingsBoard
Para volver aprovisionar
```
I (28539) mqtts_example: Hora sincronizada correctamente: Fri Jan 23 22:19:32 2026
I (28539) mqtts_example: ----------------- Iniciando MQTT ---------------------
W (28539) mqtts_example: MODO: PROVISIONAMIENTO AUTOMÁTICO
I (29079) mqtts_example: MQTT Conectado.
I (29079) mqtts_example: PROVISIONING: Iniciando secuencia...
E (89499) mqtt_client: esp_mqtt_handle_transport_read_error: transport_read(): EOF
E (89499) mqtt_client: esp_mqtt_handle_transport_read_error: transport_read() error: errno=128
E (89509) mqtt_client: mqtt_process_receive: mqtt_message_receive() returned -2
I (105059) mqtts_example: MQTT Conectado.
I (105059) mqtts_example: PROVISIONING: Iniciando secuencia...
E (165689) mqtt_client: esp_mqtt_handle_transport_read_error: transport_read(): EOF
E (165689) mqtt_client: esp_mqtt_handle_transport_read_error: transport_read() error: errno=128
E (165689) mqtt_client: mqtt_process_receive: mqtt_message_receive() returned -2
I (181249) mqtts_example: MQTT Conectado.
```

## Para cumplir con los dos nuevos requisitos (Telemetría periódica y Atributos compartidos para configuración remota)
Si no ves la palabra "Grouping" directamente en la configuración de la clave de datos, es porque en esta versión la agregación se define en la Ventana de Tiempo (Time Window) del widget o del tablero completo.

Sigue estos pasos para activar la media (promedio) en tu gráfico: Configuración desde la Ventana de Tiempo
1. Haz clic en el icono del reloj (Tiempo real) en la esquina superior derecha de tu widget o del tablero tab_airus.
2. Asegúrate de que la pestaña seleccionada sea Tiempo real.
3. Busca el desplegable que dice Función de agregación (suele estar justo debajo del intervalo de tiempo).
4. Selecciona Promedio (o Average).
5. En Intervalo de agregación, define cada cuánto tiempo quieres que se calcule ese promedio (ej. cada 10 segundos).

## Comparar gráficamente tus cuatro nodos (el ESP32-C3, el ESP32-C6 y los otros dos que añadas mediante la MAC única)
En un solo widget de ThingsBoard, debes utilizar un `Timeseries Line Chart` configurado con múltiples fuentes de datos.
### Paso 1. Preparar los Alias de Entidad
Para que un solo gráfico entienda que debe buscar datos de distintos dispositivos, primero debemos "agruparlos" mediante un alias:
1. En tu Dashboard (`tab_airus`), entra en Modo de edición.
2. Haz clic en el icono de Entity Aliases (el icono de filtro/lista en la barra superior).
3. Puedes hacerlo de dos formas:
  - **Alias Individuales**: Crea uno para cada nodo (ej. Nodo_C3, Nodo_C6).
  - **Alias de Filtro**: Crea uno llamado `Mis_Nodos`, selecciona el tipo "Entity List" y busca manualmente los nombres de tus 4 dispositivos (los que tienen el formato `ESP32_XXXXXX`). 
### Paso 2. Configurar el Widget de Comparación
1. Haz clic en Añadir nuevo `widget > Paquete Charts > Timeseries Line Chart`.
2. En la pestaña Data (Datos), añade una nueva serie por cada nodo:
 - **Datasource 1**: Selecciona el alias del primer dispositivo y en Data Key elige rssi.
 - **Datasource 2**: Selecciona el alias del segundo dispositivo y elige la misma clave rssi.

## ThingsBoard guarda automáticamente todo el historial de los datos que envías como telemetría.
Cómo ver el historial en la plataforma. Desde la pestaña "Última telemetría"
1. Ve a Entidades > Dispositivos.
2. Haz clic en tu dispositivo (ej. ESP32_Auto_Gen o el nombre con MAC que creamos).
3. Selecciona la pestaña Última telemetría.
4. Ahí verás una lista con rssi e `intervalo_actual`. Si haces clic en el icono de "Historial" (parece un pequeño gráfico) al lado del valor, podrás ver una tabla con todos los datos pasados.

## Borrado los dispositivos en demo.thingsboard.io
El error `Connection refused, not authorized` ocurre porque has borrado los dispositivos en la plataforma, pero tus ESP32 todavía tienen guardados los Tokens antiguos en su memoria interna (NVS).
* Al intentar conectar con un Token que ya no existe en el servidor, ThingsBoard rechaza la conexión inmediatamente.
* **Solución**: Limpiar la memoria NVS (Obligatorio): 
- Vía comandos (Recomendado): Ejecuta en tu terminal para cada dispositivo: `idf.py -p PUERTO erase-flash`

## Problema en una colisión de identidad causada por el código fuente en mqtts_comp.c.
El primero (C3) "ganó" el nombre ESP32_Auto_Gen. ThingsBoard permite el provisionamiento, pero si la política es "permitir crear nuevos dispositivos", el segundo falla al intentar reclamar un nombre que ya está en uso y conectado. Con el cambio de la MAC, cada nodo tendrá su propia identidad única automáticamente.

## Como asegurar que el ESP32 capture correctamente los cambios realizados desde el widget de Update Multiple Attributes en ThingsBoard
### Paso 1. Cambiar el Ámbito (Scope) en el Widget
Basado en tu imagen, sigue esta ruta:
1. En la pestaña Datos, haz clic en el icono del lápiz (editar) que aparece al lado de la clave `intervalo_envio`.
2. Se abrirá una ventana emergente llamada "Configuración de la clave de datos".
3. Busca la sección Configuración del widget o Ajustes avanzados.
4. Allí encontrarás el menú desplegable `Attribute scope` (Ámbito del atributo). Cámbialo de "Server Attribute" a "Shared Attribute".
5. Haz clic en Aplicar y luego en Guardar en el tablero principal.
```
Dispositivos
Atributos->Atributos del clientes-Atributos Compartidos->

Cards
Update Multiple Attributes-> Editar intervalo_envio->Avanzado-> Atributo Compartido-Entero-> + ->intervalo_envio
```

### Paso 2. Por qué es obligatorio usar "Shared Attribute"
El código de tu ESP32 está suscrito al tópico `v1/devices/me/attributes`. En el ecosistema de ThingsBoard:
* **Server Attributes**: Son privados para el servidor. El ESP32 no recibe notificaciones cuando cambian.
* **Shared Attributes**: Están diseñados para ser compartidos con el dispositivo. Cualquier cambio en estos activará un mensaje MQTT que tu `mqtt_event_handler` podrá capturar.
* Configuración en el Panel de ThingsBoard. Para que esto funcione, debes configurar el dashboard para enviar el atributo:

1. Entra a tu Dispositivo: Ve a la pestaña Attributes (Atributos) y selecciona Shared Attributes (Atributos compartidos).
2. Crear el Atributo: Añade un nuevo atributo llamado `intervalo_envio` (Integer) con valor `10000` (10 segundos).
3. Crear Widget en Dashboard:
 - Crea un nuevo Dashboard.
 - Añade un widget de tipo "Input" o "Knob Control" (Control de perilla).
 - En "Datasource", selecciona tu dispositivo.

En "Datakey", selecciona el atributo intervalo_envio (asegúrate de marcarlo como "Shared Attribute", no Telemetry).

## ¿Por qué no se actualizaba antes?
Existen tres razones comunes basadas en tu log y el código previo:
1. **Estructura del JSON**: Al solicitar el atributo con `attributes/request/1`, ThingsBoard responde con `{"shared": {"intervalo_envio": 10000}}`. Sin embargo, cuando cambias el widget y pulsas "Guardar", ThingsBoard envía directamente {"intervalo_envio": 15000}. El código ahora maneja ambos casos.
2. **Tipo de Atributo**: Asegúrate de que en el widget de ThingsBoard (el de la imagen image_b8bc98.png) la clave `intervalo_envio` esté configurada como Atributo Compartido (Shared Attribute). Si se guarda como "Server Attribute", el ESP32 nunca recibirá la notificación automática.
3. **Bucle de Tarea**: Verifica que en tu `mqtts_task`, el `vTaskDelay` use la variable `intervalo_envio`.

## Telemetría periódica y Atributos compartidos para configuración remota
Resumen del Flujo
1. Arranque: El ESP32 se conecta. Si ya tiene token, se suscribe a `v1/devices/me/attributes`.
2. Petición Inicial: El ESP32 pregunta "¿Qué configuración tengo?". ThingsBoard responde con el valor guardado (ej. 5000).
3. Bucle: El `while(1)` envía datos cada 5000ms.
4. Cambio Dinámico: Tú cambias el widget en ThingsBoard a "2000".
5. Recepción: El `mqtt_event_handler` detecta el JSON, actualiza la variable `intervalo_envio` a 2000.
6. Efecto: El `vTaskDelay` del bucle ahora dura solo 2 segundos. ¡El dispositivo acelera el envío sin reiniciarse!

## Para lograr el provisionamiento automático (Device Provisioning) en ESP-IDF
Esta es una implementación completa y profesional. Para lograr el provisionamiento automático (Device Provisioning) en ESP-IDF tal como lo hace el script de Python, necesitamos implementar una máquina de estados sencilla:
1. **Estado 1 (Inicio)**: Verificar en la memoria no volátil (NVS) si ya tenemos un Token guardado.
2. **Estado 2 (Provisionamiento)**: Si no hay Token, conectarse como usuario "provision", enviar las claves (Key/Secret), recibir el Token y guardarlo en NVS.
3. **Estado 3 (Operación)**: Si ya hay Token (o acabamos de recibirlo), conectarse normalmente para enviar telemetría.

## Instalación Mosquitto
1. Actualiza los repositorios:
```
bash
sudo apt update
```

2. Instala Mosquitto y sus clientes: El paquete mosquitto-clients incluye las herramientas `mosquitto_sub` y `mosquitto_pub` para pruebas.
```
bash
sudo apt install mosquitto mosquitto-clients
```

### Verificación y funcionamiento
**Paso 1**. Comprueba el estado del servicio:
```
bash
sudo systemctl status mosquitto
```

Debería mostrar active (running). Si no está activo, inicia y habilita el arranque automático:
```
bash
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
```

**Paso 2**. Prueba básica (dos terminales):
1. Terminal 1 (Suscriptor): Abre una terminal y suscríbete a un tema (por ejemplo, test/topic).
```
bash
mosquitto_sub -h localhost -t "test/topic"
```

2. Terminal 2 (Publicador): En otra terminal, publica un mensaje en el mismo tema.
```
bash
mosquitto_pub -h localhost -t "test/topic" -m "¡Hola desde Mosquitto!"
```

3. Verificación: Verás el mensaje "¡Hola desde Mosquitto!" aparecer en la Terminal 1, confirmando que el broker funciona y reenvía mensajes. 

