#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // for pipe()
#include <sys/fcntl.h>  // for pid_t, open() function flags, etc.
#include <sys/wait.h>   // for wait() and waitpid()
#include <string.h>

#include "executor.h"


#define MAX_JOBS 100    // maximum number of active jobs permitted in shell
static struct job jobs[MAX_JOBS]; // jobs array to keep track of all jobs/processes running in the background
static int next_job_id = 1;     // static variable for unique job_id



/**
 * @brief Adds a background job to the jobs array
 * @param pid Process ID of the background job
 * @param cmd Command string that the job is executing
 */
void add_job(pid_t pid, const char* cmd) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) { // add new job in empty slot or overwrite inactive jobs
            jobs[i].pid = pid;
            jobs[i].cmd = strdup(cmd);  // strdup() used to dynamically duplicate the command string
            jobs[i].job_id = next_job_id++;
            jobs[i].active = 1;
            return;
        }
    }
    fprintf(stderr, "Shell: Cannot add more background jobs, max limit reached!\n");
    return;
}



/**
 * @brief Updates job states by checking and freeing the memory for finished jobs
 */
void update_job_states() {
    int status;
    pid_t result;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {

            // Check if the process has finished (non-blocking wait)
            // WNOHANG is used to check the status of child process without blocking parent process
            result = waitpid(jobs[i].pid, &status, WNOHANG);

            if (result == jobs[i].pid) {    // waitpid returns child PID on child execution completion
                // the process(job) has finished
                jobs[i].active = 0;
                free(jobs[i].cmd);
                jobs[i].cmd = NULL;
            } 
            else if (result == -1) {    // waitpid returns -1 on error
                // the process does not exist anymore / the job pid is invalid
                perror("update_job_states: waitpid");
                jobs[i].active = 0;
                free(jobs[i].cmd);
                jobs[i].cmd = NULL;
            }

        }
    }
}



/**
 * @brief Lists all jobs currently running in the background with their pid and command
 */
void list_jobs() {
    // update the current job array first
    update_job_states();

    // print the details of each running(active) job
    int found_active_job = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            printf("[JOB ID: %d] Executing %s (PID: %d)\n", jobs[i].job_id, jobs[i].cmd, jobs[i].pid);
            found_active_job = 1;
        }
    }
    if (found_active_job == 0) {
        printf("No active background jobs to display.\n");
    }
}



/**
 * @brief Executes a single, simple command in another process
 * @param cmd Command to execute
 * @param args Array of arguments including the command as args[0]
 * @param in Name of file for input redirection; otherwise NULL
 * @param out Name of file for output redirection; otherwise NULL
 * @param bg Background execution control flag (1 for background, 0 for foreground)
 * @return 0 on success, or a non-zero value on error
 */
int execute_command(char* cmd, char** args, char* in, char* out, int bg) {
    
    pid_t pid = fork();

    if (pid < 0) {
		// fork failed
		perror("fork failed");
		return EXIT_FAILURE;
    }

    else if (pid == 0) {
		// child process

        int fd_in, fd_out;

        // Input redirection
        if (in != NULL) {
            fd_in = open(in, O_RDONLY); // opens input file in read only access mode
            if (fd_in == -1) {
                perror("Input redirection failed");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO); // points standard input to the input file (duplicates file descriptor)
            close(fd_in);
        }


        // Output redirection
        if (out != NULL) {
            // opens output file in write only access mode and creates the file if it does not exist
            // 0644 is the default permission settings for creating files
            fd_out = open(out, O_WRONLY | O_CREAT, 0644);

            if (fd_out == -1) {
                perror("Output redirection failed");
                exit(EXIT_FAILURE);
            }
            if (ftruncate(fd_out, 0) == -1) { // erases existing file content
                perror("Ftruncate error");
                exit(EXIT_FAILURE);
            } 

            dup2(fd_out, STDOUT_FILENO); // points standard output to the output file
            close(fd_out);
        }

        // Replace the child process with a new program to execute the command
        execvp(cmd, args);

        // If execvp returns, an error occurred
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }

    // parent process
    // Background execution control
    if (bg == 0) { 
        // shell waits until the command execution completes before user can enter new command
        wait(NULL);
    } else if (bg == 1) {
        // process runs in the background
        // add process to job list
        add_job(pid, cmd);
    }

    return EXIT_SUCCESS;
}



/**
 * @brief Executes a command pipeline.
 * @param l A pointer to a cmdline structure containing the parsed command.
 * @return 0 on success, or a non-zero value on error.
 */
