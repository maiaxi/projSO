//#include <stdio.h>
#define DT_DIR 4
#define DT_REG 8
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define MAX_COMMAND_LENGTH 100 // Sofia| se for para ficar meter no ficheiro constants.h

void processJobs(const char *directory) {
    DIR *dir;
    struct dirent *entry;

    // Open directory
    dir = opendir(directory);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    // Read directory contents
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Check if it's a regular file
            // Check if the file has a .jobs extension
            const char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ".jobs") == 0) { // Sofia this
                // Construct full file paths
                char inputFilePath[256];
                char outputFilePath[256];

                snprintf(inputFilePath, sizeof(inputFilePath), "%s/%s", directory, entry->d_name);
                snprintf(outputFilePath, sizeof(outputFilePath), "%s/%s.out", directory, entry->d_name);

                // Open input file
                int input_fd = open(inputFilePath, O_RDONLY); // Sofia this
                if (input_fd == -1) {
                    perror("Error opening input file");
                    continue;
                }

                // Create or open output file
                int output_fd = open(outputFilePath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                if (output_fd == -1) {
                    perror("Error opening/creating output file");
                    close(input_fd);
                    continue;
                }

                char command[MAX_COMMAND_LENGTH];
                ssize_t bytes_read;

                // Read commands from input file
                while ((bytes_read = read(input_fd, command, MAX_COMMAND_LENGTH)) > 0) {
                    // Process command (you'll need your specific command processing logic here)
                    // For demonstration, write to output file
                    write(output_fd, command, bytes_read);
                }

                // Close files
                close(input_fd);
                close(output_fd);
            }
        }
    }

    // Close directory
    closedir(dir);
}

/*int main() {
    const char *directoryPath = "jobs"; // Directory path containing .jobs files

    processJobs(directoryPath);

    return 0;
}*/ // Sofia
