#include "proxy_parse.h"
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
#include <netinet/tcp.h>

#define QUEUE_LENGTH 10
#define RECV_BUFFER_SIZE 2048

/* TODO: proxy()
 * Establish a socket connection to listen for incoming connections.
 * Accept each client request in a new process.
 * Parse header of request and get requested URL.
 * Get data from requested remote server.
 * Send data to the client
 * Return 0 on success, non-zero on failure
*/
int proxy(char *proxy_port) {
  int status;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  int sockfd, new_fd;
  struct addrinfo hints, *res, *p;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  // hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, proxy_port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 1;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
      perror("proxy: socket");
      continue;
    }

    //if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    //  perror("setsockopt");
    //  exit(1);
    //}
    
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("proxy: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(res);

  if (p == NULL) {
    fprintf(stderr, "proxy: failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, QUEUE_LENGTH) == -1) {
    perror("listen");
    exit(1);
  }

  pid_t childpid;

  while(1) {
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    // inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    
    if ((childpid = fork()) == 0) {
      //handle the connecting client request
      char buff[RECV_BUFFER_SIZE];
      ssize_t recv_bytes;
      while((recv_bytes = recv(new_fd, buff, RECV_BUFFER_SIZE, 0)) > 0) {
        if (recv_bytes == -1) {
          perror("recv");
	  continue;
	}
	// have client request (max size 2048 bytes)
	// TODO: 
	// - parse out request line and headers
	// - open connection to requested resource
	// - send request to resource
	// - recv response from resource
	// - write to client socket
	
	//size_t read_bytes = strlen(buff);
	//size_t bytes_sent = 0;
	//ssize_t send_bytes;
	//while(bytes_sent < read_bytes) {
	//  send_bytes = send(new_fd, buff + bytes_sent, read_bytes - bytes_sent, 0);
	//  if (send_bytes == -1) {
        //    perror("send");
	//    close(new_fd);
	//    return 3;
        //  }
	//  bytes_sent += send_bytes;
	//}
	struct ParsedRequest *client_request = ParsedRequest_create();

	if (ParsedRequest_parse(client_request, buff, strlen(buff)) == -1) {
	  //TODO: Handle failed request parse
	}
        
        close(new_fd);
      }
      //handle killing child (process)
    }
    close(new_fd);
  }
  return 0;
}


int main(int argc, char * argv[]) {
  char *proxy_port;

  if (argc != 2) {
    fprintf(stderr, "Usage: ./proxy <port>\n");
    exit(EXIT_FAILURE);
  }

  proxy_port = argv[1];
  return proxy(proxy_port);
}
