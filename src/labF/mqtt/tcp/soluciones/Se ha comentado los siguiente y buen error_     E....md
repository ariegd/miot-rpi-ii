Esta nueva salida confirma dos cosas fundamentales: primero, que el código ahora **sí avanza** y llega a ejecutar mqtt\_app\_start() (como indica la "Linea 164"); y segundo, que el error actual es puramente de **red**.

El error esp-tls: couldn't get hostname for :test.mosquitto.org: getaddrinfo() returns 202 significa que el ESP32 no puede encontrar la dirección IP del servidor porque **no tiene conexión a internet**.

### **¿Por qué falla ahora?**

Al comentar la línea //ESP\_ERROR\_CHECK(example\_connect());, has desactivado la conexión WiFi del ESP32. Sin WiFi, el dispositivo no tiene acceso a los servidores DNS para traducir "test.mosquitto.org" en una IP, ni una ruta para enviar los datos.

### **La Solución Correcta**

Para que funcione, **debes mantener la conexión WiFi activa**, pero asegurarte de que el programa no se quede bloqueado. Sigue estos pasos:

1. **Descomenta la conexión WiFi:** El ESP32 necesita estar en la red para hablar con el broker.  
2. **Usa la IP directa (Temporal):** Para descartar problemas de DNS con tu Hotspot móvil, vamos a usar la dirección IP de Mosquitto directamente.

Modifica tu app\_main.c así:

C

    ESP\_LOGI(TAG, "----------------- Linea 161 \---------------------");  
    // 1\. VOLVER A ACTIVAR WIFI (Obligatorio)  
    ESP\_ERROR\_CHECK(example\_connect());

    // 2\. Esperar a que la IP esté realmente lista en el stack  
    vTaskDelay(pdMS\_TO\_TICKS(2000)); 

    ESP\_LOGI(TAG, "----------------- Linea 164 \---------------------");  
    mqtt\_app\_start();

### **Cambio en la configuración (Menuconfig o Código)**

Dado que el error es couldn't get hostname, el sistema falla al intentar resolver el nombre. Prueba a cambiar el Broker URL en idf.py menuconfig (o directamente en tu código si usas una cadena fija) por la IP numérica:

* **De:** mqtt://test.mosquitto.org:1883  
* **A:** mqtt://91.121.93.94:1883

### **Resumen del diagnóstico**

* **Si comentas example\_connect()**: El código corre rápido pero falla porque no hay radio WiFi encendida (error getaddrinfo).  
* **Si activas example\_connect()**: El código se conectará al WiFi de tu Galaxy A53. Si tras obtener la IP ves el mensaje de "Linea 164" pero sigue dando error de conexión, es que tu móvil no está dando salida a internet al ESP32.

**Prueba esto:** Descomenta la línea de conexión, usa la IP 91.121.93.94 y verifica si el log de la "Linea 164" aparece después del evento de IP.