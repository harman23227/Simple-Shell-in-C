#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <limits.h>

#define MAX_INPUT_LENGTH 2048  // Maximum length for user input
#define MAX_HISTORY 100         // Maximum number of commands to store in history

// Structure to hold command history entries
typedef struct {
    char *command;  // Dynamically allocated command string
    pid_t pid;      // Process ID of the command
    time_t start_time;  // Time when the command started executing
    time_t end_time;    // Time when the command finished executing
} HistoryEntry;

HistoryEntry history[MAX_HISTORY];  // Array to hold history entries
int count_history = 0;               // Current count of history entries

// Function to add a command to the history
void add_to_history(const char *command, pid_t pid, time_t start_time, time_t end_time) {
    // Check if history is full, if so, remove the oldest entry
    if (count_history >= MAX_HISTORY) {
        for (int i = 1; i < count_history; i++) {
            free(history[i - 1].command); // Free memory of the oldest command
            history[i - 1] = history[i];    // Shift entries left
        }
        count_history--; // Reduce count of history
    }

    // Duplicate the command and store the relevant details
    history[count_history].command = strdup(command);
    if (history[count_history].command == NULL) {
        perror("Memory allocation failed for command");
        exit(EXIT_FAILURE);
    }
    history[count_history].pid = pid;
    history[count_history].start_time = start_time;
    history[count_history].end_time = end_time;
    count_history++; // Increase count of history
}

// Function to display the command history
void display_history() {
    printf("-------------------------------\n");
    printf("\n Command History: \n");
    printf("-------------------------------\n");

    // Iterate through each entry and print the details
    for (int i = 0; i < count_history; i++) {
        double duration = difftime(history[i].end_time, history[i].start_time);
        printf("Command: %s\n", history[i].command);
        printf("PID: %d\n", history[i].pid);
        printf("Start Time: %ld\n", history[i].start_time);
        printf("End Time: %ld\n", history[i].end_time);
        printf("Duration: %.2f seconds\n", duration);
        printf("-------------------------------\n");
    }
}

// Signal handler for SIGINT (Ctrl+C)
void my_handler(int sig) {
    if (sig == SIGINT) {
        display_history(); // Show history on interrupt
        exit(1);
    }
}

// Function to create an array of command arguments from a string
char **makecmd(char *str) {
    char **command = NULL;
    char *sep = " \t\n";  // Delimiters for splitting commands
    int command_count = 0; // Counter for number of commands
    int capacity = 10;     // Initial capacity of the command array

    // Allocate initial memory for the command array
    command = (char **)malloc(sizeof(char *) * capacity);
    if (command == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE); 
    }

    // Tokenize the input string based on delimiters
    char *token = strtok(str, sep);
    while (token != NULL) {
        if (strlen(token) > 0) { // Ignore empty tokens
            // Resize the array if necessary
            if (command_count >= capacity) {
                capacity *= 2; 
                char **temp = realloc(command, sizeof(char *) * capacity);
                if (temp == NULL) {
                    perror("Memory reallocation failed");
                    for (int j = 0; j < command_count; j++) {
                        free(command[j]); // Free previously allocated commands
                    }
                    free(command);
                    exit(EXIT_FAILURE);
                }
                command = temp; // Update pointer to resized array
            }

            // Duplicate the token and add to command array
            command[command_count] = strdup(token);
            if (command[command_count] == NULL) {
                perror("Memory allocation failed for command");
                for (int j = 0; j < command_count; j++) {
                    free(command[j]); // Free previously allocated commands
                }
                free(command);
                exit(EXIT_FAILURE); 
            }

            command_count++; // Increment command count
        }
        token = strtok(NULL, sep); // Get next token
    }

    command[command_count] = NULL; // Null-terminate the command array
    return command; // Return the array of command arguments
}

