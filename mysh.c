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
#include <glob.h>  // For wildcard expansion

#define MAX_ARGS 64
#define MAX_CMD_LEN 1024
#define MAX_PATH_LEN 1024
char prev_cwd[MAX_PATH_LEN];  // To store the previous current working directory

void execute_command(char **args, int interactive);
void change_directory(char *path);
void print_working_directory();
void handle_exit(char **args);
void handle_which(char **args);
void handle_pipe(char *input, int interactive);
int wildcard_expansion(char *pattern, char ***expanded_args);
void free_args(char **args, int count);
void parse_and_execute(char *input, int interactive);
void traverse_and_execute(const char *path);
void handle_redirection(char **args, int *stdin_fd, int *stdout_fd);

int main(int argc, char **argv) {
    char input[MAX_CMD_LEN];
    int interactive = 0;

    // Check if we're in batch mode (argc > 1 means we have a file/argument)
    if (argc == 1) {
        interactive = 1;  // No arguments means it's interactive
    }

    // Print welcome message only in interactive mode
    if (interactive) {
        printf("Welcome to my Shell.\n");
    }

    if (interactive) {  // Interactive mode
        while (1) {
            // Print the prompt
            printf("mysh> ");
            fflush(stdout);  // Ensure the prompt is printed immediately

            ssize_t bytes_read = 0;
            size_t total_read = 0;

            // Use read() to obtain input (the input can be longer than one read call)
            while (total_read < MAX_CMD_LEN - 1) {
                bytes_read = read(STDIN_FILENO, input + total_read, MAX_CMD_LEN - total_read - 1);
                
                if (bytes_read < 0) {
                    perror("Error reading input");
                    exit(1);
                }

                if (bytes_read == 0) {
                    break;  // End of input (EOF)
                }

                total_read += bytes_read;

                // Check if newline is encountered
                if (strchr(input, '\n')) {
                    break;  // Exit loop when a newline is encountered
                }
            }

            // Null-terminate the input string
            input[total_read] = '\0';

            // Remove trailing newline if it exists
            input[strcspn(input, "\n")] = 0;

            // Exit if user types "exit"
            if (strcmp(input, "exit") == 0) {
                break;
            }

            parse_and_execute(input, interactive);
        }
    } else {  // Batch mode: We assume the first argument is a file
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
                parse_and_execute(input, 0);  // Process command in batch mode
            }

            fclose(file);
        }
    }

    // Exit message only in interactive mode
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
    if (strchr(input, '|')) {
        handle_pipe(input, interactive);
        return;
    }
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
        return;
    }
    if (strcmp(args[0], "which") == 0) {
        handle_which(args);
        return;
    }

    // Save original file descriptors
    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);

    // Handle redirection
    int stdin_fd = -1, stdout_fd = -1;
    handle_redirection(args, &stdin_fd, &stdout_fd);

    // Execute the command
    execute_command(args, interactive);

    // Restore original stdin and stdout
    if (stdin_fd >= 0) {
        dup2(original_stdin, STDIN_FILENO);
        close(stdin_fd);
    }
    if (stdout_fd >= 0) {
        dup2(original_stdout, STDOUT_FILENO);
        close(stdout_fd);
    }
    close(original_stdin);
    close(original_stdout);
}


