// Name: Andy Nguyen
// ***compile using -> gcc processmovies.c -o smallsh
// ***run updated test script using -> ./p3testscript 2>&1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h> 
#include <fcntl.h>  

int foreground_only_mode = 0; // Foreground only mode is currently off

// Toggle 'ON/OFF' foreground only mode when shell receives SIGSTP (CTRL+Z)
void catch_sigtstp(int signal_num) {
	// If foreground mode is OFF, turn it ON
	if (foreground_only_mode == 0) {
		write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 50); // Message length is 50 chars
		write(STDOUT_FILENO, ": ", 2); // Reprompt user for command
		foreground_only_mode = 1; // Turn on foreground mode

	// Else if foreground mode is ON, turn it OFF	
	} else if (foreground_only_mode == 1) {
		write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 30); // Message length is 30 chars
		write(STDOUT_FILENO, ": ", 2); // Reprompt user for command
		foreground_only_mode = 0; // Turn off foreground mode
	}
}

// Check for $$ and replace with PID
void shell_expansion(char* string, int string_len, char expanded_string[]) {
		pid_t curr_pid = getpid(); // Store current PID
		char string_pid [32768]; // Common max value for PID
		sprintf(string_pid, "%d", curr_pid); // Make PID a string

		int i;
		for (i = 0; i < string_len; i++) {
			// If $$ is found
			if (string[i] == '$' && string[i+1] == '$') {
				// Copy string up until $$
				strcat(expanded_string, string_pid);
				i++;
			// Else there's no $$, append the single character and add null terminator to expanded string
			} else {
				char add_char[2] = {string[i], '\0'};  
            	strcat(expanded_string, add_char);
			}
		}
		return;
}

// built-in 'exit' command
void exit_program(pid_t* pid_array, int background_pid_count) {
	// Kill all background processes
	if (pid_array != NULL) {
		int i;
		for (i = 0; i < background_pid_count; i++) {
			kill(pid_array[i], SIGKILL);
		}
		// Free the dynamic array of background PIDs
		free(pid_array);
	}
    exit(1);
}

// built-in 'cd' command
void change_directory(char* path) {
	// If user did not supply argument, change to home directory
	if (path == NULL) {
		char* home_directory = getenv("HOME");
		chdir(home_directory);

	// Else if, path doesn't exist
	} else if (chdir(path) != 0) {
		printf("Path doesn't exist\n");
		fflush(stdout); // Flush error message

	// Else, change directory to supplied path
	} else {
		chdir(path);
	}
	return;
}

// built-in 'status' command
void shell_status(int exit_status) {
	// Check if status of last command is signal or exit code
	if (WIFEXITED(exit_status)) {
		// If last command was exit code
		int exit_code = WEXITSTATUS(exit_status);
		printf("exit value %d\n", exit_code); // Print exit value to terminal
	} else if (WIFSIGNALED(exit_status)) {
		// If last command was interrupted by signal
		int signal_number = WTERMSIG(exit_status);
		printf("terminated by signal %d\n", signal_number); // Print terminating signal to terminal
	}
	fflush(stdout); // Flush out status message
	return;
}

// Add background PID to the array
pid_t* add_pid(pid_t* pid_arr, int array_size, pid_t pid) {
	// Allocate additional memory to dynamic array
	pid_arr = realloc(pid_arr, (array_size + 1) * sizeof(pid_t));
	// Add pid to background pid's array 
    pid_arr[array_size] = pid;
	return pid_arr;
}

// Check for finished background processes and update
int check_background_processes(pid_t pid_arr[], int arr_size) {
	int status;
	int i;
	for (i = 0; i < arr_size; i++) {
		pid_t process_result = waitpid(pid_arr[i], &status, WNOHANG); // background processes

		// If child process finished
		if (process_result > 0) {
			// If process exited normally
			if (WIFEXITED(status)) {
				// Print completion and exit value
				int exit_value = WEXITSTATUS(status);
                printf("background pid %d is done: exit value %d\n", pid_arr[i], exit_value);

			// If process killed by a signal
            } else if (WIFSIGNALED(status)) {
				// Print completion and signal number
				int signal_number = WTERMSIG(status);
                printf("background pid %d is done: terminated by signal %d\n", pid_arr[i], signal_number);
            }
			fflush(stdout);

			// Remove completed PID(s)
			int j;
			// Shift the PIDs to the left to compensate for removal
			for (j = i; j < arr_size - 1; j++) {
				pid_arr[j] = pid_arr[j+1];
			}
			arr_size--; // Decrement array size counter
			i--; // Adjust index for outer for-loop
		}
	}
	return arr_size; // Update array size
}

