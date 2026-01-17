¡Excelente\! Los logs confirman que **finalmente has conseguido una conexión exitosa** con el broker. El retraso de 2 segundos que añadiste después de example\_connect() permitió que el stack de red se estabilizara por completo antes de iniciar MQTT.

### **Análisis de tus resultados:**

1. **Conexión Establecida**: El mensaje MQTT\_EVENT\_CONNECTED indica que el apretón de manos entre tu ESP32 y el broker test.mosquitto.org se completó correctamente.  
2. **Publicación y Suscripción**: Los logs muestran que el dispositivo envió un mensaje con ID 48555 y se suscribió a los tópicos configurados.  
3. **Recepción de Datos**: Los mensajes MQTT\_EVENT\_DATA con el tópico /topic/qos0 y datos data confirman que el ESP32 no solo envía, sino que también está escuchando correctamente lo que ocurre en el broker.

### ---

**Paso Final: Enviar tu RSSI (Potencia de Señal)**

Ahora que la comunicación funciona, vamos a sustituir el texto estático "data\_3" por el valor real del **RSSI** de tu conexión WiFi.

Modifica el caso MQTT\_EVENT\_CONNECTED dentro de tu función mqtt\_event\_handler en app\_main.c:

C

    case MQTT\_EVENT\_CONNECTED:  
        ESP\_LOGI(TAG, "MQTT\_EVENT\_CONNECTED");  
          
        // 1\. Obtener información de la conexión WiFi  
        wifi\_ap\_record\_t ap\_info;  
        esp\_wifi\_sta\_get\_ap\_info(\&ap\_info);  
          
        // 2\. Preparar el mensaje con el valor de RSSI  
        char rssi\_str\[16\];  
        snprintf(rssi\_str, sizeof(rssi\_str), "%d dBm", ap\_info.rssi);  
          
        // 3\. Publicar en tu tópico  
        // Usamos el tópico que probaste en tu PC: "/test/topic"  
        msg\_id \= esp\_mqtt\_client\_publish(client, "/test/topic", rssi\_str, 0, 1, 0);  
        ESP\_LOGI(TAG, "RSSI Enviado (%s), msg\_id=%d", rssi\_str, msg\_id);  
        break;

### **Cómo verificarlo:**

1. En la terminal de tu ordenador, ejecuta el comando que ya sabes que funciona:  
   mosquitto\_sub \-h test.mosquitto.org \-t "/test/topic" \-v  
2. Reinicia tu ESP32.  
3. Deberías ver aparecer en tu pantalla algo como:  
   /test/topic \-48 dBm