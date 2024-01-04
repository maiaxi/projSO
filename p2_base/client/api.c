#include "api.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define FIFO_PATHNAME "fifo.pipe"


int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  // Remove pipe if it does not exist
  if (unlink(req_pipe_path) != 0 && errno != ENOENT) {
      fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", req_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  // Create pipe
  if (mkfifo(req_pipe_path, 0640) != 0) {
      fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
  }



  // Remove pipe if it does not exist
  if (unlink(resp_pipe_path) != 0 && errno != ENOENT) {
      fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", resp_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  // Create pipe
  if (mkfifo(resp_pipe_path, 0640) != 0) {
      fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
  }

  //opens the server pipe to send the message code to the server
  /*(char) OP_CODE=1 | (char[40]) nome do pipe do cliente (para pedidos) | (char[40]) nome do pipe do cliente (para
respostas)*/

  int server_fd = open(server_pipe_path, O_WRONLY);
  if (server_fd == -1) {
      fprintf(stderr, "[ERR]: open(%s) failed: %s\n", server_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  //creates a buffer to send the message code to the server
  char buffer[81];
  memset(buffer, 0, 81);


  //writes the message code to the buffer
  buffer[0] = '1';

  //writes the request pipe path to the buffer
  for(int i = 0; i < 40; i++){
    buffer[i+1] = req_pipe_path[i];
  }
  //writes the response pipe path to the buffer
  for(int i = 0; i < 40; i++){
    buffer[i+41] = resp_pipe_path[i];
  }


  //writes the buffer to the server pipe
  if (write(server_fd, buffer, 81) != 81) {
      fprintf(stderr, "[ERR]: write(%s) failed: %s\n", server_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  //listens to the response pipe waiting for a integer response from the server
  int resp_fd = open(resp_pipe_path, O_RDONLY);
  if (resp_fd == -1) {
      fprintf(stderr, "[ERR]: open(%s) failed: %s\n", resp_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  //reads the response from the server
  int resp;
  if (read(resp_fd, &resp, sizeof(int)) == -1) {
      fprintf(stderr, "[ERR]: read(%s) failed: %s\n", resp_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  printf("resp: %d\n", resp);
  return 0;
}

int ems_quit(void) { 
  //TODO: close pipes
  //send 
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  
  
  return 1;
}
