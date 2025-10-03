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
	const int server_port = 8878;

	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// creates binary representation of server name
	// and stores it as sin_addr
	// http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
	inet_pton(AF_INET, server_name, &server_address.sin_addr);

	// htons: port in network order format
	server_address.sin_port = htons(server_port);

	// open a stream socket
	int sock;
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error en creacion de socket\n");
		return 1;
	}

	// TCP is connection oriented, a reliable connection
	// **must** be established before any data is exchanged
	if (connect(sock, (struct sockaddr*)&server_address,
	            sizeof(server_address)) < 0) {
		printf("Error en conexion a servidor\n");
		return 1;
	}

	// send and receive
	
        int i = 0, n = 0;
        char respuesta[16];
        srand(time(NULL));
        
        while (i < 11) {
          // data that will be sent to the server
           info_sensor sensor;
           sensor.id = rand() % 50;
           sensor.temperatura = 11.0 + (float)(rand() % 100) / 25.0;
           sensor.estado = (rand() % 2 != 0 ) ? 'N' : 'R';
           sensor.fecha = (double) time (NULL);
           
           send(sock, &sensor, sizeof(sensor), 0);
	    // will remain open until the server terminates the connection
	    if ((n = recv(sock, &respuesta, sizeof(respuesta), 0)) > 0) {
		      printf("Respuesta del servidor: %s\n", respuesta);
	    }
            i++;
            sleep(1);
        }

	// close the socket
	close(sock);
	return 0;
}
