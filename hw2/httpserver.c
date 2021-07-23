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
/*Helper Method Prototypes */
void handle_file_mode(char * request_path,struct stat*my_stat,int fd);
void handle_dir_mode(char * request_path,struct stat*my_stat,int fd);
void send_data_from_file(char * file_path,int fd);
void notFound(int fd);
void * proxy_handler(void *args);
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

  if(request ==  NULL){
    notFound(fd);
    close(fd);
    return;
  }
  
  int len =  strlen(server_files_directory)+strlen(request->path) + 1;
  char request_path[len];
  strcpy(request_path,server_files_directory);
  strcat(request_path,request->path);
 // printf("%s\n",request_path);

  struct stat * my_stat= malloc(sizeof(struct stat));
  int stat_code = stat(request_path,my_stat);
  
  if(stat_code ==-1 ){
   notFound(fd);
  }else
  if(S_ISREG(my_stat->st_mode)){
    handle_file_mode(request_path,my_stat,fd);  
  }else
  if(S_ISDIR(my_stat->st_mode)){
    handle_dir_mode(request_path,my_stat,fd);
  }else{
    notFound(fd);
  }
  free(my_stat);
  close(fd);
}

void send_data_from_file(char * file_path,int fd){
  int file_descrp = open(file_path,O_RDONLY);
  if(fd !=-1){
    int rd_bytes ;
    char buffer[2048];
    while((rd_bytes=read(file_descrp,buffer,2048))){
      http_send_data(fd,buffer,rd_bytes);
    }
  }else{
    perror("File not Detected");
  }
  close(file_descrp);
}


void handle_file_mode(char * request_path,struct stat* my_stat, int fd){
  char * cont_type = http_get_mime_type(request_path);
  /*Send headers*/
  char  len[(int)my_stat->st_size+1];
  snprintf(len,my_stat->st_size +1,"%d",(int)my_stat->st_size);
  http_start_response(fd, 200);
  http_send_header(fd, "Content-type", cont_type);
  http_send_header(fd, "Server", "httpserver/1.0");
  http_send_header(fd, "Content-Length", len);
  http_end_headers(fd);
  /*send file data*/
  send_data_from_file(request_path,fd);
}


int fileExists(char * path){
    struct stat buffer;
    int exist = stat(path,&buffer);
    if(exist == 0)return 1;
    return -1;
}

void handle_dir_mode(char * request_path,struct stat*my_stat,int fd){
  char * index_html = "/index.html";
  char * index_html_path=malloc(strlen(request_path)+strlen(index_html)+1) ;
  strcpy(index_html_path,request_path);
  strcat(index_html_path,index_html);
  int exists = fileExists(index_html_path);
  if(exists==1){
    struct stat htmlStat;
    stat(index_html_path,&htmlStat);
    handle_file_mode(index_html_path,&htmlStat,fd);
  }else{
    char * cont_type = "text/html";

    /*Start responce and do headers passing*/
    http_start_response(fd, 200);
    http_send_header(fd, "Content-type", cont_type);
    http_send_header(fd, "Server", "httpserver/1.0");
    http_end_headers(fd);
    /*Headers passing done*/

    DIR * req_dir = opendir(request_path);
    struct dirent * entry;
    struct stat flag_stat;
    if(req_dir != NULL){
        while((entry = readdir(req_dir)) != NULL){
          int len = strlen(request_path);
          if(request_path[len-1]!='/') len++;
          char path [len];
          strcpy(path,request_path);
          if(request_path[len-1]!= '/') strcat(path,"/");
    //size of enrty->d_name is 256 , so make length 257 for null pointer
          len+= 257;
          char new_path [len];
          strcpy(new_path,path);
          strcat(path,entry->d_name);

          stat(path,&flag_stat);
          char buff[len+1];
          if(S_ISREG(flag_stat.st_mode)){
            snprintf(buff,len+1,"<a href='./%s'>%s/</a><br>\n",entry->d_name,entry->d_name);
          }else{
            snprintf(buff,len+1,"<a href='%s/'>%s/</a><br>\n",entry->d_name,entry->d_name);
          }
          http_send_string(fd,buff); 
        }
      closedir(req_dir);
    }
  }
  free(index_html_path);
}
struct proxy_node {
  int read_fd;
  int write_fd;
}; 
int BUF_MAX_SIZE = 1024;

void notFound(int fd){
    http_start_response(fd,404);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd,
                      "<center>"
                      "<h1>Error 404 Not Found!</h1>"
                      "<hr>"
                      "</center>");
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
  long fds1 = (((long) client_socket_fd) << 32) | fd;
  long fds2 = (((long) fd) << 32) | client_socket_fd;

  pthread_t pid[2];
  pthread_create(&pid[0], NULL, proxy_handler,(void*) fds1);
  pthread_create(&pid[1], NULL, proxy_handler,(void*) fds2);
}

void * proxy_handler(void *args){
  int descriptor1,descriptor2,read_bytes_size,written_bytes_size;
  long mask = (long)args;
  descriptor1 = *(int*)(&mask);
  // mask= mask>>32;
  int * pmask = &mask;
  pmask++;
  descriptor2 = *(pmask);
  // printf("DEs1=%d  Desc2=%d\n",descriptor1,descriptor2);
  char buffer[2048];
  while((read_bytes_size = read(descriptor1,buffer,2048)) >0){

    written_bytes_size = write(descriptor2,buffer,read_bytes_size);

    if(written_bytes_size <=0){
      break;
    }
    
  }
   close(descriptor1);
  // close(descriptor2);
  return NULL;
}

void * work_wrapper(void * args){
  void (*request_handler)(int) = args;
  while(1){
    request_handler(wq_pop(&work_queue));
  }
  
  return NULL;
}

void init_thread_pool(int num_threads, void (*request_handler)(int)) {
   wq_init(&work_queue);
   pthread_t workers[num_threads];
   for(int i=0; i< num_threads; i++){
     pthread_create(&workers[i],NULL,work_wrapper,request_handler);
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

    
    if(num_threads>1){
      wq_push(&work_queue,client_socket_number);
    }else{
      request_handler(client_socket_number);
    }
   

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
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
