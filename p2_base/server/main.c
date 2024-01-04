#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>



#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "server/eventlist.h"


#define NUM_WORKER_THREADS 4

typedef struct {
  int sess_id;
  char* req_pipe_path[41];
  char* resp_pipe_path[41];
} client_t;

int num_clients = 0;

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }
  //start an array of clients to keep track of them
  client_t* clients;
  //TODO: Intialize server, create worker threads
  if(unlink(argv[1]) != 0 && errno != ENOENT){
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[1], strerror(errno));
    exit(EXIT_FAILURE);
  }

  if(mkfifo(argv[1], 0640) != 0){
    fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  
  while (1) {
    //TODO: Read from pipe
    //TODO: Write new client to the producer-consumer buffer
    //waits for a client to connect
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
      fprintf(stderr, "[ERR]: open(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }
    char* msg = malloc(81);
    memset(msg, 0, 81);
    printf("Waiting for client...\n");
    if (read(fd, msg, 81) == -1) {
      fprintf(stderr, "[ERR]: read(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }
    printf("Client connected!\n");
    switch(msg[0]){
      case '1':
        //parse message
        char* req_pipe_path = malloc(41);
        char* resp_pipe_path = malloc(41);
        for(int i = 0; i < 40; i++){
          req_pipe_path[i] = msg[i+1];
          resp_pipe_path[i] = msg[i+41];
        }
        if(num_clients == 0){
          clients = malloc(sizeof(client_t));
        }
        else{
          clients = realloc(clients, sizeof(client_t)*(num_clients+1));
        }
        num_clients++;
        int client_id = num_clients;

        clients[client_id-1].sess_id = client_id;
        strcpy(clients[client_id-1].req_pipe_path, req_pipe_path);
        strcpy(clients[client_id-1].resp_pipe_path, resp_pipe_path);

        //send response with the session id (int)
        //opens the client pipe to send the session id
        int client_fd = open(resp_pipe_path, O_WRONLY);
        if (client_fd == -1) {
          fprintf(stderr, "[ERR]: open(%s) failed: %s\n", req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }
        //writes the session id to the client
        if (write(client_fd, &client_id, sizeof(int)) == -1) {
          fprintf(stderr, "[ERR]: write(%s) failed: %s\n", req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }


        break;
      case '2':
        printf("Client wants to unregister!\n");
        break;
      case '3':
        printf("Client wants to list!\n");
        break;
      case '4':
        printf("Client wants to get!\n");
        break;
      case '5':
        printf("Client wants to put!\n");
        break;
      case '6':
        printf("Client wants to sync!\n");
        break;
      case '7':
        printf("Client wants to shutdown!\n");
        break;
      default:
        printf("Invalid message code!\n");
        break;
    }
  }

  //TODO: Close Server

  ems_terminate();
}