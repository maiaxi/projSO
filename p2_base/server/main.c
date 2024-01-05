#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "server/eventlist.h"

#define NUM_WORKER_THREADS 4
#define BUFFER_SIZE 10

typedef struct {
  int sess_id;
  char req_pipe_path[40];
  char resp_pipe_path[40];
} client_info;

int num_threads = 0;
int num_clients = 0;
int server_fd = 0;

client_info buffer[BUFFER_SIZE];
int buffer_in = 0;
int buffer_out = 0;
int buffer_count = 0;

pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t buffer_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer_not_full = PTHREAD_COND_INITIALIZER;

volatile sig_atomic_t print_event_info = 0;

// signal handler for SIGUSR1
void sigusr1_handler(int signum) {
  if (signum != SIGUSR1) {
    fprintf(stderr, "[ERR]: sigusr1_handler received wrong signal\n");
    exit(EXIT_FAILURE);
  }
  print_event_info = 1;
  // Set up SIGUSR1 handler
  struct sigaction sa;
  sa.sa_handler = sigusr1_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;  // Automatically restart certain interrupted system calls
  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    perror("Failed to set up SIGUSR1 handler");
  }
}

void* worker_thread(void* arg) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  pthread_sigmask(SIG_BLOCK, &set, NULL);

  int temp_session;
  char op_code[2];
  // opens the client request pipe
  // print the length of the request pipe path
  pthread_mutex_lock(&buffer_mutex);
  while (buffer_count == 0) {
    pthread_cond_wait(&buffer_not_empty, &buffer_mutex);
  }
  client_info info = buffer[buffer_out];
  buffer_out = (buffer_out + 1) % BUFFER_SIZE;
  buffer_count--;
  pthread_cond_signal(&buffer_not_full);
  pthread_mutex_unlock(&buffer_mutex);

  int req_fd = open(info.req_pipe_path, O_RDONLY);

  if (req_fd == -1) {
    fprintf(stderr, "[ERR]: open(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
    exit(EXIT_FAILURE);
  }

  int resp_fd = open(info.resp_pipe_path, O_WRONLY);
  if (resp_fd == -1) {
    fprintf(stderr, "[ERR]: open(%s) failed: %s\n", info.resp_pipe_path, strerror(errno));
    exit(EXIT_FAILURE);
  }
  printf("Client %d connected!\n", info.sess_id);

  while (1) {
    // gets the first char of the message to determine the operation
    printf("Waiting for client %d to send a message...\n", info.sess_id);
    read(req_fd, op_code, sizeof(char) * 2);
    printf("Operation code: %c\n", op_code[0]);
    switch (op_code[0]) {
      case '2':
        printf("Client %d disconnected!\n", info.sess_id);
        close(req_fd);
        close(resp_fd);
        pthread_exit(EXIT_SUCCESS);
      case '3':
        // the server asks for the session id
        if (read(req_fd, &temp_session, sizeof(int)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // the server asks the event id
        unsigned int event_id;
        if (read(req_fd, &event_id, sizeof(unsigned int)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // asks for the event rows
        size_t event_row;
        if (read(req_fd, &event_row, sizeof(size_t)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // asks for the event collumns
        size_t event_collumns;
        if (read(req_fd, &event_collumns, sizeof(size_t)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // creates the event
        if (ems_create(event_id, event_row, event_collumns)) {
          fprintf(stderr, "Failed to create event\n");
        }
        break;

      case '4':
        // the server asks for the session id
        if (read(req_fd, &temp_session, sizeof(int)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }
        // the server asks the event id
        unsigned int event_id_reserve;
        if (read(req_fd, &event_id_reserve, sizeof(unsigned int)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // asks for the number of seats
        size_t num_seats;
        if (read(req_fd, &num_seats, sizeof(size_t)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // asks for the x coordinates
        size_t* xs = malloc(sizeof(size_t) * num_seats);
        if (read(req_fd, xs, sizeof(size_t) * num_seats) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // asks for the y coordinates
        size_t* ys = malloc(sizeof(size_t) * num_seats);
        if (read(req_fd, ys, sizeof(size_t) * num_seats) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        if (ems_reserve(event_id_reserve, num_seats, xs, ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }
        break;

      case '5':
        // the server asks for the session id
        if (read(req_fd, &temp_session, sizeof(int)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // the server asks the event id
        unsigned int event_id_show;
        if (read(req_fd, &event_id_show, sizeof(unsigned int)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // shows the event
        if (ems_show(resp_fd, event_id_show)) {
          fprintf(stderr, "Failed to show event\n");
        }
        break;

      case '6':
        // the server asks for the session id
        if (read(req_fd, &temp_session, sizeof(int)) == -1) {
          fprintf(stderr, "[ERR]: read(%s) failed: %s\n", info.req_pipe_path, strerror(errno));
          exit(EXIT_FAILURE);
        }

        // lists the events
        if (ems_list_events(resp_fd)) {
          fprintf(stderr, "Failed to list events\n");
        }
        break;
    }
  }
}

int main(int argc, char* argv[]) {
  /* Set up SIGUSR1 handler
   We use this type of handler because
   we want to be able to interrupt the
   server so that it prints the events i
   info and does not interrupt the system calls*/
  struct sigaction sa;
  sa.sa_handler = sigusr1_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;  // Automatically restart certain interrupted system calls
  if (sigaction(SIGUSR1, &sa, NULL) == -1) {
    perror("Failed to set up SIGUSR1 handler");
    return 1;
  }

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

  if (unlink(argv[1]) != 0 && errno != ENOENT) {
    fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", argv[1], strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (mkfifo(argv[1], 0640) != 0) {
    fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // initializes the worker threads
  pthread_t worker_threads[NUM_WORKER_THREADS];
  for (int i = 0; i < NUM_WORKER_THREADS; i++) {
    pthread_create(&worker_threads[i], NULL, worker_thread, NULL);
  }

  while (1) {
    if (print_event_info) {
      print_event_info = 0;
      ems_print_event_info();
    }
    client_info info;
    // waits for a client to connect
    printf("Waiting for client to connect...\n");
    server_fd = open(argv[1], O_RDONLY);
    if (server_fd == -1) {
      fprintf(stderr, "[ERR]: open(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }

    // reads the first char of the message to determine if the client wants to connect
    char start[2];

    if (read(server_fd, start, sizeof(char) * 2) == -1) {
      fprintf(stderr, "[ERR]: read(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }
    if (start[0] != '1') {
      fprintf(stderr, "[ERR]: read(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }

    // reads the first part of the message from the client
    char buffer_req[40];
    if (read(server_fd, buffer_req, 40) == -1) {
      fprintf(stderr, "[ERR]: read(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }

    // reads the second part of the message from the client
    char buffer_resp[40];
    if (read(server_fd, buffer_resp, 40) == -1) {
      fprintf(stderr, "[ERR]: read(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }

    info.sess_id = num_clients;
    for (int i = 0; i < 40; i++) {
      strcpy(info.req_pipe_path, buffer_req);
      strcpy(info.resp_pipe_path, buffer_resp);
    }

    num_clients++;

    close(server_fd);
    server_fd = open(argv[1], O_WRONLY);
    // sends the session id to the client
    int session_id = num_clients;
    if (write(server_fd, &session_id, sizeof(int)) == -1) {
      fprintf(stderr, "[ERR]: write(%s) failed: %s\n", argv[1], strerror(errno));
      exit(EXIT_FAILURE);
    }
    pthread_mutex_lock(&buffer_mutex);
    while (buffer_count == BUFFER_SIZE) {
      pthread_cond_wait(&buffer_not_full, &buffer_mutex);
    }
    buffer[buffer_in] = info;
    buffer_in = (buffer_in + 1) % BUFFER_SIZE;
    buffer_count++;
    pthread_cond_signal(&buffer_not_empty);
    pthread_mutex_unlock(&buffer_mutex);
    close(server_fd);
  }
  close(server_fd);
  unlink(argv[1]);

  ems_terminate();
}