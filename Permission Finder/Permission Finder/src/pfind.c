#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

const mode_t P_FLAG[] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};

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

int is_matching_permissions(const char* permissions, mode_t mode) {
    for (size_t i = 0; i < strlen(permissions); i++) {
        if ((permissions[i] == '-' && (mode & P_FLAG[i]) != 0) || (permissions[i] != '-' && (mode & P_FLAG[i]) == 0)) {
            return 0;
        }
    }

    return 1;
}

int traverse(const char* path, const char* permissions, int initial_traverse) {

    // Stat the file or directory
    struct stat buf;
    int stat_result;
    if (initial_traverse) {
        stat_result = stat(path, &buf);
    } else {
        stat_result = lstat(path, &buf);
    }
    if (stat_result < 0) {
        fprintf(stderr, "Error: Cannot stat '%s'. %s.\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    mode_t mode = buf.st_mode;

    // Print path if permissions match
    if (is_matching_permissions(permissions, mode)) {
        printf("%s\n", path);
    }

    // Recurse if it's a directory
    if (S_ISDIR(mode)) {
        // Open directory
        DIR* d;
        if ((d = opendir(path)) == NULL) {
            fprintf(stderr, "Error: Cannot open directory '%s'. %s.\n", path, strerror(errno));
            return EXIT_FAILURE;
        }

        // Add a trailing slash to the path
        // First, create a string buffer with one more than PATH_MAX to leave room
        // for the trailing slash
        char full_filename[PATH_MAX + 1];

        // Initialize to an empty string by making the first character a null
        full_filename[0] = '\0';

        // Is the path the root? (just a single slash)
        if (strcmp(path, "/")) {
            // Not the root: copy the path into full_filename
            size_t copy_len = strnlen(path, PATH_MAX);
            memcpy(full_filename, path, copy_len);
            full_filename[copy_len] = '\0';
        }

        // Add the trailing slash (or, if it's the root, the only slash)
        size_t pathlen = strlen(full_filename) + 1;
        full_filename[pathlen - 1] = '/';
        full_filename[pathlen] = '\0';

        // Read the contents of the directory
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                // Append the name of the file or subdirectory
                strncpy(full_filename + pathlen, dir->d_name, PATH_MAX - pathlen);

                // Call traverse recursively
                traverse(full_filename, permissions, 0);
            }
        }

        // Close directory
        closedir(d);
    }
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
       usage(argv);
       return EXIT_FAILURE;
    }

    char* directory = NULL;
    char* permissions = NULL;
    int opt;

    // Validate arguments
    while ((opt = getopt(argc, argv, ":d:p:h")) != -1) {
        switch (opt) {
            case 'd':
                if (optarg) {
                    directory = optarg;
                }
                break;

            case 'p':
                if (optarg) {
                    permissions = optarg;
                }
                break;

            case 'h':
                usage(argv);
                return EXIT_SUCCESS;

            case '?':
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                return EXIT_FAILURE;

            // Added default option
            default:
                fprintf(stderr, "Error: Arguments could not be parsed.\n");
                return EXIT_FAILURE;
        }
    }
    if (directory == NULL) {
        fprintf(stderr, "Error: Required argument -d <directory> not found.\n");
        return EXIT_FAILURE;
    }
    if (permissions == NULL) {
        fprintf(stderr, "Error: Required argument -p <permissions string> not found.\n");
        return EXIT_FAILURE;
    }

    // Convert directory to a path using realpath
    char path[PATH_MAX];
    if (realpath(directory, path) == NULL) {
        fprintf(stderr, "Error: Cannot stat '%s'. %s.\n", directory, strerror(errno));
        return EXIT_FAILURE;
    }

    // Check that the path is a valid directory
    DIR *dir;
    if ((dir = opendir(path)) == NULL) {
        fprintf(stderr, "Error: Cannot open directory '%s'. %s.\n", path, strerror(errno));
        return EXIT_FAILURE;
    }
    closedir(dir);

    // Check that the permissions string is valid
    if (! is_valid_permissions(permissions)) {
        fprintf(stderr, "Error: Permissions string '%s' is invalid.\n", permissions);
        return EXIT_FAILURE;
    }

    // Traverse the directory
    return traverse(path, permissions, 1);
}
