#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

/* This helper function will check if the following HTTP request's path
 * corresponds to a file. It will return  a non-zero number if it's a file
 * and 0 if its not
 */

int is_file(char* path){
    struct stat buffer;
    // Will information about the named file and write it to the area pointed to by the buffer argument
    stat(path, &buffer);
    // Finally using the element  st_mode in the stat struct we can check if it's a file with S_IFREG

    return S_ISREG(buffer.st_mode);
}

/* This helper function will check if the following HTTP request's path
 * corresponds to a directory. It will return a non-zero number if it's a
 * directory and 0 if its not.
 */
int is_directory(char* path){
  struct stat buffer;
  stat(path, &buffer);
  return S_ISDIR(buffer.st_mode);

}
/* This helper function helps us find if a directory contains and index.html file
  * we return a nonzero number and a 0 if it doesn't
  */
char* html_in_directory(char* path){
  DIR *dir;
  char* pointer = NULL;
  struct dirent *dp;
  dir = opendir(path);
  while((dp=readdir(dir))!=NULL){
    if (strcmp(http_get_mime_type(dp->d_name),"text/html")==0){
      pointer = dp->d_name;
      return pointer;
    }
  }
  return pointer;
}
/* Helper function to create html page and write to fd
*/
void write_html_to_fd(int fd, char* path, char* parent_path){
  DIR *dir;
  char buffer[8192];
  int j=0;
  char str[15];
  struct dirent *dp;
  dir = opendir(path);
  j=sprintf(buffer+j,"<html>");
  j=sprintf(buffer+j,"<body>");
  while((dp=readdir(dir))!=NULL)
  {
    //char* new_path = strcat(path,dp->d_name);
    j+=sprintf(buffer+j,"<a href=\"%s\">%s</a>",dp->d_name,dp->d_name);
      
  }
  // Parent addition link

  j+=sprintf(buffer+j,"<a href=\"%s\"> %s</a>",parent_path,parent_path);
  j+=sprintf(buffer+j,"</body>");
  j+=sprintf(buffer+j,"</html>");
  sprintf(str,"%d",j);
  http_send_header(fd,"Content-Length",str);
  http_end_headers(fd);
  http_send_data(fd,buffer,j);

}

/* Helper function to write to file descriptor if the path the given file exists
 * it will return a response 200 OK if the path exists and write to the file descriptor.
 * If the path is not found then we will return a reponse 404 Not Found indicating the 
 * path to the file was not correct or the file did not exist there
 */
void write_to_fd(int fd,char *path){
  int f_descriptor =open(path,O_RDONLY);
  char str[15];
  int size;
  int total_size=0;
  struct stat buffer;
  stat(path, &buffer); //This the best function ever implemented (Bless up fam)
  char buffer2[buffer.st_size];
  
  if (f_descriptor<0){  // Check if file descriptor was created
    http_start_response(fd,404);   // If was not return a not found response
    http_end_headers(fd);   // CRLF line

  }else{
    http_start_response(fd,200);
    http_send_header(fd,"Content-Type",http_get_mime_type(path));

    while((size=read(f_descriptor,buffer2,sizeof(buffer2)))>0)
    { 

      buffer2[size]='\0';
      total_size=size;


    }
    sprintf(str,"%d",total_size);
    http_send_header(fd,"Content-Length",str);
    http_end_headers(fd);
    http_send_data(fd,buffer2,total_size);
  }
  close(f_descriptor);

}
/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd) {
  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */

  struct http_request *request = http_request_parse(fd);
  // Create new path by appending server_files_directory and request->path(path to specific file)
  char *buffer=malloc(200*sizeof(char));
  strcpy(buffer,server_files_directory);

  char* new_path = strcat(buffer,request->path);
  if(is_file(new_path)){
    // This helper function will take care of the rest

    write_to_fd(fd,new_path);
  } else if(is_directory(new_path)){
      // check if the html 

      char *html_file;
      if((html_file=html_in_directory(new_path))){
        char * new_html_path = strcat(new_path,html_file);
        write_to_fd(fd,new_html_path);
      }else{
      // No html file then we need to return a link to each file in

        http_start_response(fd,200); 
        http_send_header(fd, "Content-Type","text/html");
        write_html_to_fd(fd,new_path,request->path);
      }
  }else{
   http_start_response(fd,404);   // If was not return a not found response
   http_end_headers(fd);
 }
 free(buffer);
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */

  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client_socket_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    return;

  }

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
}

void *handle_thread(void (*new_request)(int)){
  // We need create the thread_pool so we will make threads wait at the the pop stage until 
  int new_client_fd=wq_pop(&work_queue);
  printf("New thread\n");
  new_request(new_client_fd);
  close(new_client_fd);
  return NULL;
}
void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  /*
   * TODO: Part of your solution for Task 2 goes here!
   */
  printf("Number of threads running: %d\n", num_threads+1);
  if (num_threads>0){
    printf("Initialized threadpool\n" );
    wq_init(&work_queue);
    pthread_t threads[num_threads];
    int i;
  //if (!request) http_fatal_error("Malloc failed");
    for (i=0;i<num_threads;++i){
      pthread_create(&threads[i],NULL,(void*) handle_thread, request_handler);
    }
  }
}
  
/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);

    
    if(num_threads>0){
      wq_push(&work_queue,client_socket_number);

      
    } else {
      request_handler(client_socket_number);
      close(client_socket_number);
    }


    
  }
  if(num_threads>0){
    pthread_mutex_destroy(&work_queue.mut);
    pthread_cond_destroy(&work_queue.cond_pop);
  //pthread_cond_destroy(&work_queue.cond_push);
  }
  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
