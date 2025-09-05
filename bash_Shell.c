#include<stdio.h>
#include<unistd.h>
#include<stdlib.h> 
#include<fcntl.h>
#include<string.h> 
#include<sys/signal.h>
#include<sys/wait.h>
#include<ctype.h>

#define MAX_CMD_SIZE 100
#define MAX_PARAMETERS 5
#define MAX_BACKGROUND_TASKS 4

pid_t background_task_ids[MAX_BACKGROUND_TASKS]; // Array to track background process IDs
int active_background_count = 0; // Counter for active background processes

// ======== FORWARD DECLARATIONS ======== //
char* get_user_command();
void remove_whitespace(char *text);
void run_single_command(char *cmd_text, pid_t process_id);
int validate_argument_count(char *cmd_text);
void run_command_direct(char *cmd_text);
int create_new_terminal();
void handle_piped_commands(char *cmd_text);
void process_word_counter(char *cmd_text);
void process_file_concatenation(char *cmd_text);
void process_mutual_file_append(char *cmd_text);
void handle_reversed_pipe(char *cmd_text);
void manage_background_execution(char *cmd_text);
void handle_input_redirection(char *cmd_text);
void run_sequential_commands(char *cmd_text);
void handle_conditional_execution(char *cmd_text);
void handle_output_redirection(char *cmd_text, char *redirect_type);

// ======== CORE FUNCTIONS ======== //

char* get_user_command() {
    char *user_input = malloc(150 * sizeof(char)); // Allocate memory for command input
    printf("mbash25$"); // Display custom shell prompt
    fgets(user_input, 150, stdin); // Read user input from stdin
    user_input[strcspn(user_input, "\n")] = '\0'; // Remove newline character
    return user_input; // Return the cleaned command string
}

void remove_whitespace(char *text) {
    if (!text || !*text) return; // Handle null or empty strings
    
    char *beginning = text; // Find start of non-whitespace content
    while (*beginning && isspace((unsigned char)*beginning)) 
        beginning++;
    
    if (!*beginning) { // String is all whitespace
        *text = '\0';
        return;
    }

    char *ending = beginning + strlen(beginning) - 1; // Find end of non-whitespace content
    while (ending > beginning && isspace((unsigned char)*ending)) 
        ending--;
    
    *(ending + 1) = '\0'; // Null-terminate the trimmed string
    
    if (beginning != text) // Move trimmed content to start of buffer
        memmove(text, beginning, ending - beginning + 2);
}

void run_single_command(char *cmd_text, pid_t process_id) {
    if (process_id < 0) { // Fork failed
        perror("Fork error");
        exit(0);
    } else if (process_id == 0) { // Child process execution
        char *parameters[MAX_PARAMETERS]; // Array to hold command arguments
        char *current_token = strtok(cmd_text, " "); // Tokenize command by spaces
        int param_index = 0;
        while (current_token && param_index < MAX_PARAMETERS - 1) {
            parameters[param_index++] = current_token; // Store each argument
            current_token = strtok(NULL, " ");
        }
        parameters[param_index] = NULL; // Null-terminate argument array
        
        if (execvp(parameters[0], parameters) == -1) { // Execute command
            perror("Execution failed");
            exit(EXIT_FAILURE);
        }
    } else { // Parent process waits for child completion
        wait(NULL);
    }
}

int validate_argument_count(char *cmd_text) {
    char temp_buffer[100]; // Temporary buffer for tokenization
    strcpy(temp_buffer, cmd_text);
    char *parameters[20];
    char *current_token = strtok(temp_buffer, " ");
    int param_index = 0;
    
    while (current_token) { // Count command arguments
        if (param_index >= MAX_PARAMETERS) return -1; // Too many parameters
        parameters[param_index++] = current_token;
        current_token = strtok(NULL, " ");
    }
    return (param_index < 1) ? -1 : 0; // Return error if no command found
}