// Function to parse input commands separated by pipes
char **parse(char *str) {
    char **command = NULL;
    char *sep = "|";  // Pipe delimiter
    int command_count = 0; // Counter for number of commands
    int capacity = 10;     // Initial capacity of the command array

    // Allocate initial memory for the command array
    command = (char **)malloc(sizeof(char *) * capacity);
    if (command == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE); 
    }

    // Tokenize the input string based on the pipe delimiter
    char *token = strtok(str, sep);
    while (token != NULL) {
        if (strlen(token) > 0) { // Ignore empty tokens
            // Resize the array if necessary
            if (command_count >= capacity) {
                capacity *= 2; 
                char **temp = realloc(command, sizeof(char *) * capacity);
                if (temp == NULL) {
                    perror("Memory reallocation failed");
                    for (int j = 0; j < command_count; j++) {
                        free(command[j]); // Free previously allocated commands
                    }
                    free(command);
                    exit(EXIT_FAILURE);
                }
                command = temp; // Update pointer to resized array
            }

            // Duplicate the token and add to command array
            command[command_count] = strdup(token);
            if (command[command_count] == NULL) {
                perror("Memory allocation failed for command");
                for (int j = 0; j < command_count; j++) {
                    free(command[j]); // Free previously allocated commands
                }
                free(command);
                exit(EXIT_FAILURE); 
            }

            command_count++; // Increment command count
        }
        token = strtok(NULL, sep); // Get next token
    }

    command[command_count] = NULL; // Null-terminate the command array
    return command; // Return the array of command arguments
}

// Function to execute a single command with input/output redirection
void executeCommand(char *command, int inputfd, int outputfd) {
    // Duplicate the command string for processing
    char *trimmed_command = strdup(command);
    if (trimmed_command == NULL) {
        perror("Failed to duplicate command");
        exit(EXIT_FAILURE);
    }

    int and_flag = 0; // Flag to check if the command should run in the background
    char *ampersand_pos = strchr(trimmed_command, '&'); // Check for '&' character
    if (ampersand_pos != NULL) {
        *ampersand_pos = '\0'; // Terminate command before '&'
        and_flag = 1; // Set background flag
    }

    // Trim trailing spaces from the command
    size_t len = strlen(trimmed_command);
    while (len > 0 && (trimmed_command[len - 1] == ' ' || trimmed_command[len - 1] == '\t')) {
        trimmed_command[--len] = '\0';
    }

    time_t start_time = time(NULL); // Record start time of command
    int pid = fork(); // Create a new process
    if (pid < 0) {
        perror("Forking child failed");
        free(trimmed_command);
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // Child process
        // Redirect input/output as necessary
        if (inputfd != STDIN_FILENO) {
            dup2(inputfd, STDIN_FILENO);
            close(inputfd);
        }
        if (outputfd != STDOUT_FILENO) {
            dup2(outputfd, STDOUT_FILENO);
            close(outputfd);
        }

        // Handle background processes
        if (and_flag) {
            int null_fd = open("/dev/null", O_WRONLY);
            if (null_fd < 0) {
                perror("Failed to open /dev/null");
                exit(EXIT_FAILURE);
            }
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
            signal(SIGHUP, SIG_IGN); // Ignore hangup signal
        }

        // Parse the command into arguments
        char **cmd_PARSED = makecmd(trimmed_command);
        if (cmd_PARSED == NULL) {
            perror("Failed to parse command");
            exit(EXIT_FAILURE);
        }

        execvp(cmd_PARSED[0], cmd_PARSED); // Execute the command
        perror("exec failed"); // If exec fails, print error
        exit(EXIT_FAILURE);
    }

    free(trimmed_command); // Free duplicated command string

    // Handle waiting for foreground commands
    if (!and_flag) {
        int status;
        waitpid(pid, &status, 0); // Wait for the command to finish

        time_t end_time = time(NULL); // Record end time
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            // Only add to history if command was successful
            add_to_history(command, pid, start_time, end_time);
        }
    } else {
        // For background commands, add to history immediately
        time_t end_time = time(NULL);
        add_to_history(command, pid, start_time, end_time);
        printf("Started background process with PID: %d\n", pid);
    }
}

