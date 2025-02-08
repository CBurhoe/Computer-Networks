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

#define SEND_BUFFER_SIZE 2048


/* TODO: client()
 * Open socket and send message from stdin.
 * Return 0 on success, non-zero on failure
*/
int client(char *server_ip, char *server_port) {
  /* https://book.systemsapproach.org/foundation/software.html#client

  struct sockaddr_in sin;
  char *host;
  char buff[SEND_BUFFER_SIZE];
  int s, len;

  bzero((char *)&sin, sizeof(sin));  //bzero() vs memset() https://stackoverflow.com/questions/17096990/why-use-bzero-over-memset
  sin.sin_family = AF_INET;
  bcopy(server_ip, (char *)&sin.sin_addr, sizeof(*server_ip)); //probably broken, fix pointers or switch to getaddrinfo()
  sin.sin_port = htons(server_port); //may not work for char*

  if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }
  if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("connect");
    close(s);
    exit(1);
  }
  
  while(fgets(buff, sizeof(buff), stdin)) {
    buff[SEND_BUFFER_SIZE-1] = '\0';
    len = strlen(buff) + 1;
    send(s, buff, len, 0);
  }
  */

  /* https://beej.us/guide/bgnet/html/#a-simple-stream-client */
  int sockfd;
  char buff[SEND_BUFFER_SIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv, len;


  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(server_ip, server_port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  freeaddrinfo(servinfo);

  size_t read_bytes;
  while(fgets(buff, sizeof(buff), stdin)) {

    //buff[read_bytes] = '\0';
    buff[SEND_BUFFER_SIZE-1] = '\0';
    len = strlen(buff) + 1;
    send(sockfd, buff, len, 0);
    //size_t total = 0;
    //size_t bytes_left = read_bytes;
    //ssize_t n;

    //while(total < len) {
    //  n = send(sockfd, buff + total, bytes_left, 0);
    //  if (n == -1) {
    //    if (errno == EINTR) {
    //      continue;
    //    }
    //    perror("send");
    //    return 2;
    //  }
    //  total += n;
    //  bytes_left -= n;
    //}

    //send(sockfd, buff, len, 0);
  }

  close(sockfd);
  return 0;
}

/*
 * main()
 * Parse command-line arguments and call client function
*/
int main(int argc, char **argv) {
  char *server_ip;
  char *server_port;

  if (argc != 3) {
    fprintf(stderr, "Usage: ./client-c [server IP] [server port] < [message]\n");
    exit(EXIT_FAILURE);
  }

  server_ip = argv[1];
  server_port = argv[2];
  return client(server_ip, server_port);
}
