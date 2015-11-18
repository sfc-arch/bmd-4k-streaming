#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
  int sock;
  int client_sock;
  int yes = 1;

  struct sockaddr_in addr;
  struct sockaddr_in client;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(argv[1]));
  addr.sin_addr.s_addr = INADDR_ANY;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
  bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  if (listen(sock, 5) != 0) {
    fprintf(stderr, "Listen failed.\n");
  }

  char buf[65535];
  fprintf(stderr, "Waiting\n");
  socklen_t len = sizeof(client);
  client_sock = accept(sock, (struct sockaddr *)&client, &len);
  fprintf(stderr, "Client Connected!\n");

  while (1) {
    int recv_bytes = recv(client_sock, buf, sizeof(buf), 0);

    if (recv_bytes != -1) {
      write(1, buf, recv_bytes);
    }
  }

  // printf("%s\n", buf);
  //close(sock);

  return 0;
}