void run_command_direct(char *cmd_text) {
    char *parameters[MAX_PARAMETERS]; // Prepare argument array
    char *current_token = strtok(cmd_text, " ");
    int param_index = 0;
    
    while (current_token && param_index < MAX_PARAMETERS - 1) {
        parameters[param_index++] = current_token; // Parse command arguments
        current_token = strtok(NULL, " ");
    }
    parameters[param_index] = NULL; // Null-terminate for exec
    
    if (execvp(parameters[0], parameters) == -1) { // Direct execution without fork
        perror("Execution failed");
        exit(EXIT_FAILURE);
    }
}

int create_new_terminal() {
    char *terminal_args[] = {"xterm", "-e", "./mbash25", NULL}; // Terminal launch arguments
    pid_t child_process = fork();    
    if (child_process == 0) { // Child launches new terminal
        execvp("xterm", terminal_args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else if (child_process == -1) { // Fork failed
        perror("Fork error");
        return -1;
    }
    return 0; // Success
}

// ======== PIPING IMPLEMENTATION ======== //

void handle_piped_commands(char *cmd_text) {
    int total_pipes = -1; // Counter for number of pipes needed
    char *individual_commands[20]; // Array to store individual commands
    char *current_token = strtok(cmd_text, "|"); // Split by pipe character
    int cmd_index = 0;
    while (current_token) {
        individual_commands[cmd_index++] = current_token; // Store each command
        total_pipes++; // Count pipes (commands - 1)
        current_token = strtok(NULL, "|");
    }

    int original_stdin = dup(STDIN_FILENO); // Save original stdin/stdout
    int original_stdout = dup(STDOUT_FILENO);
    int main_shell_pid = getpid(); // Get parent process ID
    int pipe_descriptors[total_pipes][2]; // Array of pipe file descriptors
    int total_commands = total_pipes + 1;

    // Create all required pipes
    for(int pipe_counter = 0; pipe_counter < total_pipes; pipe_counter++) {
        if(main_shell_pid == getpid()) { // Only parent creates pipes
            int pipe_fd[2];
            pipe(pipe_fd); // Create pipe
            pipe_descriptors[pipe_counter][0] = pipe_fd[0]; // Read end
            pipe_descriptors[pipe_counter][1] = pipe_fd[1]; // Write end
        }
    }

    int pipe_index = 0;
    for(int cmd_counter = 0; cmd_counter < total_commands; cmd_counter++) {
        if(validate_argument_count(individual_commands[cmd_counter]) == -1) {
            printf("Invalid argument count\n");
            return;
        }
        
        pid_t process_id = fork(); // Fork for each command
        if (process_id == 0) { // Child process setup
            // First command - output to first pipe
            if(cmd_counter == 0) {
                dup2(pipe_descriptors[pipe_index][1], STDOUT_FILENO);
                for (int fd_index = 0; fd_index < total_pipes; fd_index++) {
                    close(pipe_descriptors[fd_index][0]); // Close unused read ends
                    if(fd_index != pipe_index) close(pipe_descriptors[fd_index][1]);
                }
            }
            // Last command - input from last pipe
            else if(cmd_counter == total_commands-1) {
                dup2(pipe_descriptors[pipe_index-1][0], STDIN_FILENO);
                for (int fd_index = 0; fd_index < total_pipes; fd_index++) {
                    close(pipe_descriptors[fd_index][1]); // Close unused write ends
                    if(fd_index != pipe_index-1) close(pipe_descriptors[fd_index][0]);
                }
            }
            // Middle commands - input from previous, output to next pipe
            else {
                dup2(pipe_descriptors[pipe_index-1][0], STDIN_FILENO);
                dup2(pipe_descriptors[pipe_index][1], STDOUT_FILENO);
                for (int fd_index = 0; fd_index < total_pipes; fd_index++) {
                    if(fd_index != pipe_index-1) close(pipe_descriptors[fd_index][0]);
                    if(fd_index != pipe_index) close(pipe_descriptors[fd_index][1]);
                }
            }
            
            run_command_direct(individual_commands[cmd_counter]); // Execute command
        }
        pipe_index++;
    }

    // Close all pipes in parent process
    for (int fd_index = 0; fd_index < total_pipes; fd_index++) {
        close(pipe_descriptors[fd_index][0]);
        close(pipe_descriptors[fd_index][1]);
    }

    // Wait for all child processes to complete
    for (int cmd_counter = 0; cmd_counter < total_commands; cmd_counter++) {
        wait(NULL);
    }

    // Restore original stdin/stdout
    dup2(original_stdin, STDIN_FILENO);
    dup2(original_stdout, STDOUT_FILENO);
    close(original_stdin);
    close(original_stdout);
}

// ======== NEW FUNCTIONALITY ======== //

void process_word_counter(char *cmd_text) {
    char *file_list[5]; // Array to store file names
    char *current_token = strtok(cmd_text, " ");
    int file_index = 0;
    
    while (current_token) {
        if (strcmp(current_token, "#") != 0) // Skip the '#' operator
            file_list[file_index++] = current_token;
        current_token = strtok(NULL, " ");
    }
    file_list[file_index] = NULL;
    
    if (file_index > 4 || file_index < 1) { // Validate file count
        printf("Invalid file count (1-4 allowed)\n");
        return;
    }
    
    for (int file_counter = 0; file_counter < file_index; file_counter++) {
        pid_t process_id = fork();
        if (process_id == 0) { // Child executes wc for each file
            execlp("wc", "wc", "-w", file_list[file_counter], NULL);
            perror("wc failed");
            exit(EXIT_FAILURE);
        } else {
            wait(NULL); // Wait for word count completion
        }
    }
}

void process_file_concatenation(char *cmd_text) {
    char *file_list[6]; // Array to store file names for concatenation
    char *current_token = strtok(cmd_text, " ");
    int file_index = 0;
    
    while (current_token) {
        if (strcmp(current_token, "++") != 0) // Skip the '++' operator
            file_list[file_index++] = current_token;
        current_token = strtok(NULL, " ");
    }
    file_list[file_index] = NULL;
    
    if (file_index > 5 || file_index < 2) { // Validate file count
        printf("Invalid file count (2-5 allowed)\n");
        return;
    }
    
    pid_t process_id = fork();
    if (process_id == 0) { // Child executes cat with all files
        execvp("cat", file_list);
        perror("cat failed");
        exit(EXIT_FAILURE);
    } else {
        wait(NULL); // Wait for concatenation completion
    }
}

void process_mutual_file_append(char *cmd_text) {
    char *file_list[3]; // Array for exactly two files
    char *current_token = strtok(cmd_text, " ");
    int file_index = 0;
    
    while (current_token && file_index < 2) {
        if (strcmp(current_token, "+") != 0) // Skip the '+' operator
            file_list[file_index++] = current_token;
        current_token = strtok(NULL, " ");
    }
    file_list[2] = NULL;
    
    if (file_index != 2) { // Exactly two files required
        printf("Exactly two files required\n");
        return;
    }
    
    pid_t process_id = fork();
    if (process_id == 0) { // Child handles file operations
        
        // Step 1: Append file2 to file1
        FILE *second_file = fopen(file_list[1], "r");
        if (!second_file) {
            perror("Open failed");
            exit(EXIT_FAILURE);
        }
        
        FILE *first_file_append = fopen(file_list[0], "a");
        if (!first_file_append) {
            perror("Open failed");
            fclose(second_file);
            exit(EXIT_FAILURE);
        }
        
        char data_buffer[4096]; // Buffer for file content transfer
        size_t read_bytes;
        while ((read_bytes = fread(data_buffer, 1, sizeof(data_buffer), second_file)) > 0) {
            fwrite(data_buffer, 1, read_bytes, first_file_append); // Copy data
        }
        
        fclose(second_file);
        fclose(first_file_append);
        
        // Step 2: Append updated file1 to file2
        FILE *first_file_read = fopen(file_list[0], "r");
        if (!first_file_read) {
            perror("Open failed");
            exit(EXIT_FAILURE);
        }
        
        FILE *second_file_append = fopen(file_list[1], "a");
        if (!second_file_append) {
            perror("Open failed");
            fclose(first_file_read);
            exit(EXIT_FAILURE);
        }
        
        while ((read_bytes = fread(data_buffer, 1, sizeof(data_buffer), first_file_read)) > 0) {
            fwrite(data_buffer, 1, read_bytes, second_file_append); // Copy data back
        }
        
        fclose(first_file_read);
        fclose(second_file_append);
        
        printf("Mutually appended %s and %s\n", file_list[0], file_list[1]);
        exit(EXIT_SUCCESS);
    } else if (process_id > 0) {
        wait(NULL); // Wait for mutual append completion
    } else {
        perror("Fork failed");
    }
}

void handle_reversed_pipe(char *cmd_text) {
    char *command_list[5]; // Array to store commands
    char *current_token = strtok(cmd_text, "~"); // Split by tilde operator
    int command_count = 0;
    
    while (current_token && command_count < 4) {
        remove_whitespace(current_token); // Clean whitespace
        command_list[command_count++] = current_token;
        current_token = strtok(NULL, "~");
    }
    command_list[command_count] = NULL;
    
    if (command_count < 2) { // Need at least two commands
        printf("At least two commands required\n");
        return;
    }
    
    // Reverse command order for execution
    for (int reverse_index = 0; reverse_index < command_count / 2; reverse_index++) {
        char *temp_cmd = command_list[reverse_index];
        command_list[reverse_index] = command_list[command_count - 1 - reverse_index];
        command_list[command_count - 1 - reverse_index] = temp_cmd;
    }
    
    // Build pipe command string from reversed commands
    char piped_command[150] = "";
    for (int build_index = 0; build_index < command_count; build_index++) {
        strcat(piped_command, command_list[build_index]);
        if (build_index < command_count - 1) strcat(piped_command, "|");
    }
    
    handle_piped_commands(piped_command); // Execute as normal pipe
}

// ======== BACKGROUND PROCESSES ======== //

void manage_background_execution(char *cmd_text) {
    if (strcmp(cmd_text, "fg") == 0) { // Foreground command
        if (active_background_count > 0) {
            pid_t process_id = background_task_ids[--active_background_count]; // Get last background process
            waitpid(process_id, NULL, 0); // Bring to foreground
            printf("Process %d brought to foreground\n", process_id);
        } else {
            printf("No background processes\n");
        }
        return;
    }
    
    char *command_list[5]; // Array for background commands
    char *current_token = strtok(cmd_text, "&"); // Split by ampersand
    int command_count = 0;
    
    while (current_token && command_count < 4) {
        remove_whitespace(current_token); // Clean whitespace
        if (*current_token) command_list[command_count++] = current_token;
        current_token = strtok(NULL, "&");
    }
    
    for (int cmd_index = 0; cmd_index < command_count; cmd_index++) {
        if (active_background_count >= MAX_BACKGROUND_TASKS) { // Check limit
            printf("Background process limit reached\n");
            return;
        }
        
        pid_t process_id = fork();
        if (process_id == 0) { // Child becomes background process
            setpgid(0, 0); // Set new process group
            run_command_direct(command_list[cmd_index]);
        } else if (process_id > 0) {
            background_task_ids[active_background_count++] = process_id; // Track process
            printf("Started background process %d: %s\n", process_id, command_list[cmd_index]);
        } else {
            perror("Fork failed");
        }
    }
}

// ======== REDIRECTION ======== //

void handle_input_redirection(char *cmd_text) {
    char *input_file; // File to read from
    char *command_part; // Command to execute
    int saved_stdin = dup(STDIN_FILENO); // Save original stdin
    
    char *current_token = strtok(cmd_text, "<"); // Split by input redirection
    command_part = current_token;
    remove_whitespace(command_part);
    current_token = strtok(NULL, "<");
    input_file = current_token;
    remove_whitespace(input_file);
    
    pid_t process_id = fork();
    if (process_id == 0) { // Child handles redirection
        int file_descriptor = open(input_file, O_RDONLY); // Open input file
        if (file_descriptor < 0) {
            perror("Open failed");
            exit(EXIT_FAILURE);
        }
        dup2(file_descriptor, STDIN_FILENO); // Redirect stdin to file
        close(file_descriptor);
        run_command_direct(command_part); // Execute command
    } else if (process_id > 0) {
        waitpid(process_id, NULL, 0); // Wait for completion
        dup2(saved_stdin, STDIN_FILENO); // Restore stdin
        close(saved_stdin);
    }
}

void handle_output_redirection(char *cmd_text, char *redirect_type) {
    char *output_file; // File to write to
    char *command_part; // Command to execute
    int saved_stdout = dup(STDOUT_FILENO); // Save original stdout
    
    if(strcmp(redirect_type,">")==0) { // Standard output redirection
        char *current_token = strtok(cmd_text, ">");
        command_part = current_token;
        remove_whitespace(command_part);
        current_token = strtok(NULL, ">");
        output_file = current_token;
        remove_whitespace(output_file);
        
        pid_t process_id = fork();
        if(process_id==0) { // Child handles redirection
            int file_descriptor = open(output_file, O_CREAT|O_WRONLY|O_TRUNC, 0644); // Create/truncate file
            dup2(file_descriptor, STDOUT_FILENO); // Redirect stdout to file
            close(file_descriptor);
            run_command_direct(command_part);
        } else {
            waitpid(process_id, NULL, 0); // Wait for completion
            dup2(saved_stdout, STDOUT_FILENO); // Restore stdout
            close(saved_stdout);
        }
    }
    else if(strcmp(redirect_type,">>")==0) { // Append output redirection
        char *current_token = strtok(cmd_text, ">>");
        command_part = current_token;
        remove_whitespace(command_part);
        current_token = strtok(NULL, ">>");
        output_file = current_token;
        remove_whitespace(output_file);
        
        pid_t process_id = fork();
        if(process_id==0) { // Child handles append redirection
            int file_descriptor = open(output_file, O_CREAT|O_WRONLY|O_APPEND, 0644); // Create/append to file
            dup2(file_descriptor, STDOUT_FILENO); // Redirect stdout to file
            close(file_descriptor);
            run_command_direct(command_part);
        } else {
            waitpid(process_id, NULL, 0); // Wait for completion
            dup2(saved_stdout, STDOUT_FILENO); // Restore stdout
            close(saved_stdout);
        }
    }
}

// ======== SEQUENTIAL EXECUTION ======== //

void run_sequential_commands(char *cmd_text) {
    char *command_list[20]; // Array to store commands
    char *current_token = strtok(cmd_text, ";"); // Split by semicolon
    int command_count = 0;
    
    while (current_token) {
        command_list[command_count++] = current_token; // Store each command
        current_token = strtok(NULL, ";");
    }
    
    for(int cmd_index = 0; cmd_index < command_count; cmd_index++) {
        pid_t process_id = fork();
        if (process_id == 0) { // Child executes command
            run_command_direct(command_list[cmd_index]);
        } else {
            waitpid(process_id, NULL, 0); // Wait before next command
        }
    }
}

// ======== CONDITIONAL EXECUTION ======== //

void handle_conditional_execution(char *cmd_text) {
    int total_length = strlen(cmd_text);
    char *current_command = malloc((total_length + 1) * sizeof(char)); // Buffer for current command
    int execution_status = -1; // Track last command result (-1: not set, 0: failed, 1: success)
    char operator_type[3] = ""; // Track current operator type
    int char_index = 0, cmd_index = 0;
    
    while(1) {
        if(cmd_text[char_index]=='&' && cmd_text[char_index+1]=='&') { // AND operator
            char_index += 2;
            if(execution_status == -1 || // First command
               (execution_status == 1 && strcmp(operator_type,"&&") == 0) || // Previous success with AND
               (execution_status == 0 && strcmp(operator_type,"||") == 0)) { // Previous fail with OR
                
                if(validate_argument_count(current_command) == -1) {
                    printf("Invalid argument count\n");
                    continue;
                }
                
                current_command[cmd_index] = '\0';
                pid_t process_id = fork();
                if(process_id == 0) { // Child executes command
                    run_command_direct(current_command);
                } else {
                    int process_status;
                    waitpid(process_id, &process_status, 0);
                    if(WIFEXITED(process_status)) { // Check exit status
                        execution_status = (WEXITSTATUS(process_status) == 0) ? 1 : 0;
                    }
                    cmd_index = 0; // Reset for next command
                }
            }
            strcpy(operator_type, "&&"); // Set operator type
        }
        else if(cmd_text[char_index]=='|' && cmd_text[char_index+1]=='|') { // OR operator
            char_index += 2;
            if(execution_status == -1 || // First command
               (execution_status == 1 && strcmp(operator_type,"&&") == 0) || // Previous success with AND
               (execution_status == 0 && strcmp(operator_type,"||") == 0)) { // Previous fail with OR
                
                if(validate_argument_count(current_command) == -1) {
                    printf("Invalid argument count\n");
                    continue;
                }
                
                current_command[cmd_index] = '\0';
                pid_t process_id = fork();
                if(process_id == 0) { // Child executes command
                    run_command_direct(current_command);
                } else {
                    int process_status;
                    waitpid(process_id, &process_status, 0);
                    if(WIFEXITED(process_status)) { // Check exit status
                        execution_status = (WEXITSTATUS(process_status) == 0) ? 1 : 0;
                    }
                    cmd_index = 0; // Reset for next command
                }
            }
            strcpy(operator_type, "||"); // Set operator type
        }
        else if(cmd_text[char_index] == '\0') { // End of command string
            if(validate_argument_count(current_command) == -1) {
                printf("Invalid argument count\n");
                break;
            }
            
            current_command[cmd_index] = '\0';
            if((strcmp(operator_type,"&&") == 0 && execution_status == 1) || // Execute if AND and previous success
               (strcmp(operator_type,"||") == 0 && execution_status == 0)) { // Execute if OR and previous fail
                pid_t process_id = fork();
                if(process_id == 0) { // Child executes final command
                    run_command_direct(current_command);
                } else {
                    waitpid(process_id, NULL, 0);
                }
            }
            break;
        }
        else {
            current_command[cmd_index++] = cmd_text[char_index++]; // Build current command
        }
    }
    free(current_command);
}

// ======== MAIN FUNCTION ======== //

int main(int argc, char *argv[]) {
    while (1) { // Main shell loop
        char* user_command = get_user_command(); // Get command from user
        
        if (strcmp(user_command, "killterm") == 0) { // Exit shell
            free(user_command);
            exit(0);
        }
        else if (strcmp(user_command, "newt") == 0) { // Create new terminal
            create_new_terminal();
        }
        else if (strstr(user_command, "&&") || strstr(user_command, "||")) { // Conditional execution
            handle_conditional_execution(user_command);
        }
        else if (strstr(user_command, "~")) { // Reversed pipe
            handle_reversed_pipe(user_command);
        }
        else if (strstr(user_command, "#")) { // Word counter
            process_word_counter(user_command);
        }
        else if (strstr(user_command, "++")) { // File concatenation
            process_file_concatenation(user_command);
        }
        else if (strstr(user_command, "+") && !strstr(user_command, "++")) { // Mutual file append
            process_mutual_file_append(user_command);
        }
        else if (strstr(user_command, "&") || strcmp(user_command, "fg") == 0) { // Background execution
            manage_background_execution(user_command);
        }
        else if (strstr(user_command, "|")) { // Pipe execution
            handle_piped_commands(user_command);
        }
        else if (strstr(user_command, "<")) { // Input redirection
            handle_input_redirection(user_command);
        }
        else if (strstr(user_command, ">>")) { // Append output redirection
            handle_output_redirection(user_command, ">>");
        }
        else if (strstr(user_command, ">")) { // Output redirection
            handle_output_redirection(user_command, ">");
        }
        else if (strstr(user_command, ";")) { // Sequential execution
            run_sequential_commands(user_command);
        }
        else { // Single command execution
            if (validate_argument_count(user_command) == -1) {
                printf("Invalid argument count (1-5 allowed)\n");
            } else {
                pid_t process_id = fork(); // Fork for single command
                run_single_command(user_command, process_id);
            }
        }
        free(user_command); // Clean up memory
    }
    return 0;
}
