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

size_t get_request_length(const char *buff, size_t buffer_length) {
  for (size_t i = 0; i < buffer_length; i++) {
    if (buff[i] == '\r' && buff[i+1] == '\n' && buff[i+2] == '\r' && buff[i+3] == '\n') {
      return i + 4;
    }
  }
  return 0;
}
// Code modified from proxy_parse.c (various functions)
int Prepare_request(struct ParsedRequest *pr, char *buf, size_t buflen) {
  char * current = buf;

//  if(buflen <  ParsedRequest_requestLineLen(pr))
//  {
//    fprintf(stderr, "not enough memory for first line\n");
//    return -1;
//  }
  memcpy(current, pr->method, strlen(pr->method));
  current += strlen(pr->method);
  current[0]  = ' ';
  current += 1;


  memcpy(current, pr->path, strlen(pr->path));
  current += strlen(pr->path);

  current[0] = ' ';
  current += 1;

  memcpy(current, pr->version, strlen(pr->version));
  current += strlen(pr->version);
  memcpy(current, "\r\n", 2);
  current += 2;

  struct ParsedHeader * ph;
  size_t i = 0;

//  if (buflen < ParsedHeader_headersLen(pr)) {
//    fprintf(stderr, "buffer for printing headers too small\n");
//    return -1;
//  }

  while(pr->headersused > i) {
    ph = pr->headers+i;
    if (ph->key) {
      memcpy(current, ph->key, strlen(ph->key));
      memcpy(current+strlen(ph->key), ": ", 2);
      memcpy(current+strlen(ph->key)+2, ph->value, strlen(ph->value));
      memcpy(current+strlen(ph->key)+2+strlen(ph->value), "\r\n", 2);
      current += strlen(ph->key)+strlen(ph->value)+4;
    }
    i++;
  }
  memcpy(current, "\r\n", 2);
  return 0;
}

int Not_implemented(char *buf, int client_fd) {
  memcpy(buf, "HTTP/1.0 501 Not Implemented\r\n\r\n", 32);
  send(client_fd, buf, 32, 0);
  return 0;
}

int Bad_request(char *buf, int client_fd) {
  memcpy(buf, "HTTP/1.0 400 Bad Request\r\n\r\n", 28);
  send(client_fd, buf, 28, 0);
  return 0;
}


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

  if ((status = getaddrinfo(NULL, proxy_port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return 1;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
      perror("proxy: socket");
      continue;
    }

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

  pid_t pid;

  while(1) {
    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    
    if ((pid = fork()) == 0) {
      //handle the connecting client request
      char buff[RECV_BUFFER_SIZE];
      ssize_t recv_bytes;
      if ((recv_bytes = recv(new_fd, buff, RECV_BUFFER_SIZE, 0)) < 0) {
        if (recv_bytes == -1) {
          perror("recv");
	  continue;
	}
      }

      struct ParsedRequest *client_request = ParsedRequest_create();
      if (ParsedRequest_parse(client_request, buff, recv_bytes) < 0) {
        Bad_request(buff, new_fd);
	close(new_fd);
	continue;
      }
      int proxy_fd;
      struct addrinfo proxy_hints, *remote_servinfo;
      int rv;

      memset(&proxy_hints, 0, sizeof proxy_hints);
      proxy_hints.ai_family = AF_UNSPEC;
      proxy_hints.ai_socktype = SOCK_STREAM;
      if (client_request->port == NULL) {
        client_request->port = "80";
      }
      if ((rv = getaddrinfo(client_request->host, client_request->port, &proxy_hints, &remote_servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
      }
      for (p = remote_servinfo; p != NULL; p = p->ai_next) {
        if ((proxy_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
          perror("proxy client: socket");
          continue;
        }

        if (connect(proxy_fd, p->ai_addr, p->ai_addrlen) == -1) {
          close(proxy_fd);
          perror("proxy client: connect");
          continue;
        }
        break;
      }
      freeaddrinfo(remote_servinfo);
	
      struct ParsedRequest *proxy_request = ParsedRequest_create();
      char proxy_buff[RECV_BUFFER_SIZE];
      if (strcmp(client_request->method, "GET") != 0) {
        //TODO: return 5XX Not Implemented
        Not_implemented(proxy_buff, new_fd);	
	close(new_fd);
	close(proxy_fd);
	continue;
      }

      proxy_request->method = strdup(client_request->method);
      proxy_request->path = strdup(client_request->path);
      if (strcmp(client_request->version, "HTTP/1.0") == 0) {
        proxy_request->version = strdup(client_request->version);  
      } else {
        proxy_request->version = "HTTP/1.0";
      }
      const char *host = "Host";
      struct ParsedHeader *ph = ParsedHeader_get(client_request, host);
      if (ph == NULL) {
        if ((ParsedHeader_set(proxy_request, host, client_request->host)) != 0) {
          fprintf(stderr, "Failed to set Host header\n");
        }
      } else {
        ParsedHeader_set(proxy_request, host, ph->value);
      }
      ParsedRequest_destroy(client_request);
      const char *connection_key = "Connection";
      const char *connection_val = "close";
      if ((ParsedHeader_set(proxy_request, connection_key, connection_val)) != 0) {
        fprintf(stderr, "Failed to set Connection header\n");
      }
      if ((Prepare_request(proxy_request, proxy_buff, RECV_BUFFER_SIZE)) != 0) {
        fprintf(stderr, "Failed to convert ParsedRequest into string\n");
        return 1;
      }
	//if ((ParsedRequest_unparse(proxy_request, proxy_buff, RECV_BUFFER_SIZE)) != 0) {
	  //TODO: handle failed unparse
	//  fprintf(stderr, "Failed to prepare send buffer\n");
	  //return 1;
	//}
      size_t proxy_req_len = get_request_length(proxy_buff, RECV_BUFFER_SIZE);
      proxy_buff[proxy_req_len] = '\0';
      if (send(proxy_fd, proxy_buff, proxy_req_len, 0) == -1) {
        perror("send");
      }
      ParsedRequest_destroy(proxy_request);
      char remote_buff[RECV_BUFFER_SIZE];
      int numbytes = 0;

      while ((numbytes = recv(proxy_fd, remote_buff, RECV_BUFFER_SIZE-1, 0)) > 0) {
        size_t chunk_sent = 0;
        ssize_t total_sent = 0;
        while(total_sent < numbytes) {
          chunk_sent = send(new_fd, remote_buff + total_sent, numbytes - total_sent, 0);
          if (chunk_sent == -1) {
            if (errno == EINTR) {
              continue;
            }
            perror("client send");
            break;
          }
          total_sent += chunk_sent;
        }
        if (chunk_sent == -1) {
          break;
        }
      }
      if (numbytes == -1) {
        perror("remote server recv");
      }
      close(proxy_fd);
      close(new_fd);
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
