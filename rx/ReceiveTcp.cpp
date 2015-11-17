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

const int rowbytes = 7680;
const int height = 2160;
const int chunk_num = 6;
const int chunk_size = rowbytes / chunk_num;

enum STREAM_TYPE { T_STREAM_VIDEO, T_STREAM_AUDIO };
struct stream_info {
  enum STREAM_TYPE type;
  uint32_t bytes;
};

int main() {
  int sock;
  int client_sock;
  int yes = 1;
  int fifo;
  // char * audio_fifo = "/tmp/audio_fifo";

  int mode = 0; // 0 = Wait, 1 = Video, 2 = Audio
  uint32_t remain_bytes = 0;
  struct sockaddr_in addr;
  struct sockaddr_in client;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(62309);
  addr.sin_addr.s_addr = INADDR_ANY;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));
  bind(sock, (struct sockaddr *)&addr, sizeof(addr));
  if (listen(sock, 5) != 0) {
    printf("Listen failed.\n");
  }

  uint32_t row;
  char *video;

  video = (char *) malloc(16588800);
  if (video == NULL) {
    printf("malloc failed.\n");
    exit(EXIT_FAILURE);
  }

  char buf[65535];
  // sock = socket(AF_INET, SOCK_DGRAM, 0);
  //
  // addr.sin_family = AF_INET;
  // addr.sin_port = htons(62308);
  // addr.sin_addr.s_addr = INADDR_ANY;
  //
  // bind(sock, (struct sockaddr *) &addr, sizeof(addr));

  // fprintf(stderr, "res: %d\n", res);
  // mkfifo("/tmp/audio_fifo", 0666);
  // fifo = open("/tmp/audio_fifo", O_WRONLY);
  // fprintf(stderr, "fifo: %d\n", fifo);

  // printf("Waiting\n");
  socklen_t len = sizeof(client);
  client_sock = accept(sock, (struct sockaddr *)&client, &len);
  // write(sock, "HELLO", 5);

  while (1) {
    int buf_size = remain_bytes != 0 && sizeof(buf) > remain_bytes ? remain_bytes : sizeof(buf);
    int recv_bytes = recv(client_sock, buf, buf_size, 0);
    // fprintf(stderr, "Receive: %i, mode: %i\n", recv_bytes, mode);

    if (mode == 0) {
      struct stream_info info;
      memcpy(&info, buf, sizeof(stream_info));

      if (info.type == T_STREAM_VIDEO) {
        mode = 1;
      }
      if (info.type == T_STREAM_AUDIO) {
        mode = 2;
      }

      remain_bytes = info.bytes;
    } else {
      if (recv_bytes != -1) {
        remain_bytes -= recv_bytes;
        // fprintf(stderr, "remain_bytes: %u\n", remain_bytes);
        write(mode, buf, recv_bytes);
        // if (mode == 1) {
        //   write(1, buf, recv_bytes);
        // } else {
        //   write(fifo, buf, recv_bytes);
        // }
        if (remain_bytes == 0) {
          remain_bytes = sizeof(stream_info);
          // fprintf(stderr, "remain_bytes zero: %u\n", remain_bytes);
          mode = 0;
        }
      }
    }
    // fprintf(stderr, "remain_bytes: %i\n", remain_bytes);

    // recv(sock, video, sizeof(video), 0);
    // struct stream_header header;
    // memcpy(&header, buf, sizeof(stream_header));
    // // memcpy(&row, buf, sizeof(uint32_t));
    // memcpy(video + rowbytes * header.row + chunk_size * header.chunk, buf + sizeof(stream_header), chunk_size);
    // memcpy(video, )
    // if (header.row == height - 1) {
    // write(1, video, rowbytes * height);
    // }
    // printf("Frame received (#%4u)\n", row);
  }


  // printf("%s\n", buf);

  //close(sock);

  return 0;
}
