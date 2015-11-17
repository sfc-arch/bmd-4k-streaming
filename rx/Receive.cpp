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
const int chunk_num = 6;
const int chunk_size = rowbytes / chunk_num;

struct stream_header {
  uint32_t row;
  uint32_t chunk;
};

int main() {
  int sock;
  struct sockaddr_in addr;
  uint32_t row;
  char *video;

  video = (char *) malloc(16588800);
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
    struct stream_header header;
    memcpy(&header, buf, sizeof(stream_header));
    // memcpy(&row, buf, sizeof(uint32_t));
    memcpy(video + rowbytes * header.row + chunk_size * header.chunk, buf + sizeof(stream_header), chunk_size);
    if (header.row == height - 1) {
      write(1, video, rowbytes * height);
    }
    // printf("Frame received (#%4u)\n", row);
  }


  // printf("%s\n", buf);

  //close(sock);

  return 0;
}

