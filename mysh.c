#define _POSIX_C_SOURCE 200809L
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
#include <glob.h>  

#define MAX_ARGS 64
#define MAX_CMD_LEN 1024
#define MAX_PATH_LEN 1024
char prev_cwd[MAX_PATH_LEN];  

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

    if (argc == 1) {
        interactive = 1;  
    }

    if (interactive) {
        printf("Welcome to my Shell.\n");
    }

    if (interactive) {  
        while (1) {
            printf("mysh> ");
            fflush(stdout);  

            ssize_t bytes_read = 0;
            size_t total_read = 0;

    
            while (total_read < MAX_CMD_LEN - 1) {
                bytes_read = read(STDIN_FILENO, input + total_read, MAX_CMD_LEN - total_read - 1);
                
                if (bytes_read < 0) {
                    perror("Error reading input");
                    exit(1);
                }

                if (bytes_read == 0) {
                    break;  
                }

                total_read += bytes_read;

                if (strchr(input, '\n')) {
                    break;  
                }
            }

            input[total_read] = '\0';

            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "exit") == 0) {
                break;
            }

            parse_and_execute(input, interactive);
        }
    } else {  
        struct stat path_stat;
        if (stat(argv[1], &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
            traverse_and_execute(argv[1]);
        } else {
            FILE *file = fopen(argv[1], "r");
            if (!file) {
                perror("Error opening file");
                exit(1);
            }

            while (fgets(input, MAX_CMD_LEN, file)) {
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


/*
This function recursively traverses a directory which is specified by the path, and it opens each file within it. For every file, it then reads the contents 
in a line-by-line format, removes any newline chars from each line, and then moves on to pass every line to the parse_and_execute function which would then execute in batch mode.
Prints out an error message when a file cant be read in and is responsibel for handling one directory at a time. 
*/

void traverse_and_execute(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Error opening directory");
        return;
    }
    struct dirent *entry;
    char filepath[MAX_PATH_LEN];

    while ((entry = readdir(dir))) {
        snprintf(filepath, MAX_PATH_LEN, "%s/%s", path, entry->d_name);
        struct stat path_stat;
        if (stat(filepath, &path_stat) == 0 && S_ISREG(path_stat.st_mode)) {
            FILE *file = fopen(filepath, "r");
            if (!file) {
                perror("Error opening file");
                continue;
            }

            char input[MAX_CMD_LEN];
            while (fgets(input, MAX_CMD_LEN, file)) {
                input[strcspn(input, "\n")] = 0; 
                parse_and_execute(input, 0);  
            }
            fclose(file);
        }
    }

    closedir(dir);
}
/*
This function is responsible for processing input command strings. We store the input string into agruements which gets stored in the args array. 
We utilize built in commands such as cd, pwd, exit and which, and handle them accordingly. For input and output redirection, we call the handle_redirection function.
After command is executed, we restore the fire descriptors back to original for stdin and stdout and close any duplicate file descriptors. 
*/
void parse_and_execute(char *input, int interactive) {
    if (strchr(input, '|')) {
        handle_pipe(input, interactive);
        return;
    }
    char *args[MAX_ARGS];
    int arg_count = 0;

    char *token = strtok(input, " ");
    while (token && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    if (arg_count == 0) return;

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

    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);

    int stdin_fd = -1, stdout_fd = -1;
    handle_redirection(args, &stdin_fd, &stdout_fd);

    execute_command(args, interactive);

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

/*
This function is responsible for executing commands passed in via arguement and handles wildcard and process forking. 
It checks each arg in the args list for wildcard characters and it if is found, then we call the wildcard_expansion function which would then expand this pattern
into matching filenames. 
This function also deals with the process forking aspect utilziing fork(). Inside the child processes, it would attempt to execute the command and if it is unable to,
it returns a specified and detailed error message.
If a child process is exited sucessfully, then we would examine the exit status. If it is non-zero, that would output an error message
If a child process was terminated by a certain signal, then we would output a error message with the signal number that caused this termination. 
*/
void execute_command(char **args, int interactive) {
    char *command = args[0];
    char **expanded_args = NULL;
    int expanded_count = 0;

    for (int i = 0; args[i] != NULL; i++) {
        if (strchr(args[i], '*') || strchr(args[i], '?')) {
            expanded_count = wildcard_expansion(args[i], &expanded_args);
            if (expanded_count > 0) {
                args[i] = NULL; 
                for (int j = 0; j < expanded_count; j++) {
                    args[i++] = expanded_args[j];
                }
                args[i] = NULL; 
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
        execvp(command, args);
        perror("Exec failed");
        exit(1);
    } else {
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


/*
wildcard_expansion would expand a wildcard pattern and match filenames. glob takes a pattern and would try to match it to a filename. 
If glob does end up finding file matches, it then allocates memory for expanded_args which would be responsible for holding the filenames that were expanded. 
It then copies each filename that matches into expanded_args via strdup.
If no matching files, then we assign the original pattern to expanded_args as the sole element
This function also returns the count, which is the # of matching files. Returning 1 if no match is found. 
*/
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

/*
This function handles command pipe executions. 
The function splits the input string into seperate and individual commands and stores each comand into an array and also counts the number of commands
Exits early if less than 2 commands are found and has a specified error message for that
For child processes, we utilize dup2 to redirect input or output for the command.  If not the first command, STDIN gets redirected to prev commands output. If not last command, STDOUT redirects to end of pipe
The current commands are parsed into args and there is various error handling in play for whatever step the process fails. 
*/
void handle_pipe(char *input, int interactive) {
    char *commands[MAX_ARGS];
    int command_count = 0;

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
            if (i > 0) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i < command_count - 1) {
                dup2(pipe_fds[1], STDOUT_FILENO); 
                close(pipe_fds[1]);
            }
            close(pipe_fds[0]); 

            char *args[MAX_ARGS];
            int arg_count = 0;
            char *token = strtok(commands[i], " ");
            while (token && arg_count < MAX_ARGS - 1) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL;

            execvp(args[0], args);
            perror("Exec failed");
            exit(1);
        } else {
       
            wait(NULL);
            close(pipe_fds[1]);
            if (i > 0) {
                close(prev_fd);
            }
            prev_fd = pipe_fds[0];
    }
}
}
/*
This function is responsible for handling input and output redirection.
It holds various error handling for things such as missing files or failing to open. 
We iteratre through the arguements, looking for redirection operators. 
If the output redirector is found, we check the next arguement to see if a filename is provided. If not, we throw a error. We open this file for writing and redirect std out to the file via dup2. 
In the case of input redirection, we open the file for reading and redirect std in to the that file. 
After the handling of redirection, we remove the operator and file from the arguments. 
*/
void handle_redirection(char **args, int *stdin_fd, int *stdout_fd) {
    int i = 0;
    while (args[i] != NULL) {
        if (strcmp(args[i], ">") == 0) {
        
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: No file specified for output redirection\n");
                exit(1);
            }
            *stdout_fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (*stdout_fd < 0) {
                perror("Error opening file for output redirection");
                exit(1);
            }
            dup2(*stdout_fd, STDOUT_FILENO);
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--;
        } else if (strcmp(args[i], "<") == 0) {
            if (args[i + 1] == NULL) {
                fprintf(stderr, "Error: No file specified for input redirection\n");
                exit(1);
            }
            *stdin_fd = open(args[i + 1], O_RDONLY);
            if (*stdin_fd < 0) {
                perror("Error opening file for input redirection");
                exit(1);
            }
            dup2(*stdin_fd, STDIN_FILENO);
            for (int j = i; args[j] != NULL; j++) {
                args[j] = args[j + 2];
            }
            i--;
        }
        i++;
    }
}

void change_directory(char *path) {
    if (chdir(path) == 0) {
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
    char *path_copy = strdup(path); 
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