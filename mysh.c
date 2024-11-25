#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fnmatch.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>

#define MAX_ARGS 64
#define MAX_CMD_LEN 1024
#define MAX_PATH_LEN 1024

void execute_command(char *command, char **args);
void change_directory(char *path);
void print_working_directory();
void handle_exit(char **args);
void handle_which(char **args);
int wildcard_expansion(char *pattern, char **args);
void parse_and_execute(char *input, int interactive);
void traverse_and_execute(const char *path);

int main(int argc, char **argv) {
    char input[MAX_CMD_LEN];
    int interactive = isatty(fileno(stdin));

    if (interactive) {
        printf("Welcome to my shell!\n");
    }

    if (argc == 1) {
        while (1) {
            if (interactive) {
                printf("mysh> ");
            }

            if (!fgets(input, MAX_CMD_LEN, stdin)) {
                break;
            }

            // Remove trailing newline
            input[strcspn(input, "\n")] = 0;
            parse_and_execute(input, interactive);
        }
    } else {
        struct stat path_stat;
        if (stat(argv[1], &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
            // If input is a directory, traverse and execute
            traverse_and_execute(argv[1]);
        } else {
            // Batch mode: Read commands from a file
            FILE *file = fopen(argv[1], "r");
            if (!file) {
                perror("Error opening file");
                exit(1);
            }

            while (fgets(input, MAX_CMD_LEN, file)) {
                // Remove trailing newline
                input[strcspn(input, "\n")] = 0;
                parse_and_execute(input, 0);
            }

            fclose(file);
        }
    }

    if (interactive) {
        printf("Exiting my shell.\n");
    }

    return 0;
}

void traverse_and_execute(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    char full_path[MAX_PATH_LEN];

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat entry_stat;
        if (stat(full_path, &entry_stat) == 0) {
            if (S_ISDIR(entry_stat.st_mode)) {
                // Found a directory, run `mysh` recursively
                printf("Entering directory: %s\n", full_path);
                pid_t pid = fork();
                if (pid == 0) {
                    // In child process
                    execl("./mysh", "./mysh", full_path, (char *)NULL);
                    perror("execl failed");
                    exit(1);
                } else if (pid > 0) {
                    // In parent process
                    int status;
                    waitpid(pid, &status, 0);
                } else {
                    perror("Fork failed");
                }
            }
        }
    }

    closedir(dir);
}

void parse_and_execute(char *input, int interactive) {
    char *args[MAX_ARGS];
    char *token = strtok(input, " ");
    int i = 0;

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    if (args[0] == NULL) {
        return;  // Empty command
    }

    // Built-in commands
    if (strcmp(args[0], "cd") == 0) {
        if (i != 2) {
            fprintf(stderr, "cd: Invalid number of arguments\n");
        } else {
            change_directory(args[1]);
        }
    } else if (strcmp(args[0], "pwd") == 0) {
        print_working_directory();
    } else if (strcmp(args[0], "exit") == 0) {
        handle_exit(args);
    } else if (strcmp(args[0], "which") == 0) {
        handle_which(args);
    } else {
        // Wildcard expansion
        char *expanded_args[MAX_ARGS];
        int expanded_args_count = 0;

        for (int j = 0; args[j] != NULL; j++) {
            if (strchr(args[j], '*') != NULL) {
                expanded_args_count += wildcard_expansion(args[j], expanded_args + expanded_args_count);
            } else {
                expanded_args[expanded_args_count++] = args[j];
            }
        }
        expanded_args[expanded_args_count] = NULL;

        // Execute command (external)
        execute_command(args[0], expanded_args);
    }
}

void execute_command(char *command, char **args) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    if (pid == 0) {
        // Child process
        execvp(command, args);
        // If execvp returns, there was an error
        perror("Exec failed");
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Command failed with exit code: %d\n", WEXITSTATUS(status));
            }
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Terminated by signal: %d\n", WTERMSIG(status));
        }
    }
}

void change_directory(char *path) {
    if (chdir(path) != 0) {
        perror("cd failed");
    }
}

void print_working_directory() {
    char cwd[MAX_PATH_LEN];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("getcwd failed");
    }
}

void handle_exit(char **args) {
    if (args[1] != NULL) {
        for (int i = 1; args[i] != NULL; i++) {
            printf("%s ", args[i]);
        }
    }
    exit(0);
}

void handle_which(char **args) {
    if (args[1] == NULL || args[2] != NULL) {
        fprintf(stderr, "which: Invalid number of arguments\n");
        return;
    }

    char *paths[] = {"/usr/local/bin", "/usr/bin", "/bin"};
    char full_path[MAX_PATH_LEN];

    for (int i = 0; i < 3; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s", paths[i], args[1]);
        if (access(full_path, X_OK) == 0) {
            printf("%s\n", full_path);
            return;
        }
    }

    printf("which: %s not found\n", args[1]);
}

int wildcard_expansion(char *pattern, char **args) {
    struct dirent *entry;
    DIR *dir = opendir(".");
    int count = 0;

    if (dir == NULL) {
        perror("opendir failed");
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch(pattern, entry->d_name, 0) == 0) {
            args[count++] = entry->d_name;
        }
    }
    closedir(dir);

    if (count == 0) {
        args[count++] = pattern;  // No matches, pass original token
    }

    return count;
}
