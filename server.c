#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define MAX_BUFFER_SIZE 100000
#define EOL "\r\n"
#define EOL_SIZE 2

void *connection(void * );

typedef struct {
  char *type;
  char *ext;
} extn;

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

void error(char *msg) {
  perror(msg);
  exit(1);
}

int get_file_size(int fd) {
  struct stat stat_struct;
  if (fstat(fd, &stat_struct) == -1) {
    return -1;
  }
  return (int)stat_struct.st_size;
}

void send_msg(int fd, char *msg, int isJPEG) {
  if (!isJPEG) {
    write(fd, msg, strlen(msg));
  }
  else {
    write(fd, msg, isJPEG);
  }
}

int receive_new(int fd, char *buffer) {
 char *p = buffer; // Use of a pointer to the buffer rather than dealing with the buffer directly
 int eol_matched = 0; // Use to check whether the recieved byte is matched with the buffer byte or not
 while (read(fd, p, 1) != 0) {
   if (*p == EOL[eol_matched]) {
     ++eol_matched;
     if (eol_matched == EOL_SIZE) {
       *(p + 1 - EOL_SIZE) = '\0'; // End the string
       return strlen(buffer); // Return the bytes recieved
     }
  }
  else {
    eol_matched = 0;
  }
  p++; // Increment the pointer to receive next byte
}
  return 0;
}

void *connection(void *params) {
  char request[500], resource[500], *ptr;
  int file;
  int isJPEG = 0;
  arguments *args = params;
  int fd = args->newsockfd;
  char *webroot = args->webroot;
  char buffer[MAX_BUFFER_SIZE];
  bzero(buffer, MAX_BUFFER_SIZE);
  if (receive_new(fd, request) == 0) {
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
    if (strncmp(request, "GET ", 4) == 0) { // Is this a valid GET request?
      ptr = request + 4;
    }
    if (ptr[strlen(ptr) - 1] == '/') {
      strcat(ptr, "index.html");
    }
    strcpy(resource, webroot);
    strcat(resource, ptr); // Build the location of the file
    // CODE WORKS UP TO HERE!
    for (int i=0; extensions[i].ext != NULL; i++) {
      if (strstr(resource, extensions[i].ext) != NULL) {
        file = open(resource, O_RDONLY);
        printf("Opening %s\n", resource);
        if (file == -1) {
          send_msg(fd, "HTTP/1.1 404\r\n\r\n", isJPEG);
        }
        else {
          send_msg(fd, "HTTP/1.1 200 OK\r\n", isJPEG);
          send_msg(fd, "Content-Type: ", isJPEG);
          send_msg(fd, extensions[i].type, isJPEG);
          send_msg(fd, "\r\n\r\n", isJPEG);
          if (strcmp(extensions[i].ext, "jpg") == 0){
            FILE *f = fopen(resource, "rb");
            fseek(f, 0, SEEK_END);
            int file_len = ftell(f);
            fseek(f, 0, SEEK_SET);
            isJPEG = fread(buffer, 1, file_len, f);
            fclose(f);
          }
          else {
            read(file, buffer, MAX_BUFFER_SIZE);
          }
        }
      }
    }
    send_msg(fd, buffer, isJPEG);
  }
  shutdown(fd, SHUT_RDWR);
  return NULL;
}

int main(int argc, char **argv) {
	int sockfd, newsockfd, portno;
	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen;
  pthread_t tid;
	int n;

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
	/* Accept a connection - block until a connection is ready to
	 be accepted. Get back a new file descriptor to communicate on. */
  char *webroot = argv[2];
  arguments args;
  printf("Accepting\n");
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
    pthread_join(tid, NULL);
  }
  close(newsockfd);
  close(sockfd);
  return 0; /* we never get here */
}
