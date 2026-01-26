| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

#  Práctica Final RPI-II
```
Máster IoT, curso 25-26
 	└── Autor
 		    └── Ariel Gámez <arielg01@ucm.es>
```
[repositorio](https://github.com/ariegd/miot-rpi-ii/tree/labF/src/labF) en GitHub
[video](https://youtu.be/IxZo4qbLnYY) en Youtube

## Objetivos
```
■  Seleccionar y procesar una estadística proporcionada por el sistema (ej. RSSI u otra disponible en ESP-IDF).
■  Serializar los datos usando un formato eficiente (JSON, CBOR o PBUF).
■  Transmitir datos cifrados empleando MQTT (MQTTS) o CoAP/LwM2M con DTLS.
■  Integrar la solución con ThingsBoard para telemetría, control de parámetros y visualización.
■  Permitir la actualización remota del periodo de envío desde ThingsBoard.
■  Representar la estadística de un conjunto de 4 nodos a lo largo del tiempo.
```

## Directorio del proyecto
A continuación se muestra una explicación de los archivos en la carpeta del proyecto.
```
├── CMakeLists.txt
├── components
│   ├── mqtts_comp                          <-- componente
│   │   ├── CMakeLists.txt
│   │   ├── idf_component.yml
│   │   ├── include
│   │   │   └── mqtts_comp.h
│   │   ├── Kconfig.projbuild
│   │   └── mqtts_comp.c
│   └── rssi_wifi_comp                   <-- componente
│       ├── CMakeLists.txt
│       ├── include
│       │   └── rssi_wifi_comp.h
│       ├── Kconfig.projbuild
│       └── rssi_wifi_comp.c
├── dependencies.lock
├── main
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── rssi_thingsboard.c
├── pytest_hello_world.py
├── README.md
├── sdkconfig
├── sdkconfig.ci
└── sdkconfig.old
```

## Problemas encontrados
 * [hilo de resolución](https://github.com/ariegd/miot-rpi-ii/blob/labF/src/labF/rssi_thingsboard/README.md)
