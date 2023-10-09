#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <sys/wait.h>

#define PIPE_READ_END 0
#define PIPE_WRITE_END 1

void usage(char* argv[]) {
    printf("Usage: %s -d <directory> -p <permissions string> [-h]\n", argv[0]);
}

int is_valid_permissions(const char* str) {
    if (str == NULL || strlen(str) != 9) {
        return 0;
    }

    const char valid_chars[][3] = {{'-', 'r'},{'-', 'w'},{'-', 'x'}};

    for (size_t i = 0; i < 9; i++) {
        char c = str[i];

        if (c != valid_chars[i % 3][0] && c != valid_chars[i % 3][1]) {
            return 0;
        }
    }

    return 1;
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
       usage(argv);
       return EXIT_SUCCESS;
    }

    /* Declare two pipes: one for communication between pfind and sort processes
    another for communication between sort and parent processes */
    int pfind_to_sort[2];
    int sort_to_parent[2];

    // Checks if the pipes are created successfully,an error message is printed if either returns a negative value
    if (pipe(pfind_to_sort) < 0) {
        fprintf(stderr, "Error: Failed to create pfind_to_sort pipe. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (pipe(sort_to_parent) < 0) {
        fprintf(stderr, "Error: Failed to create sort_to_parent pipe. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Calls fork; process ID of child process is stored in pfind_pid
    pid_t pfind_pid = fork();

    // Checks if the fork was successful
    if (pfind_pid < 0) {
        fprintf(stderr, "Error: Failed to fork pfind process. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    } else if (pfind_pid == 0) {
        // This is the code for the child process for pfind
        if (close(pfind_to_sort[PIPE_READ_END]) < 0) {
            fprintf(stderr, "Error: Failed to close read end of pfind_to_sort pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Duplicate the file descriptor for the write end of the pfind_to_sort pipe to stdout
        if (dup2(pfind_to_sort[PIPE_WRITE_END], STDOUT_FILENO) < 0) {
            fprintf(stderr, "Error: Failed to duplicate write end of pfind_to_sort pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Close the read end of the sort_to_parent pipe as it is not needed by the child process
        if (close(sort_to_parent[PIPE_READ_END]) < 0) {
            fprintf(stderr, "Error: Failed to close read end of sort_to_parent pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Close the write end of the sort_to_parent pipe as it is not needed by the child process
        if (close(sort_to_parent[PIPE_WRITE_END]) < 0) {
            fprintf(stderr, "Error: Failed to close write end of sort_to_parent pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Execute the pfind program with specified command-line arguments
        // (will only return if there is a problem)
        execv("./pfind", argv);

        // If the above returns, it indicates failure
        fprintf(stderr, "Error: pfind failed.\n");
        return EXIT_FAILURE;

    }

    // This code runs in the parent process (spfind)

    // Wait for child (pfind) to finish
    pid_t wait_pid;
    int status;
    wait_pid = wait(&status);

    // Return failure if pfind child process was not successful
    if ((status != EXIT_SUCCESS) || (wait_pid < 0)) {
        exit(EXIT_FAILURE);
    }

    // Create a new process by forking the current process, with sort_pid holding the process ID
    pid_t sort_pid = fork();

    // Check if the fork to create the sort child process was successful
    if (sort_pid < 0) {
        fprintf(stderr, "Error: Failed to fork sort process. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    } else if (sort_pid == 0) { // Code for child process (sort)
        if (close(pfind_to_sort[PIPE_WRITE_END]) < 0) {
            fprintf(stderr, "Error: Failed to close write end of pfind_to_sort pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Duplicate the file descriptor for the read end of the pfind_to_sort pipe to stdin
        if (dup2(pfind_to_sort[PIPE_READ_END], STDIN_FILENO) < 0) {
            fprintf(stderr, "Error: Failed to duplicate read end of pfind_to_sort pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Close the read end of the sort_to_parent pipe as it is not needed by the child process
        if (close(sort_to_parent[PIPE_READ_END]) < 0) {
            fprintf(stderr, "Error: Failed to close read end of sort_to_parent pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Duplicate the file descriptor for the write end of the sort_to_parent pipe to stdout
        if (dup2(sort_to_parent[PIPE_WRITE_END], STDOUT_FILENO) < 0) {
            fprintf(stderr, "Error: Failed to duplicate write end of sort_to_parent pipe. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
        // Execute the sort program (will only return if there is a problem)
        execl("/usr/bin/sort", "sort", NULL);

        // If the above returns, it indicates failure
        fprintf(stderr, "Error: sort failed.\n");
        return EXIT_FAILURE;
    }

    // Close the read end of the pfind_to_sort pipe in the parent process
    if (close(pfind_to_sort[PIPE_READ_END]) < 0) {
        fprintf(stderr, "Error: Failed to close read end of pfind_to_sort pipe. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    // Close the write end of the pfind_to_sort pipe in the parent process
    if (close(pfind_to_sort[PIPE_WRITE_END]) < 0) {
        fprintf(stderr, "Error: Failed to close write end of pfind_to_sort pipe. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }
    // Close the write end of the sort_to_parent pipe in the parent process
    if (close(sort_to_parent[PIPE_WRITE_END]) < 0) {
        fprintf(stderr, "Error: Failed to close write end of sort_to_parent pipe. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Wait for the sort child process to complete and store its exit status
    wait_pid = waitpid(sort_pid, &status, 0);
    if ((status != EXIT_SUCCESS) || (wait_pid < 0)) {
        exit(EXIT_FAILURE);
    }

    // Declare a buffer to hold data read from the sort_to_parent pipe
    char buffer[1024];
    ssize_t read_bytes;
    ssize_t write_out;
    int line_count = 0;
    int is_usage = 0;

    // Read data from the sort_to_parent pipe until there is no more data
    while ((read_bytes = read(sort_to_parent[PIPE_READ_END], buffer, sizeof(buffer))) > 0) {
        if ((line_count == 0) && (strncmp(buffer, "Usage:", 6) == 0)) {
            is_usage = 1;
        }
        for (ssize_t i = 0; i < read_bytes; i++) {
            if (buffer[i] == '\n') {
                line_count++;
            }
        }
        write_out = write(STDOUT_FILENO, buffer, read_bytes); // Write the buffer to stdout
        if (write_out < 0) {
            fprintf(stderr, "Error: Writing to stdout failed. %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    // Close the read end of the sort_to_parent pipe in the parent process
    if (close(sort_to_parent[PIPE_READ_END]) < 0) {
        fprintf(stderr, "Error: Failed to close read end of sort_to_parent pipe. %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Print the total number of matches found except if the only thing printed was
    // the usage statement
    if (!is_usage) {
        printf("Total matches: %d\n", line_count);
    }

    return EXIT_SUCCESS;
}
