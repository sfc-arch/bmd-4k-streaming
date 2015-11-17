#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int rowbytes = 7680;
const int height = 2160;

int main() {
  int sock;
  struct sockaddr_in addr;
  uint32_t row;
  char *video;

  video = (char *) malloc(rowbytes * height);
  if (video == NULL) {
    printf("malloc failed.\n");
    exit(EXIT_FAILURE);
  }

  char buf[65535];
  sock = socket(AF_INET, SOCK_DGRAM, 0);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(62308);
  addr.sin_addr.s_addr = INADDR_ANY;

  bind(sock, (struct sockaddr *) &addr, sizeof(addr));

  printf("Waiting\n");

  while (1) {
    // recv(sock, buf, sizeof(buf), 0);
    recv(sock, buf, sizeof(buf), 0);
    memcpy(&row, buf, sizeof(uint32_t));
    memcpy(video + rowbytes * row, buf + sizeof(uint32_t), rowbytes);
    if (row == height - 1) {
      write(stdout, video, rowbytes * height);
    }
    // printf("Frame received (#%4u)\n", row);
  }


  // printf("%s\n", buf);

  //close(sock);

  return 0;
}
