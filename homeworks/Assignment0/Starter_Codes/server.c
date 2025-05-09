#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>

#define QUEUE_LENGTH 10
#define RECV_BUFFER_SIZE 2048
#define SERVER_IP "127.0.0.1"

/* TODO: server()
 * Open socket and wait for client to connect
 * Print received message to stdout
 * Return 0 on success, non-zero on failure
*/
int server(char *server_port) {
  int status;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints, *res;
  int sockfd, new_fd;


  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  // hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(SERVER_IP, server_port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return 1;
  }

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
    perror("socket");
    return 2;
  }
  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    perror("bind");
    return 3;
  }
  if (listen(sockfd, QUEUE_LENGTH) == -1) {
    perror("listen");
    return 4;
  }

  while(1) {
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
  	if (new_fd == -1) {
      perror("accept");
      continue;
    }

    char buff[RECV_BUFFER_SIZE];
    ssize_t recv_bytes;
    
    while((recv_bytes = recv(new_fd, buff, RECV_BUFFER_SIZE, 0)) > 0) {
      if (recv_bytes == -1) {
        perror("recv");
        close(new_fd);
        continue;
      }
      fwrite(buff, 1, recv_bytes, stdout);
      fflush(stdout); 
    }
    
    close(new_fd);
  }

  return 0;
}

/*
 * main():
 * Parse command-line arguments and call server function
*/
int main(int argc, char **argv) {
  char *server_port;

  if (argc != 2) {
    fprintf(stderr, "Usage: ./server-c [server port]\n");
    exit(EXIT_FAILURE);
  }

  server_port = argv[1];
  return server(server_port);
}
