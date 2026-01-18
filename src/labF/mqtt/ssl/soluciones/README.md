| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

# Cómo general un certificado digital para el servidor local como este: `mosquitto_sub -h 10.174.33.72 -p 8883 -t "/topic/qos0"` sobre el cliente mqtt con ssl ejemplo: `esp-idf/examples/protocols/mqtt/ssl`

Notas Importantes: 
* El certificado una vez que se genera jamás se debe renombrar.
* Copiar en el directorio y cambiar permisos
```
# borrar primero
rm -r /etc/mosquitto/certs/*

# copiar
cp *.pem /etc/mosquitto/certs/
cp *.crt /etc/mosquitto/certs/
cp *.key /etc/mosquitto/certs/

# cambiar permisos
sudo /usr/sbin/mosquitto -c /etc/mosquitto/mosquitto.conf
sudo chown -R mosquitto:mosquitto /etc/mosquitto/certs/
```
* El servidor, mosquitto en nuestro caso, en el host se debe reiniciar: 
```
sudo systemctl restart mosquitto
```

Para el ejemplo `esp-idf/examples/protocols/mqtt/ssl` no hay una guía específica para crear certificados propios, pero en los ejemplos de Espressif se usan siempre certificados generados con openssl de forma muy similar. A partir de lo que sí está documentado, el flujo típico para un broker MQTT local con TLS en 8883 es:

1. **Crear una CA (opcional pero recomendado)**
Igual que en el ejemplo de HTTPS local control, primero se crea una CA raíz que firmará el certificado del servidor: [ESP local ctrl]
```
# 1) Crear CA raíz
openssl req -new -x509 -subj "/CN=root" -days 3650 -sha256 \
    -out mqtt_eclipseprojects_io.pem -keyout rootkey.pem \
    -addext "keyUsage=critical,digitalSignature,keyCertSign"
```

2. **Crear clave y CSR para el servidor (tu broker Mosquitto)**
Usa como CN la IP o el nombre que vayas a usar (en tu caso 10.174.33.72 o un FQDN si tienes DNS):
```
# 2) Clave privada del servidor y CSR
openssl req -newkey rsa:2048 -nodes -keyout server.key \
    -days 3650 -out server.csr \
    -subj "/CN=10.174.33.72"
```

3. **Firmar el certificado del servidor con la CA**
```
# 3) Firmar el CSR del servidor con la CA
openssl x509 -req -in server.csr -CA mqtt_eclipseprojects_io.pem -CAkey rootkey.pem \
    -CAcreateserial -out server.crt -days 500 -sha256
```
Ahora tienes:
* server.key → clave privada del broker
* server.crt → certificado del broker
* mqtt_eclipseprojects_io.pem → CA que usará el ESP32 para verificar el broker [ESP local ctrl]

4. **Configurar Mosquitto en el servidor local (puerto 8883)**
En tu mosquitto.conf (ejemplo mínimo TLS):
```
listener 8883
cafile /ruta/a/mqtt_eclipseprojects_io.pem
certfile /ruta/a/server.crt
keyfile /ruta/a/server.key
require_certificate false   # o true si quieres autenticación mutua
```

5. **Configurar el ejemplo esp-idf/examples/protocols/mqtt/ssl**
El ejemplo de SSL de ESP‑MQTT espera un CA en formato PEM para validar el broker, igual que se hace con `mqtt.eclipseprojects.io` o `test.mosquitto.org` (en esos ejemplos se descarga el CA con openssl s_client).
En tu caso:
    1. Copia rootCA.pem al directorio main/ del ejemplo y referencia ese fichero en el código (en lugar del mqtt_eclipse_org.pem o mosquitto.org.crt que usan los ejemplos).
    2. En menuconfig del ejemplo:
            * Ajusta la URI del broker a algo como:
                mqtts://10.174.33.72:8883
    3. Asegúrate de que el código pasa el buffer PEM de tu rootCA.pem a esp_mqtt_client_init a través de la configuración TLS (igual que en los ejemplos donde se embebe el CA del broker). [ESP‑MQTT SSL]
    
6. **Probar desde PC con mosquitto_sub**
Con el broker ya configurado con TLS:
```
mosquitto_sub -h 10.174.33.72 -p 8883 -t "/topic/qos0" --cafile mqtt_eclipseprojects_io.pem
```