// Function to execute a series of commands
void executecommands(char **commands) {
    int i = 0; // Command index
    int fd[2]; // Pipe file descriptors
    int inputfd = STDIN_FILENO; // Input file descriptor for the first command

    while (commands[i] != NULL) {
        // Create a pipe if there are more commands to follow
        if (commands[i + 1] != NULL) {
            if (pipe(fd) == -1) {
                perror("Pipe creation failed");
                exit(EXIT_FAILURE);
            }
        } else {
            fd[1] = STDOUT_FILENO; // Last command writes to stdout
        }

        // Execute the current command
        executeCommand(commands[i], inputfd, fd[1]);

        // Close the write end of the pipe
        if (commands[i + 1] != NULL) {
            if (close(fd[1]) == -1) {
                perror("Failed to close write end of pipe");
            }
        }
        
        // Close the read end of the previous command
        if (inputfd != STDIN_FILENO) {
            if (close(inputfd) == -1) {
                perror("Failed to close read end of previous command");
            }
        }

        inputfd = fd[0]; // Update inputfd for the next command
        i++;
    }

    // Wait for all child processes to finish
    while (wait(NULL) > 0);
}

// Function to clean up memory allocated for command history
void cleanup() {
    for (int i = 0; i < count_history; i++) {
        free(history[i].command); // Free each command string in history
    }
}

// Main function
int main() {
    struct sigaction sig; // Structure for handling signals
    memset(&sig, 0, sizeof(sig)); // Zero out the structure
    sig.sa_handler = my_handler; // Set handler for SIGINT
    if (sigaction(SIGINT, &sig, NULL) != 0) {
        perror("Signal handling failed");
        exit(EXIT_FAILURE);
    }

    int run = 1; // Control variable for the main loop
    char user[LOGIN_NAME_MAX]; // Array to hold username
    char host[HOST_NAME_MAX]; // Array to hold hostname
    char *input = malloc(MAX_INPUT_LENGTH); // Allocate memory for user input
    if (input == NULL) {
        perror("Failed to allocate memory for input");
        exit(EXIT_FAILURE);
    }

    // Main loop for the shell
    do {
        // Get the username and hostname
        if (getlogin_r(user, sizeof(user)) != 0 || gethostname(host, sizeof(host)) != 0) {
            perror("Error: Unable to get host name or username");
            run = 0;
            continue;
        }

        char *cd = malloc(PATH_MAX); // Allocate memory for current directory
        if (cd == NULL) {
            perror("Error: Unable to allocate memory for current working directory");
            run = 0;
            continue;
        }

        // Get the current working directory
        if (getcwd(cd, PATH_MAX) == NULL) {
            perror("Error: Unable to get current working directory");
            free(cd);
            run = 0;
            continue;
        }

        // Print the shell prompt
        printf("\033[32m%s\033[0m", user);
        printf("\033[31m@%s\033[0m", host);
        printf(":\033[34m%s\033[0m\n", cd);
        printf("\033[1m$\033[0m ");

        // Read user input
        if (!fgets(input, MAX_INPUT_LENGTH, stdin)) {
            perror("Error reading input");
            free(cd);
            continue;
        }
        // Check for exit command
        if (strcmp(input, "exit\n") == 0) {
            free(cd);
            run = 0;
            continue;
        }
        // Check for history command
        if (strcmp(input, "history\n") == 0) {
            display_history();
            free(cd);
            continue;
        }
        
        // Duplicate the input string for parsing
        char *input_copy = strdup(input);
        if (!input_copy) {
            perror("Failed to duplicate input string");
            free(cd);
            continue;
        }
        
        // Parse the input into commands
        char **result = parse(input_copy);
        executecommands(result); // Execute the parsed commands
        
        free(input_copy); // Free the duplicated input string
        free(cd); // Free the allocated current directory memory
        printf("\n");
    } while (run);

    free(input); // Free the allocated memory for user input
    cleanup(); // Clean up history before exiting
    return 0; // Exit the program
}

