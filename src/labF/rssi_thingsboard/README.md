| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

#  Práctica Final RPI-II

## Como asegurar que el ESP32 capture correctamente los cambios realizados desde el widget de Update Multiple Attributes en ThingsBoard
### Paso 1. Cambiar el Ámbito (Scope) en el Widget
Basado en tu imagen, sigue esta ruta:
1. En la pestaña Datos, haz clic en el icono del lápiz (editar) que aparece al lado de la clave `intervalo_envio`.
2. Se abrirá una ventana emergente llamada "Configuración de la clave de datos".
3. Busca la sección Configuración del widget o Ajustes avanzados.
4. Allí encontrarás el menú desplegable Attribute scope (Ámbito del atributo). Cámbialo de "Server Attribute" a "Shared Attribute".
5. Haz clic en Aplicar y luego en Guardar en el tablero principal.

### Paso 2. Por qué es obligatorio usar "Shared Attribute"
El código de tu ESP32 está suscrito al tópico `v1/devices/me/attributes`. En el ecosistema de ThingsBoard:
* **Server Attributes**: Son privados para el servidor. El ESP32 no recibe notificaciones cuando cambian.
* **Shared Attributes**: Están diseñados para ser compartidos con el dispositivo. Cualquier cambio en estos activará un mensaje MQTT que tu mqtt_event_handler podrá capturar.


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