void execute_command(char **args, int interactive) {
    char *command = args[0];
    char **expanded_args = NULL;
    int expanded_count = 0;

    // Check for wildcard in the arguments
    for (int i = 0; args[i] != NULL; i++) {
        if (strchr(args[i], '*') || strchr(args[i], '?')) {
            expanded_count = wildcard_expansion(args[i], &expanded_args);
            if (expanded_count > 0) {
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

        if (expanded_count > 0) {
            free_args(expanded_args, expanded_count);
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

int wildcard_expansion(char *pattern, char ***expanded_args) {
    glob_t globbuf;
    int i, count = 0;

    if (glob(pattern, 0, NULL, &globbuf) == 0) {
        *expanded_args = malloc(globbuf.gl_pathc * sizeof(char *));
        for (i = 0; i < globbuf.gl_pathc; i++) {
            (*expanded_args)[count++] = strdup(globbuf.gl_pathv[i]);
        }
    } else {
        *expanded_args = malloc(sizeof(char *));
        (*expanded_args)[count++] = strdup(pattern);
    }

    globfree(&globbuf);  
    return count;
}

void handle_pipe(char *input, int interactive) {
    char *commands[MAX_ARGS];
    int command_count = 0;

    // Split the input into individual commands by '|'
    char *command = strtok(input, "|");
    while (command && command_count < MAX_ARGS - 1) {
        commands[command_count++] = command;
        command = strtok(NULL, "|");
    }
    commands[command_count] = NULL;

    if (command_count < 2) {
        fprintf(stderr, "Error: Invalid pipe syntax\n");
        return;
    }

    int pipe_fds[2], prev_fd = STDIN_FILENO;
    for (int i = 0; i < command_count; i++) {
        if (pipe(pipe_fds) < 0) {
            perror("Pipe failed");
            exit(1);
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }

        if (pid == 0) {
            // Child process
            if (i > 0) {
                dup2(prev_fd, STDIN_FILENO); // Read from the previous command's output
                close(prev_fd);
            }
            if (i < command_count - 1) {
                dup2(pipe_fds[1], STDOUT_FILENO); // Write to the next command's input
                close(pipe_fds[1]);
            }
            close(pipe_fds[0]); // Close unused read end of the pipe

            // Parse the command and arguments
            char *args[MAX_ARGS];
            int arg_count = 0;
            char *token = strtok(commands[i], " ");
            while (token && arg_count < MAX_ARGS - 1) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL;

            // Execute the command
            execvp(args[0], args);
            perror("Exec failed");
            exit(1);
        } else {
            // Parent process
            wait(NULL); // Wait for the child to finish
            close(pipe_fds[1]); // Close unused write end of the pipe
            if (i > 0) {
                close(prev_fd); // Close the previous read end
            }
            prev_fd = pipe_fds[0]; // Save the current read end for the next iteration
        }
    }
}

void handle_redirection(char **args, int *stdin_fd, int *stdout_fd) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
            // Output redirection
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: No file specified for output redirection\n");
                exit(1);
            }
            *stdout_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (*stdout_fd < 0) {
                perror("Error opening file for output redirection");
                exit(1);
            }
            dup2(*stdout_fd, STDOUT_FILENO);  // Redirect stdout to the file
            // Remove redirection operator and filename from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--; // Re-check the current index after removal
        } else if (strcmp(args[i], "<") == 0) {
            // Input redirection
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: No file specified for input redirection\n");
                exit(1);
            }
            *stdin_fd = open(args[i + 1], O_RDONLY);
            if (*stdin_fd < 0) {
                perror("Error opening file for input redirection");
                exit(1);
            }
            dup2(*stdin_fd, STDIN_FILENO);  // Redirect stdin to the file
            // Remove redirection operator and filename from args
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--; // Re-check the current index after removal
        }
        i++;
    }
}

void change_directory(char *path) {
    if (chdir(path) == 0) {
        // Store the previous cwd before changing it
        getcwd(prev_cwd, MAX_PATH_LEN);
    } else {
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
    exit(0);
}

void handle_which(char **args) {
    if (args[1] == NULL) {
        return;
    }

    char *command = args[1];
    char *path = getenv("PATH");
    char *path_copy = strdup(path); // Create a copy of the PATH to avoid modifying the original
    char *token = strtok(path_copy, ":");

    while (token != NULL) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", token, command);

        if (access(full_path, F_OK) == 0) {
            printf("%s\n", full_path);
           
            return;
        }

        token = strtok(NULL, ":");
    }
}

void free_args(char **args, int count) {
    for (int i = 0; i < count; i++) {
        free(args[i]);
    }
}