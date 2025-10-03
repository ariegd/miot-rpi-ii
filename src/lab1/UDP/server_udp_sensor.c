#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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
	int SERVER_PORT = 8877;

	// socket address used for the server
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;

	// htons: host to network short: transforms a value in host byte
	// ordering format to a short value in network byte ordering format
	server_address.sin_port = htons(SERVER_PORT);

	// htons: host to network long: same as htons but to long
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);

	// create a UDP socket, creation returns -1 on failure
	int sock;
	if ((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Error en creacion de socket\n");
		return 1;
	}

	// bind it to listen to the incoming connections on the created server
	// address, will return -1 on error
	if ((bind(sock, (struct sockaddr *)&server_address,
	          sizeof(server_address))) < 0) {
		printf("Error en bind\n");
		return 1;
	}

	// socket address used to store client address
	struct sockaddr_in client_address;
	//int client_address_len = 0;
	socklen_t client_address_len = sizeof(client_address);
	
	info_sensor sensor;
	char buffer[16];

	// run indefinitely
	while (recvfrom(sock, &sensor, sizeof(sensor), 0,
		                   (struct sockaddr *)&client_address,
		                   &client_address_len) > 0) {

		// inet_ntoa prints user friendly representation of the
		// ip address
		printf("Recibido: identificador=%d  temperatura=%.1f estado=%c fecha=%.0f\n",
		            sensor.id, sensor.temperatura, sensor.estado, sensor.fecha);

		// echo received content back
                if ( sensor.estado == 'N')
                    strcpy(buffer, "NEGRO");
                else if (sensor.estado == 'R')
                    strcpy(buffer, "ROJO");
                else
                    strcpy(buffer, "DESCONOCIDO");

		// send same content back to the client ("echo")
		sendto(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address,
		       sizeof(client_address));
	}

	return 0;
}
