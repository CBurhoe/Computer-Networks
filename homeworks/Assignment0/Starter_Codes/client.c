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
  // https://book.systemsapproach.org/foundation/software.html#client

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

  /*
  int sockfd, numbytes;
  char buff[SEND_BUFFER_SIZE];
  struct addrinfo hints, *servinfo, *p;
  int rv;


  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((rv = getaddrinfo(server_ip, server_port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  // TODO: error handling for socket? and connect (hint: check return value and store in status)
  connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);


  */
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
