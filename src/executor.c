#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // for pipe()
#include <sys/fcntl.h>  // for pid_t, open() function flags etc.
#include <sys/wait.h>   // for wait() and waitpid()
#include <string.h>

#include "executor.h"


/**
 * @brief Executes a single, simple command in another process
 * @param cmd
 * @param args
 * @param in
 * @param out
 * @param bg
 * @return
 */


#define MAX_JOBS 100
static struct job jobs[MAX_JOBS];
static int next_job_id = 1;

void add_job(pid_t pid, const char* cmd) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) { // add new job in empty slot or overwrite inactive jobs
            jobs[i].pid = pid;
            jobs[i].cmd = strdup(cmd);
            jobs[i].job_id = next_job_id++;
            jobs[i].active = 1;
            return;
        }
    }
    fprintf(stderr, "Shell: Cannot add more background jobs, max limit reached!\n");
    return;
}


void update_job_states() {
    int status;
    pid_t result;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {

            // Check if the process has finished (non-blocking)
            // WNOHANG is used to check the status of child process without blocking parent process
            result = waitpid(jobs[i].pid, &status, WNOHANG);

            if (result == jobs[i].pid) {
                // the process(job) has finished
                jobs[i].active = 0;
                free(jobs[i].cmd);
                jobs[i].cmd = NULL;
            } 
            else if (result == -1) {
                // the process does not exist anymore / the job pid is invalid
                perror("update_job_states: waitpid");
                jobs[i].active = 0;
                free(jobs[i].cmd);
                jobs[i].cmd = NULL;
            }

        }
    }
}


void list_jobs() {
    // update the current job array first
    update_job_states();

    // prints the details of each running job
    int found_active_job = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            printf("[JOB ID: %d] Running %s (PID: %d)\n", jobs[i].job_id, jobs[i].cmd, jobs[i].pid);
            found_active_job = 1;
        }
    }
    if (found_active_job == 0) {
        printf("No active background jobs to display.\n");
    }
}


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
            if (ftruncate(fd_out, 0) == -1) { // Erase existing file content
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
        // shell waits until the command execution completes before entering new command
        wait(NULL);
    } else if (bg == 1) {
        // process runs in the background
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
        if (strcmp(l->seq[0][0], "jobs") == 0) {
            list_jobs();
            return EXIT_SUCCESS;
        }
    }

    // count the number of commands for pipeline
    int num_commands = 0;
    for (int i = 0; l->seq[i] != NULL; i++) {
        num_commands++;
    }

    // single command
    if (num_commands == 1) {
        char** args = l->seq[0];    // Command arguments
        char* cmd = args[0];        // Command name

        return execute_command(cmd, args, l->in, l->out, l->bg);
    }

    // two commands (simple pipe)
    else if (num_commands == 2) {
        return execute_simple_pipe(l);
    }

    // three or more commands (multiple pipe)
    else if(num_commands > 2) {
        return execute_multipipe(l);
    }

    return EXIT_SUCCESS;
}



int execute_simple_pipe(struct cmdline* l) {

    int pipefd[2];
    pid_t pid_child1, pid_child2;
    int status;

    // Create a pipe
    // pipefd[0] is the read end and pipefd[1] is the write end
    if (pipe(pipefd) == -1) {
        perror("Pipe failed");
        return EXIT_FAILURE;
    }

    // Child 1
    pid_child1 = fork();

    if (pid_child1 < 0) {
		// fork failed
		perror("fork failed");
		return EXIT_FAILURE;
    }

    else if (pid_child1 == 0) {
		// child 1 process - writer (child writes to the pipe)

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

        // Execute the first command in the pipeline
        execvp(l->seq[0][0], l->seq[0]);

        // If execvp returns, an error occurred
        perror("execvp child 1 failed");
        exit(EXIT_FAILURE);
    }

    // Child 2
    pid_child2 = fork();

    if (pid_child2 < 0) {
		// fork failed
		perror("fork failed");
		return EXIT_FAILURE;
    }

    else if (pid_child2 == 0) {
		// child 2 process - reader (child reads from the pipe)

        // close unused write end
        close(pipefd[1]);

        // point standard input to the read end of the pipe
        dup2(pipefd[0], STDIN_FILENO);

        close(pipefd[0]); // close the original read-end descriptor

        // Output redirection for the last command
        // Input redirection to file not possible since the input is already connected to the first command through pipe
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

        // Execute the second(last) command in the pipeline
        execvp(l->seq[1][0], l->seq[1]);

        // If execvp returns, an error occurred
        perror("execvp child 2 failed");
        exit(EXIT_FAILURE);
    }

    // Parent Process
    // Close both unused read and write pipe ends in the parent
    close(pipefd[0]);
    close(pipefd[1]);

    // Background execution control
    if (l->bg == 0) { 
        // shell waits for both children process to finish execution
        waitpid(pid_child1, &status, 0);
        waitpid(pid_child2, &status, 0);
    } 
    else if (l->bg == 1) {
        // both process run in the background
        // add both children process to the jobs array
        add_job(pid_child1, l->seq[0][0]);
        add_job(pid_child2, l->seq[1][0]);
    }

    return EXIT_SUCCESS;
}