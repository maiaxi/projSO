#include <limits.h>
#include <stdio.h> // Sofia

#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include <fcntl.h>
#include "constants.h"
#include "operations.h"
#include "parser.h"


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  struct dirent *entry;
  DIR *dir;
  int input_fd;
  int output_fd;
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
    return 1;
  }
  
  char *directoryPath = argv[1];
  //constant for the number of processes that can be created
  const int MAX_PROC = atoi(argv[2]);
  printf("MAX_PROC: %d\n", MAX_PROC);

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
      unsigned int event_id, delay;
      size_t num_rows, num_columns, num_coords;
      size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
      int ended = 0;
      while(1){
        switch (get_next(input_fd)){
          case CMD_CREATE:
            if (parse_create(input_fd, &event_id, &num_rows, &num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_create(event_id, num_rows, num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }

            break;
          case CMD_RESERVE:
            num_coords = parse_reserve(input_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_reserve(event_id, num_coords, xs, ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }

            break;

          case CMD_SHOW:
            if (parse_show(input_fd, &event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              
              continue;
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
              continue;
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
            ended = 1;
            break;
        }
        if(ended){
          close(input_fd);
          close(output_fd);
          break;
        }
      }
  }
  closedir(dir);
  ems_terminate();
  return 0;

}
