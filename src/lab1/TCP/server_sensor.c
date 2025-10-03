#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <stdlib.h>
#include <time.h>

typedef struct {
  int id;
  float temperatura;
  char estado;
  double fecha;
} info_sensor;

int main(int argc, char *argv[]) {
	// port to start the server on
	int SERVER_PORT = 8878;

	// socket address used for the server
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// htons: host to network short: transforms a value in host byte
	// ordering format to a short value in network byte ordering format
	server_address.sin_port = htons(SERVER_PORT);

	// htonl: host to network long: same as htons but to long
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	// create a TCP socket, creation returns -1 on failure
	int listen_sock;
	if ((listen_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Error en creacion de socket de escucha\n");
		return 1;
	}

	// bind it to listen to the incoming connections on the created server
	// address, will return -1 on error
	if ((bind(listen_sock, (struct sockaddr *)&server_address,
	          sizeof(server_address))) < 0) {
		printf("Error en bind\n");
		return 1;
	}

	int wait_size = 16;  // maximum number of waiting clients, after which
	                     // dropping begins
	if (listen(listen_sock, wait_size) < 0) {
		printf("Error en listen\n");
		return 1;
	}

	// socket address used to store client address
	struct sockaddr_in client_address;
	int client_address_len = 0;

	// run indefinitely
	while (true) {
		// open a new socket to transmit data per connection
		int sock;
		if ((sock =
		         accept(listen_sock, (struct sockaddr *)&client_address,
		                &client_address_len)) < 0) {
			printf("Error en apertura de socket\n");
			return 1;
		}
		
		int n = 0;
		info_sensor sensor;
		char buffer[16];

		printf("Cliente conectado con IP:  %s\n",
		       inet_ntoa(client_address.sin_addr));

		// keep running as long as the client keeps the connection open
		while ((n = recv(sock, &sensor, sizeof(sensor), 0)) > 0) {

			printf("Recibido: identificador=%d  temperatura=%.1f estado=%c fecha=%.0f\n",
			            sensor.id, sensor.temperatura, sensor.estado, sensor.fecha);

			// echo received content back
                        if ( sensor.estado == 'N')
                            strcpy(buffer, "NEGRO");
                        else if (sensor.estado == 'R')
                            strcpy(buffer, "ROJO");
                        else
                            strcpy(buffer, "DESCONOCIDO");
                            
			send(sock, buffer,sizeof(buffer), 0);
		}

		close(sock);
	}

	close(listen_sock);
	return 0;
}