int execute(struct cmdline* l) {

    // Update job states before executing new commands
    update_job_states();
    
    // First check if the command is the built-in command "jobs" (single command)
    if (l->seq[0] != NULL && l->seq[0][0] != NULL && l->seq[1] == NULL) {
        if (strcmp(l->seq[0][0], "jobs") == 0) {    // strcmp() to compare two strings; returns 0 if equal
            list_jobs();
            return EXIT_SUCCESS;
        }
    }

    // Count the number of commands for pipeline
    int num_commands = 0;
    for (int i = 0; l->seq[i] != NULL; i++) {
        num_commands++;
    }

    // Single command
    if (num_commands == 1) {
        char** args = l->seq[0];    // Command arguments
        char* cmd = args[0];        // Command name

        return execute_command(cmd, args, l->in, l->out, l->bg);
    }

    // Two or more commands (single or multiple pipe)
    else if (num_commands > 1) {
        return execute_pipe(l, num_commands);
    }

    return EXIT_SUCCESS;
}



/**
 * @brief Executes a pipeline (single or multiple pipe)
 * @param l A pointer to a cmdline structure containing the parsed command.
 * @param num_cmds Number of commands to execute for pipeline
 * @return 0 on success, or a non-zero value on error.
 */
int execute_pipe(struct cmdline* l, int num_cmds) {

    // We need to create a child process for each command

    pid_t pid_child[num_cmds]; // array to store PIDs of all child process
    int status;

    int read_prev_end_fd = STDIN_FILENO; // Read end file descriptor of the previous pipe
                        // For the first command, the file descriptor is standard input

    // loop through all commands
    for (int i = 0; i < num_cmds; i++) {

        int pipefd[2]; // pipefd[0] is the read end and pipefd[1] is the write end

        // Create a pipe for all commands except the last one
        if (i < num_cmds - 1) {
            if (pipe(pipefd) == -1) {
                perror("Pipe failed");
                return EXIT_FAILURE;
            }
        }

        // create a child for each command
        pid_child[i] = fork();

        if (pid_child[i] < 0) {
            // fork failed
            perror("fork failed");
            return EXIT_FAILURE;
        }

        else if (pid_child[i] == 0) {
            // Child process

            // First command - writer (child writes to the pipe)
            if (i == 0) {

                // close unused read end
                close(pipefd[0]);

                // point standard output to the write end of the pipe
                dup2(pipefd[1], STDOUT_FILENO);

                close(pipefd[1]); // close the original write-end descriptor
                
                // Input redirection for the first command
                // Output redirection to file not possible since the output is already redirected to the second command through pipe
                if (l->in != NULL) {
                    int fd_in = open(l->in, O_RDONLY); // opens input file in read only access mode
                    if (fd_in == -1) {
                        perror("Input redirection failed");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_in, STDIN_FILENO); // points standard input to the input file (duplicates file descriptor)
                    close(fd_in);
                }

            }

            // Last command - reader (child reads from the pipe)
            else if (i == num_cmds - 1) {

                // point standard input to the read end of the previous pipe
                dup2(read_prev_end_fd, STDIN_FILENO);

                close(read_prev_end_fd); // close the original read-end descriptor

                // Output redirection for the last command
                // Input redirection to file not possible since the input is already connected to the previous command through pipe
                if (l->out != NULL) {

                    // opens output file in write only access mode and creates the file if it does not exist
                    // 0644 is the default permission settings for creating files
                    int fd_out = open(l->out, O_WRONLY | O_CREAT, 0644);

                    if (fd_out == -1) {
                        perror("Output redirection failed");
                        exit(EXIT_FAILURE);
                    }
                    if (ftruncate(fd_out, 0) == -1) { // Erase existing file content
                        perror("Ftruncate error");
                        exit(EXIT_FAILURE);
                    } 

                    dup2(fd_out, STDOUT_FILENO); // points standard output to the output file
                    close(fd_out);
                }

            }

            // Middle commands if exist - reader and writer (both reads and writes to the pipe)
            else {

                // input
                // point standard input to the read end of the previous pipe
                dup2(read_prev_end_fd, STDIN_FILENO);
                // close the original read-end descriptor
                close(read_prev_end_fd);

                // output
                // point standard output to the write end of the pipe
                dup2(pipefd[1], STDOUT_FILENO);
                // close both read and write ends of the pipe
                close(pipefd[0]);
                close(pipefd[1]);

            }

            // Execute each command in the pipeline
            execvp(l->seq[i][0], l->seq[i]);

            // If execvp returns, an error occurred
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }

        // Parent process (inside loop) - for each child/command

        // close the read end of previous pipe if it was not standard input (if it was not the first command)
        if (read_prev_end_fd != STDIN_FILENO) {
            close(read_prev_end_fd);
        }

        // Save the read end of pipe for the next child except for the last process
        if (i < num_cmds - 1) {
            close(pipefd[1]); // Close the write end of parent
            read_prev_end_fd = pipefd[0]; // Save the read end for the next child
        }

        // add process to job list if background execution required
        if(l->bg == 1) {
            add_job(pid_child[i], l->seq[i][0]);
        }

    }

    // Parent process (outside loop)
    // If foreground execution required
    if(l->bg == 0) {
        // waitpid on every child
        for(int i = 0; i < num_cmds; i++) {
            waitpid(pid_child[i], &status, 0);
        }
    }
    return EXIT_SUCCESS;
}