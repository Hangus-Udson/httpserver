// Name: Angus Hudson
// Student ID: 835808
// Login ID: a.hudson1/ahudson1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define MAX_BUFFER_SIZE 256
#define CRLF "\r\n" // CRLF '\r\n' indicates the end of a HTTP request/response
#define CRLF_SIZE 2

void *connection(void * );

// Structure for storing all possible extensions requested
typedef struct {
  char *type;
  char *ext;
} extn;

// A structure for storing arguments for use in pthread multithreading
typedef struct {
  char *webroot;
  int newsockfd;
  pthread_t self;
} arguments;

extn extensions[] = {
  {"text/html", "html"},
  {"image/jpeg", "jpg"},
  {"text/css", "css"},
  {"text/javascript", "js"}
};

// Printing error messages
void error(char *msg) {
  perror(msg);
  exit(1);
}

// A function that sends a message 'msg' to the client socket 'fd' using
// write()
void send_msg(int fd, char *msg, int isJPEG) {
  // We can't use strlen() for a JPEG
  if (!isJPEG) {
    write(fd, msg, strlen(msg));
  }
  else {
    write(fd, msg, isJPEG);
  }
}

// A function that will read in HTTP GET requests from the client socket 'fd'
// and write it to 'buffer'
int receive_req(int fd, char *buffer) {
  char *p = buffer;
  int CRLF_matched = 0;
  // Read in one byte at a time to 'p'
  while (read(fd, p, 1) != 0) {
    // This is potentially part of the CRLF, so increment the CRLF index
    if (*p == CRLF[CRLF_matched]) {
      CRLF_matched++;
      // We've found a '\r\n' so the request is complete, return the length
      // of the HTTP request read to the buffer
      if (CRLF_matched == CRLF_SIZE) {
        // End the string and return bytes received
        *(p + 1 - CRLF_SIZE) = '\0';
        return strlen(buffer);
      }
    }
    // Just a regular byte, so reset the CRLF index
    else {
      CRLF_matched = 0;
    }
    p++; // Increment the pointer to receive next byte
  }
  return 0;
}

// A function for processing connections, results in an appropriate HTTP
// response to valid GET requests, otherwise there is an exit with an error
// message attached. Uses a buffer to write these responses.
void *connection(void *params) {
  char request[500], resource[500], *ptr;
  int file;
  int isJPEG = 0;
  arguments *args = params;
  int fd = args->newsockfd; // The client socket
  char *webroot = args->webroot; // The web root directory of the server
  char buffer[MAX_BUFFER_SIZE];
  bzero(buffer, MAX_BUFFER_SIZE);
  // Write the HTTP request into the 'request' buffer
  if (receive_req(fd, request) == 0) {
    printf("Receive Failed\n");
  }
  else {
    printf("Request = %s\n", request);
    ptr = strstr(request, " HTTP/"); // Making sure we have a valid request
    if (ptr == NULL) {
      printf("NOT HTTP\n");
    }
    // Reset the pointer so it can be used to help build the filepath from the request
    else {
      *ptr = 0;
      ptr = NULL;
    }
    if (strncmp(request, "GET ", 4) == 0) { // Checking for a GET request
      ptr = request + 4;
    }
    if (ptr[strlen(ptr) - 1] == '/') { // No file path specified, so return index.html
      strcat(ptr, "index.html");
    }
    // Build the location of the file
    strcpy(resource, webroot);
    strcat(resource, ptr);
    // Now compare the extension of the file path to all of the 'stored' extensions
    // our server can process. If we find a match, we use the extension 'type' for
    // the 'Content-Type' part of the response, open the file, read its contents, and
    // write it to the client socket
    for (int i=0; extensions[i].ext != NULL; i++) {
      // Found a matching extension
      if (strstr(resource, extensions[i].ext) != NULL) {
        // Open the file
        file = open(resource, O_RDONLY);
        printf("Opening %s\n", resource);
        // The file wasn't found, so send 404 response
        if (file == -1) {
          send_msg(fd, "HTTP/1.1 404\r\n\r\n", isJPEG);
        }
        // The file was found and opened successfully, so 200 response
        else {
          send_msg(fd, "HTTP/1.1 200 OK\r\n", isJPEG);
          send_msg(fd, "Content-Type: ", isJPEG);
          send_msg(fd, extensions[i].type, isJPEG);
          send_msg(fd, "\r\n\r\n", isJPEG);
          FILE *f;
          // Open the file, in binary mode for a JPEG file
          if (!strcmp(extensions[i].type, "image/jpeg")) {
            f = fopen(resource, "rb");
          }
          else {
            f = fopen(resource, "r");
          }
          // Track to the end of the file, then use the location to find out
          // the length of the file
          fseek(f, 0, SEEK_END);
          int file_len = ftell(f);
          // Extra bytes that only require a partial buffer transfer
          int rem_bytes = file_len % MAX_BUFFER_SIZE;
          // Move back to the start of the file
          fseek(f, 0, SEEK_SET);
          // Read through the file in MAX_BUFFER_SIZE chunks
          while (isJPEG+MAX_BUFFER_SIZE < file_len) {
            // Read MAX_BUFFER_SIZE bytes, write the buffer to the client, then
            // clear the buffer for the next chunk
            isJPEG += fread(buffer, 1, MAX_BUFFER_SIZE, f);
            send_msg(fd, buffer, MAX_BUFFER_SIZE);
            bzero(buffer, MAX_BUFFER_SIZE);
          }
          // Read remaining bytes, and write them to the client
          isJPEG += fread(buffer, 1, rem_bytes, f);
          send_msg(fd, buffer, rem_bytes);
          fclose(f);
        }
        close(file);
      }
    }
  }
  shutdown(fd, SHUT_RDWR);
  return NULL;
}

// Main function for initializing the server and putting it in a state to
// accept multiple connections, general structure provided by the University
// of Melbourne, COMP30023, Lab 4 'server.c'
int main(int argc, char **argv) {
	int sockfd, newsockfd, portno;
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen;
  pthread_t tid;
	int n;
  // Check if all necessary arguments are provided
	if (argc < 3) {
		fprintf(stderr,"ERROR, incorrect arguments provided\n");
		exit(1);
	}
	 /* Create TCP socket */
  printf("Creating\n");
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		error("ERROR opening socket");
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));

	portno = atoi(argv[1]);

	/* Create address we're going to listen on (given port number)
	 - converted to network byte order & any IP address for
	 this machine */

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);  // store in machine-neutral format

	 /* Bind address to the socket */

  printf("Binding\n");
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
			sizeof(serv_addr)) < 0) {
		error("ERROR on binding");
	}

	/* Listen on socket - means we're ready to accept connections -
	 incoming connection requests will be queued */

	listen(sockfd,10);
	clilen = sizeof(cli_addr);
  printf("Listening\n");
  char *webroot = argv[2];
  arguments args;
  printf("Accepting\n");
  // Accept connections and use 'pthread' to run concurrent connections
  // on separate threads
  while ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen))) {
    args.webroot = webroot;
    args.newsockfd = newsockfd;
    args.self = tid;
    if (newsockfd < 0) {
      error("ERROR on accept");
    }
    if ((pthread_create(&tid, NULL, connection, (void*) &args) < 0)) {
      error("Could not create thread");
    }
    // Join the thread so we don't terminate before the thread finishes
    pthread_join(tid, NULL);
  }
  close(newsockfd);
  close(sockfd);
  return 0;
}
