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
char prev_cwd[MAX_PATH_LEN];  // To store the previous current working directory



void execute_command(char **args);
void change_directory(char *path);
void print_working_directory();
void handle_exit(char **args);
void handle_which(char **args);
int wildcard_expansion(char *pattern, char **args);
void free_args(char **args, int count);
void parse_and_execute(char *input, int interactive);
void traverse_and_execute(const char *path);
void handle_redirection(char **args, int *stdin_fd, int *stdout_fd, int *stderr_fd);

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
        perror("Error opening directory");
        return;
    }
    struct dirent *entry;
    char filepath[MAX_PATH_LEN];
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            snprintf(filepath, MAX_PATH_LEN, "%s/%s", path, entry->d_name);
            FILE *file = fopen(filepath, "r");
            if (!file) {
                perror("Error opening file");
                continue;
            }
            char input[MAX_CMD_LEN];
            while (fgets(input, MAX_CMD_LEN, file)) {
                input[strcspn(input, "\n")] = 0; // Remove newline
                parse_and_execute(input, 0);    // Process command in batch mode
            }
            fclose(file);
        }
    }
    closedir(dir);
}

void parse_and_execute(char *input, int interactive) {
    char *args[MAX_ARGS];
    int arg_count = 0;

    // Tokenize input into arguments
    char *token = strtok(input, " ");
    while (token && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    // Handle empty input
    if (arg_count == 0) return;

    // Handle built-in commands
    if (strcmp(args[0], "cd") == 0) {
        if (arg_count != 2) {
            fprintf(stderr, "cd: Invalid number of arguments\n");
        } else {
            change_directory(args[1]);
        }
        return;
    }
    if (strcmp(args[0], "pwd") == 0) {
        print_working_directory();
        return;
    }
    if (strcmp(args[0], "exit") == 0) {
        handle_exit(args);
    }
    if (strcmp(args[0], "which") == 0) {
        handle_which(args);
        return;
    }

    // Handle external commands
    int stdin_fd = -1, stdout_fd = -1, stderr_fd = -1;
    handle_redirection(args, &stdin_fd, &stdout_fd, &stderr_fd);
    execute_command(args);

    // Restore original stdin, stdout, and stderr
    if (stdin_fd >= 0) dup2(stdin_fd, STDIN_FILENO);
    if (stdout_fd >= 0) dup2(stdout_fd, STDOUT_FILENO);
    if (stderr_fd >= 0) dup2(stderr_fd, STDERR_FILENO);
}

void execute_command(char **args) {
    char *command = args[0];
    int wildcard_expanded = 0;
    char *expanded_args[MAX_ARGS];

    // Check for wildcard in the arguments
    for (int i = 0; args[i] != NULL; i++) {
        if (strchr(args[i], '*') || strchr(args[i], '?')) {
            int expanded_count = wildcard_expansion(args[i], expanded_args);
            if (expanded_count > 0) {
                wildcard_expanded = 1;
                args[i] = NULL; // Replace the wildcard pattern with the expanded args
                for (int j = 0; j < expanded_count; j++) {
                    args[i++] = expanded_args[j];
                }
                args[i] = NULL; // Null-terminate the updated argument list
                break;
            }
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    if (pid == 0) {
        // Child process
        execvp(command, args);
        perror("Exec failed");
        exit(1);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (wildcard_expanded) {
            free_args(expanded_args, MAX_ARGS);
        }

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                fprintf(stderr, "Command failed with exit code: %d\n", WEXITSTATUS(status));
            }
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "Terminated by signal: %d\n", WTERMSIG(status));
        }
    }
}

void handle_redirection(char **args, int *stdin_fd, int *stdout_fd, int *stderr_fd) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            *stdout_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (*stdout_fd < 0) {
                perror("Error opening file for redirection");
                exit(1);
            }
            dup2(*stdout_fd, STDOUT_FILENO);
            args[i] = NULL; // Remove redirection part
            break;
        } else if (strcmp(args[i], "<") == 0) {
            *stdin_fd = open(args[i + 1], O_RDONLY);
            if (*stdin_fd < 0) {
                perror("Error opening file for redirection");
                exit(1);
            }
            dup2(*stdin_fd, STDIN_FILENO);
            args[i] = NULL; // Remove redirection part
            break;
        }
    }
}

void change_directory(char *path) {
    char cwd[MAX_PATH_LEN];

    if (path == NULL) {
        fprintf(stderr, "cd: Missing argument\n");
        return;
    }

    if (strcmp(path, "-") == 0) {
        // If the user types "cd -", switch back to the previous directory
        if (prev_cwd[0] == '\0') {
            fprintf(stderr, "cd: No previous directory\n");
        } else {
            if (chdir(prev_cwd) != 0) {
                perror("cd failed");
            } else {
                // Update prev_cwd to the current directory
                printf("%s\n", prev_cwd);
            }
        }
    } else {
        // Store the current directory before changing
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            strncpy(prev_cwd, cwd, sizeof(prev_cwd) - 1);  // Store the current directory in prev_cwd
        }

        // Attempt to change to the desired directory
        if (chdir(path) != 0) {
            perror("cd failed");
        }
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

void free_args(char **args, int count) {
    for (int i = 0; i < count; i++) {
        free(args[i]);
    }
}

int wildcard_expansion(char *pattern, char **args) {
    DIR *dir = opendir(".");
    if (!dir) {
        perror("Error opening directory");
        return 0;
    }

    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(dir))) {
        if (fnmatch(pattern, entry->d_name, 0) == 0) {
            args[count] = strdup(entry->d_name); // Dynamically allocate and add file to args
            if (!args[count]) {
                perror("Memory allocation error");
                free_args(args, count);
                closedir(dir);
                return 0;
            }
            count++;
        }
    }
    closedir(dir);
    args[count] = NULL; // Null-terminate the arguments
    return count;
}