int main() {
	// Configure signals SIGINT and SIGTSTP

 	// SIGINT: ignore signal in smallsh
	struct sigaction SA_INT = {0}; 
	SA_INT.sa_handler = SIG_IGN; // Ignore
	sigfillset(&SA_INT.sa_mask); // Block all other signals
	SA_INT.sa_flags = SA_RESTART; // Restart system calls if interrupted
	sigaction(SIGINT, &SA_INT, NULL); // Apply signal handler

	// SIGTSTP: toggle foreground-only mode
	struct sigaction SA_TSTP = {0};
	SA_TSTP.sa_handler = catch_sigtstp; // Toggle foreground-only mode
	sigfillset(&SA_TSTP.sa_mask); // Block all other signals
	SA_TSTP.sa_flags = SA_RESTART; // Restart system calls if interrupted
	sigaction(SIGTSTP, &SA_TSTP, NULL); // Apply signal handler


	// Initialize variables for reading command
	char* user_command = malloc(2049 * sizeof(char));
	size_t length = 2049;
	ssize_t read;

	// Keep track of background PIDs
	pid_t* background_pid_arr = NULL;  // Dynamic array to store background PIDs
	int background_pids_counter = 0;  // Track number of background processes

	// Set exit code to 0
	int exit_code = 0; // 0 = no error, 1 = error
	int exit_status; // Store exit status from processes

	while (1) {
		// Check and update any background child PIDS that were terminated/done
		background_pids_counter = check_background_processes(background_pid_arr, background_pids_counter);

		// Prompt user for command
		printf(": "); // Print colon for user to enter command
		fflush(stdout); // Flush command prompt

		read = getline(&user_command, &length, stdin); // Read user command
		// Reprompt the user if interrupted by a signal
        if (errno == EINTR) {  // EINTR indicates interruption by signal
			clearerr(stdin); // Clear the error state of stdin
            continue;  
        }
    	user_command[read - 1] = '\0'; // Remove trailing newline fromt getline() use

		// If user entered comment or newline, reprompt for command
		if (user_command[0] == '#' || user_command[0] == '\n') {
			continue;
		}

		// Check for $$ and replace with PID
		char expanded_command[2049 * 2] = ""; // Store expanded command
		shell_expansion(user_command, read, expanded_command);

		// Set up variables for command tokenization
		char* command = NULL; // 1 command 
		// Max: 512 arguments + NULL
		char* arguments[513];
		int argument_counter = 0;
		// OPTIONAL: 1 input, 1 output, 1 & (background)
		char* input_file = NULL;
		char* output_file = NULL;
		int background_bool = 0; // Foreground mode is OFF in the beginning

		// Tokenize input string

		// Store first token into command and argument[0]
		char* token = strtok(expanded_command, " ");
		if (token != NULL) {
			command = token;
			arguments[0] = token;
			argument_counter++;
		}
		// Parse remaining tokens if they exist
		while ((token = strtok(NULL, " ")) != NULL) {
			// If token is input redirection, store input file
			if (strcmp(token, "<") == 0) {
				token = strtok(NULL, " ");
				if (token != NULL) {
					input_file = token; // Verify input file is supplied
				}

			// If token is output redirection, store output file
			} else if (strcmp(token, ">") == 0) {
				token = strtok(NULL, " ");
				if (token != NULL) {
					output_file = token; // Verify output file is supplied
				}
			
			// If token is an ampersand, turn on background process flag
			} else if (strcmp(token, "&") == 0) {
				background_bool = 1;

			// Else, the token is an argument
			} else {
				// Store argument token in arguments array
				arguments[argument_counter] = token;
				argument_counter++; 
			}
			
			// If foreground-only mode is ON, ignore '&'
			if (foreground_only_mode == 1) {
				background_bool = 0;
			}

		}
		// Null terminate array
		arguments[argument_counter] = NULL; // Important for execvp later

		// Check if user's command is 'exit'
		if (strcmp(command, "exit") == 0) { // String compare command and 'exit'
			free(user_command); // Free memory allocated by get_line()
			exit_program(background_pid_arr, background_pids_counter);

		// Check if user's command is 'cd'
		} else if (strcmp(command, "cd") == 0) {
			change_directory(arguments[1]); // Supplied with first argument

		// Check if user's command is 'status'
		} else if (strcmp(command, "status") == 0) { // String compare command and 'status'
			shell_status(exit_status); // Print exit status or terminating signal of most recent foreground command
		
		// Execute 'other' command
		} else {
			pid_t fork_result = fork();

			// CHILD PROCESS
			if (fork_result == 0) {
				// If foreground process
				if (foreground_only_mode == 1 || background_bool == 0) {
					// Restore default SIGINT for foreground processes
					struct sigaction default_sigint = {0};
					default_sigint.sa_handler = SIG_DFL; // Restore default SIGINT
					sigaction(SIGINT, &default_sigint, NULL); // Apply signal handler

					// If input file was supplied in user input string
					if (input_file != NULL) {
						// Open the input file in read-only mode
						int i_file = open(input_file, O_RDONLY, 0644);
						// Print error if input file doesn't exit
						if (i_file == -1) {
							printf("bash: %s: No such file or directory\n", input_file);
							fflush(stdout); 
							exit(1); // Set exit status to 1 for error
						} else {
							// Redirect standard input to the file
							dup2(i_file, STDIN_FILENO); 
						}
						close(i_file); // Close file descriptor
					}

					// If output file was supplied in user input string
					if (output_file != NULL) {
						// Open the output file in write-only mode. Truncate if file exists or Create if file doesn't exist
						int o_file = open(output_file, O_WRONLY | O_TRUNC | O_CREAT, 0666);
						// Print error if output file doesn't exit
						if (o_file == -1) {
							printf("bash: %s: No such file or directory\n", output_file);
							fflush(stdout);
							exit(1); // Set exit status to 1 for error
						} else {
							// Redirect standard output to the file
							dup2(o_file, STDOUT_FILENO);
						}
						close(o_file); // Close file descriptor
					}

				// If background process
				} else if (foreground_only_mode == 0 && background_bool == 1) {

					// INPUT REDIRECTION
					if (input_file == NULL) {
						// If no input file was supplied, redirect to /dev/null 
						int i_file = open("/dev/null", O_RDONLY, 0644); // Open input file in read-only mode
						dup2(i_file, STDIN_FILENO); // Redirect standard input to /dev/null
						close(i_file); // Close file descriptor

					} else {
						// If input file was supplied, open it
						int i_file = open(input_file, O_RDONLY, 0644); // Open input file in read-only mode
						// Print error if input file doesn't exit
						if (i_file == -1) {
							printf("bash: %s: No such file or directory\n", input_file);
							fflush(stdout);
							exit(1); // Set exit status to 1
						} else {
							// Redirect standard input to file if exists
							dup2(i_file, STDIN_FILENO); 
						}
						close(i_file); // Close file descriptor
					}

					// OUTPUT REDIRECTION
					if (output_file == NULL) {
						// If no output file was supplied redirect to /dev/null
						int o_file = open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, 0666); // Open output file in write-only mode
						dup2(o_file, STDOUT_FILENO); // Redirect standard output to /dev/null
						close(o_file); // Close file descriptor

					} else {
						// If output file was supplied, open it
						int o_file = open(output_file, O_WRONLY | O_TRUNC | O_CREAT, 0666); // Open output file in write-only mode
						// Print error if output file doesn't exit
						if (o_file == -1) {
							printf("bash: %s: No such file or directory\n", output_file);
							fflush(stdout);
							exit(1); // Set exit status to 1
						} else {
							// Redirect standard output to file if exists
							dup2(o_file, STDOUT_FILENO);
						}
						close(o_file); // Close file descriptor
					}
				}	

				execvp(command, arguments); // Execute command with arguments
				// If execvp fails...
				printf("bash: %s: command not found\n", command);
				fflush(stdout); // Flush out error message
				exit(1); // Set exit status to 1


			// PARENT PROCESS (wait for child process to terminate)
			} else {
				// Ignore SIGINT in parent process
				SA_INT.sa_handler = SIG_IGN; 

				// If foreground-only mode is ON or user did not supply &, run process in foreground
				if (foreground_only_mode == 1 || background_bool == 0) {
					waitpid(fork_result, &exit_status, 0); // Must wait for child process to complete
					
					// If process was terminated by signal, print terminating signal
					if (WIFSIGNALED(exit_status)) {
						int signal_number = WTERMSIG(exit_status);
						printf("terminated by signal %d\n", signal_number);
						fflush(stdout);
					}
					// Update exit status to 1 if failed command or 0 if succeeded
					exit_code = WEXITSTATUS(exit_status);

				// If foreground-only mode is OFF and user supplied &, run process in background
				} else if (background_bool == 1) {
					// Alert user process is being run in background
					printf("background pid is %d\n", fork_result);
					fflush(stdout);

					// Add background PID to array
					background_pid_arr = add_pid(background_pid_arr, background_pids_counter, fork_result);
					background_pids_counter++; // Increment background PID counter
				}
			}
		}
	}	
	free(user_command); // Free dynamic memory allocated by getline()
	return(0);
}