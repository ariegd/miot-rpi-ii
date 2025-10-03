#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>
#include <time.h>

typedef struct {
  int id;
  float temperatura;
  char estado;
  double fecha;
} info_sensor;

int main() {
	const char* server_name = "localhost";
	const int server_port = 8877;

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// creates binary representation of server name
	// and stores it as sin_addr
	// http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
	inet_pton(AF_INET, server_name, &server_address.sin_addr);

	// htons: port in network order format
	server_address.sin_port = htons(server_port);

	// open socket
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Error en creacion de socket\n");
		return 1;
	}

	// data that will be sent to the server
	//const char* data_to_send = "Hola RPI via UDP";

	// send data
	//int len =
	//  sendto(sock, data_to_send, strlen(data_to_send), 0,
	//           (struct sockaddr*)&server_address, sizeof(server_address));

	// received echoed data back
	//char buffer[100];
	//recvfrom(sock, buffer, len, 0, NULL, NULL);

	//buffer[len] = '\0';
	//printf("Recibido: '%s'\n", buffer);
	int i = 0;
        char respuesta[16];
        srand(time(NULL));
        
        while (i < 11) {
          // data that will be sent to the server
           info_sensor sensor;
           sensor.id = rand() % 50;
           sensor.temperatura = 11.0 + (float)(rand() % 100) / 25.0;
           sensor.estado = (rand() % 5 != 0 ) ? 'N' : 'R';
           sensor.fecha = (double) time (NULL);
           
           sendto(sock, &sensor, sizeof(sensor), 0,
	           (struct sockaddr*)&server_address, sizeof(server_address));

	    // will remain open until the server terminates the connection
	    recvfrom(sock, respuesta, sizeof(respuesta), 0, NULL, NULL);
	    printf("Respuesta del servidor: %s\n", respuesta);
		      
            i++;
            sleep(1);
        }

	// close the socket
	close(sock);
	return 0;
}
