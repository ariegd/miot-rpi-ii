| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

#  Práctica Final RPI-II

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

