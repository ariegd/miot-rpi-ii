#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
  char info;
  int x;
  int y;
} mensaje;

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

	// send

	// data that will be sent to the server
	//const char* data_to_send = "Hola, RPI via TCP";
	//send(sock, data_to_send, strlen(data_to_send), 0);
	mensaje msj;
	msj.info = 'C';
	msj.x = rand() % 100;
	msj.y = rand() % 100;
        send(sock, &msj, sizeof(msj), 0);

	// receive

	int n = 0;
	mensaje msj_recv;
	char buffer[16];

	// will remain open until the server terminates the connection
	while ((n = recv(sock, &msj_recv, sizeof(msj_recv), 0)) > 0) {
		  printf("Recibido: info=%c  x=%d y=%d\n", msj_recv.info, msj_recv.x, msj_recv.y);
	}

	// close the socket
	close(sock);
	return 0;
}
