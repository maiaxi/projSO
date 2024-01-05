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


#define TRUE 1
#define FALSE 0


int req_fd = 0;
int resp_fd = 0; 
int session_id = -1;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  char buffer[40];
  
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

  int server_fd = open(server_pipe_path, O_WRONLY);
  if (server_fd == -1) {
      fprintf(stderr, "[ERR]: open(%s) failed: %s\n", server_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }

  
  

  //creates a buffer to send the message code to the server
  char option[2];
	option[0] = '1';
	option[1] = '\0';

  //sends the message code to the server
  if (write(server_fd, option, sizeof(char)*2) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", server_fd, strerror(errno));
      exit(EXIT_FAILURE);
  }


  //sends the request pipe path to the server after padding it with 0s
  memset(buffer, 0, 40);
  strcpy(buffer, req_pipe_path);
  if (write(server_fd, buffer, 40) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", server_fd, strerror(errno));
      exit(EXIT_FAILURE);
  }
  //sends the response pipe path to the server after padding it with 0s
  memset(buffer, 0, 40);
  strcpy(buffer, resp_pipe_path);
  if (write(server_fd, buffer, 40) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", server_fd, strerror(errno));
      exit(EXIT_FAILURE);
  }
  
  close(server_fd);
  server_fd = open(server_pipe_path, O_RDONLY);

  int temp;
  //receives the session id from the server
  if (read(server_fd, &temp, sizeof(int)) == -1) {
      fprintf(stderr, "[ERR]: read(%d) failed: %s\n", server_fd,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  close(server_fd);
  session_id = temp;

  //opens the request pipe
  req_fd = open(req_pipe_path, O_WRONLY);
  if (req_fd == -1) {
      fprintf(stderr, "[ERR]: open(%s) failed: %s\n", req_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  
  //opens the response pipe
  resp_fd = open(resp_pipe_path, O_RDONLY);
  if (resp_fd == -1) {
      fprintf(stderr, "[ERR]: open(%s) failed: %s\n", resp_pipe_path,
              strerror(errno));
      exit(EXIT_FAILURE);
  }
  return 0;

}

int ems_quit(void) { 
  //sends the code
  char option[2];
	option[0] = '2';
	option[1] = '\0';

  if (write(req_fd, option, sizeof(char)*2) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
      exit(EXIT_FAILURE);
  }
  //close the request pipe
  if (close(req_fd) == -1) {
      fprintf(stderr, "[ERR]: close(%d) failed: %s\n", req_fd, strerror(errno));
      exit(EXIT_FAILURE);
  }
  //close the response pipe
  if (close(resp_fd) == -1) {
      fprintf(stderr, "[ERR]: close(%d) failed: %s\n", resp_fd, strerror(errno));
      exit(EXIT_FAILURE);
  }
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char option[2];
	option[0] = '3';
	option[1] = '\0';
  //sends the message code to the server
  if (write(req_fd, option, sizeof(char)*2) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
      return 1;
  }
  

  //sends the session id to the server
  if (write(req_fd, &session_id, sizeof(int)) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
      return 1;
  }

  //sends the event id to the server
  if (write(req_fd, &event_id, sizeof(unsigned int)) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
      return 1;
  }

  //sends the number of rows to the server
  if (write(req_fd, &num_rows, sizeof(size_t)) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
      return 1;
  }
	
  //sends the number of columns to the server
  if (write(req_fd, &num_cols, sizeof(size_t)) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
      return 1;
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  
  char option[2];
	option[0] = '4';
	option[1] = '\0';
  //sends the message code to the server
  if (write(req_fd, option, sizeof(char)*2) == -1) {
      fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
      return 1;
  }
	
	//sends the session id to the server
	int temp_id = session_id;
	if (write(req_fd, &temp_id, sizeof(int)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}

	//sends the event id to the server
	if (write(req_fd, &event_id, sizeof(unsigned int)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}

	//sends the number of seats to the server
	if (write(req_fd, &num_seats, sizeof(size_t)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}

	//sends the x coordinates to the server
	if (write(req_fd, xs, num_seats * sizeof(size_t)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}
	
	//sends the y coordinates to the server
	if (write(req_fd, ys, num_seats * sizeof(size_t)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}
  return 0;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  //sends the message code to the server
  char option[2];
	option[0] = '5';
	option[1] = '\0';
	

	if (write(req_fd, option, sizeof(char)*2) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}
	
	//sends the session id to the server
	int temp_id = session_id;
	if (write(req_fd, &temp_id, sizeof(int)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}

	//sends the event id to the server
	if (write(req_fd, &event_id, sizeof(unsigned int)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}

  //verifies if the event exists
  int exists;
  if (read(resp_fd, &exists, sizeof(int)) == -1) {
      fprintf(stderr, "[ERR]: read(%d) failed: %s\n", resp_fd, strerror(errno));
      return 1;
  }
  if (exists == FALSE) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

	//receives the number of rows from the server
	size_t num_rows;
	if (read(resp_fd, &num_rows, sizeof(size_t)) == -1) {
			fprintf(stderr, "[ERR]: read(%d) failed: %s\n", resp_fd, strerror(errno));
			return 1;
	}

	//receives the number of columns from the server
	size_t num_cols;
	if (read(resp_fd, &num_cols, sizeof(size_t)) == -1) {
			fprintf(stderr, "[ERR]: read(%d) failed: %s\n", resp_fd, strerror(errno));
			return 1;
	}

	//receives the seats from the server
	unsigned int seat[num_rows * num_cols];
	if(read(resp_fd, seat, sizeof(unsigned int) * num_rows * num_cols) == -1) {
			fprintf(stderr, "[ERR]: read(%d) failed: %s\n", resp_fd, strerror(errno));
			return 1;
	}

	//prints the seats (the seat array is a 1D array, so we have to print it as a 2D array
	int aux = 0;
	//reads the seat 1 by 1
	char buffer[2];
  char space[2] = " ";
  char newline[2] = "\n";


  for (size_t i = 1; i <= num_rows; i++) {
    for (size_t j = 1; j <= num_rows; j++) {
      //if the seat is reserved puts the reservation id in the buffer
      sprintf(buffer, "%d", seat[aux++]);
      write(out_fd, buffer, strlen(buffer));
      write(out_fd, space, strlen(space));
    }
    //if its the last row, puts a newline
    if (i <= num_rows) {
      write(out_fd, newline, strlen(newline));
    }
  }
  return 0;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  //sends the message code to the server
	char option[2];
	option[0] = '6';
	option[1] = '\0';

	if (write(req_fd, option, sizeof(char)*2) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}

	//sends the session id to the server
	int temp_id = session_id;
	if (write(req_fd, &temp_id, sizeof(int)) == -1) {
			fprintf(stderr, "[ERR]: write(%d) failed: %s\n", req_fd, strerror(errno));
			return 1;
	}
  
	//receives the number of events from the server
	size_t num_events;
	if (read(resp_fd, &num_events, sizeof(size_t)) == -1) {
			fprintf(stderr, "[ERR]: read(%d) failed: %s\n", resp_fd, strerror(errno));
			return 1;
	}
	//receives the events from the server
	unsigned int events[num_events];
	if (read(resp_fd, events, sizeof(unsigned int) * num_events) == -1) {
			fprintf(stderr, "[ERR]: read(%d) failed: %s\n", resp_fd, strerror(errno));
			return 1;
	}

	char noEvents[11] = "No events\n";
  char event[8] = "Event: ";
  char newline[2] = "\n";
  char buffer[2];

	if (num_events == 0) {
		write(out_fd, noEvents, strlen(noEvents));
		return 0;
	}
	for (size_t i = 0; i < num_events; i++) {
		//puts the event id in the buffer
		sprintf(buffer, "%d", events[i]);
		write(out_fd, event, strlen(event));
		write(out_fd, buffer, strlen(buffer));
		write(out_fd, newline, strlen(newline));
	}
  return 0;
}
