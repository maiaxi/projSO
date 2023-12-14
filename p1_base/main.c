#include <limits.h>
#include <stdio.h> // Sofia
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/types.h>



#include <fcntl.h>
#include "constants.h"
#include "operations.h"
#include "parser.h"

typedef struct {
  int input_fd;
  int output_fd;
  unsigned int state_access_delay_ms;
  int MAX_THREADS;
  pthread_mutex_t *trinco;
  pthread_rwlock_t *rwl;
} thread_args_t;


void *thread_function(void *arg){
  int input_fd = ((thread_args_t *)arg)->input_fd;
  int output_fd = ((thread_args_t *)arg)->output_fd;
  pthread_mutex_t *trinco = ((thread_args_t *)arg)->trinco;
  //pthread_rwlock_t *rwl = ((thread_args_t *)arg)->rwl;
  unsigned int event_id, delay;
  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  //write by parts the switch case
  pthread_mutex_lock(trinco);
  switch(get_next(input_fd)){
    case CMD_CREATE:
      if (parse_create(input_fd, &event_id, &num_rows, &num_columns) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        //why is this continue here?
        exit(EXIT_FAILURE);
      }
      if (ems_create(event_id, num_rows, num_columns)) {
        fprintf(stderr, "Failed to create event\n");
      }
      break;
    case CMD_RESERVE:
      num_coords = parse_reserve(input_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);
      if (num_coords == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        exit(EXIT_FAILURE);
      }

      if (ems_reserve(event_id, num_coords, xs, ys)) {
        fprintf(stderr, "Failed to reserve seats\n");
      }
      break;
    case CMD_SHOW:
      if (parse_show(input_fd, &event_id) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        exit(EXIT_FAILURE);
      }

      if (ems_show(event_id, output_fd)) {
        fprintf(stderr, "Failed to show event\n");
      }
      break;
    case CMD_LIST_EVENTS:
      if (ems_list_events(output_fd)) {
        fprintf(stderr, "Failed to list events\n");
      }
      break;
    case CMD_WAIT:
      if (parse_wait(input_fd, &delay, NULL) == -1) {  // thread_id is not implemented
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        exit(EXIT_FAILURE);
      }
      if (delay > 0) {
        printf("Waiting...\n");
        ems_wait(delay);
      }
      break;
    case CMD_INVALID:
      fprintf(stderr, "Invalid command. See HELP for usage\n");
      break;
    case CMD_HELP:
      printf(
          "Available commands:\n"
          "  CREATE <event_id> <num_rows> <num_columns>\n"
          "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
          "  SHOW <event_id>\n"
          "  LIST\n"
          "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
          "  BARRIER\n"                      // Not implemented
          "  HELP\n");
      break;  
    case CMD_BARRIER:  // Not implemented
    case CMD_EMPTY:
      break;
    case EOC:
      printf("Child process with PID %d finished\n", getpid());
      close(input_fd);
      close(output_fd);
      exit(EXIT_SUCCESS);
      break;
  }
  pthread_mutex_unlock(trinco);
  return NULL;
}


int main(int argc, char *argv[]) {
  pthread_mutex_t trinco;
  pthread_mutex_init(&trinco, NULL);

  pthread_rwlock_t rwl;
  pthread_rwlock_init(&rwl, NULL);
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  struct dirent *entry;
  DIR *dir;
  int input_fd;
  int output_fd;
  if (argc < 4) {
    fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
    return 1;
  }
  
  char *directoryPath = argv[1];
  //constant for the number of processes that can be created
  const int MAX_PROC = atoi(argv[2]);
  printf("MAX_PROC: %d\n", MAX_PROC);
  const int MAX_THREADS = atoi(argv[3]);
  printf("MAX_THREADS: %d\n", MAX_THREADS);
  
  if (argc == 5) {
    char *endptr;
    unsigned long int delay = strtoul(argv[4], &endptr, 10);
    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }
    state_access_delay_ms = (unsigned int)delay;
  }
  dir = opendir(directoryPath);
  input_fd = chdir(directoryPath);
  output_fd = chdir(directoryPath);

  if (dir == NULL){
    printf("Error opening directory %s", directoryPath);
    return 1;
  }


  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  int active_children = 0;
  while(1){
    entry = readdir(dir);
    if(entry == NULL){
      close(input_fd);
      close(output_fd);
      break;
    }
    
    //verifyies if the entry name ends up in ".jobs"
    const char *dot = strrchr(entry->d_name, '.');
    if(!dot || dot == entry->d_name){
      continue;
    }
    if(strcmp(dot, ".jobs") != 0){
      continue;
    }
    if(active_children == MAX_PROC){
      int status;
      wait(&status);
      active_children--;
    }
    pid_t pid = fork();
    if(pid == -1){
      perror("fork");
      continue;
    }
    if(pid == 0){
      printf("Child process with PID %d sarting\n", getpid());
      input_fd = open(entry->d_name, O_RDONLY);
      if(input_fd == -1){
        continue;
      }
      printf("%s\n", entry->d_name);
      char *output_name = malloc(strlen(entry->d_name) + 1);
      strcpy(output_name, entry->d_name);
      char *dot2 = strrchr(output_name, '.');
      *dot2 = '\0';
      strcat(output_name, ".out");
      output_fd = open(output_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

      if(output_fd == -1){
        printf("Error opening file %s\n", output_name);
        free(output_name);
        close(input_fd);
        continue;
      }
      free(output_name);
      int total_threads = 0;
      pthread_t threads[MAX_THREADS];
      while(1){
        //this is where the threads are created
        thread_args_t args = {
          .input_fd = input_fd,
          .output_fd = output_fd,
          .state_access_delay_ms = state_access_delay_ms,
          .MAX_THREADS = MAX_THREADS,
          .trinco = &trinco,
          .rwl = &rwl
        };
        if(total_threads == MAX_THREADS){
          //wait for at least one thread to finish
          pthread_join(threads[0], NULL);
          for(int i = 1; i < MAX_THREADS; i++){
            threads[i-1] = threads[i];
          }
          total_threads--;
        }
        if(pthread_create(&threads[total_threads], NULL, thread_function, &args) != 0){
          perror("pthread_create");
          break;
        }
        total_threads++;
      }
      close(input_fd);
      close(output_fd);
      exit(EXIT_SUCCESS);
    }
    else{
      active_children++;
    }
  }
  //wait for all the children to finish
  while (active_children > 0){
    int status;
    wait(&status);
    active_children--;
  }
  closedir(dir);
  ems_terminate();
  return 0;
}
