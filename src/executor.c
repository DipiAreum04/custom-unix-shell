#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
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

int add_job(pid_t pid, const char* cmd) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) { // add new job in empty slot or overwrite inactive jobs
            jobs[i].pid = pid;
            jobs[i].cmd = strdup(cmd);
            jobs[i].job_id = next_job_id++;
            jobs[i].active = 1;
            return jobs[i].job_id;
        }
    }
    fprintf(stderr, "Shell: Cannot add more background jobs, max limit reached!\n");
    return -1;
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


int execute_command(char* cmd, char** args, int bg) {
    
    pid_t pid = fork();
    if (pid < 0) {
		// fork failed
		perror("fork failed");
		return EXIT_FAILURE;
    }
    else if(pid == 0) {
		// child process

        // Replace the child process with a new program to execute the command
        execvp(cmd, args);

        // If execvp returns, an error occurred
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }

    // parent process

    if (bg == 0) { 
        // shell waits until the command execution completes before entering new command
        wait(NULL);
    } else if (bg == 1) {
        // process runs in the background
        int job_id = add_job(pid, cmd);
        printf("[%d: PID %d] running %s in the bg\n", job_id, pid, cmd);
    }

    return EXIT_SUCCESS;
}


/**
 * @brief Executes a command pipeline.
 * @param l A pointer to a cmdline structure containing the parsed command.
 * @return 0 on success, or a non-zero value on error.
 */
int execute(struct cmdline *l){

    if (l->in) printf("in: %s\n", l->in);
    if (l->out) printf("out: %s\n", l->out);
    if (l->bg) printf("background (&)\n");

    // Update job states before executing new commands
    update_job_states();
    
    // Check if the command was the built-in command "jobs"
    if (l->seq[0] != NULL && l->seq[0][0] != NULL) {
        if (strcmp(l->seq[0][0], "jobs") == 0) {
            list_jobs();
            return EXIT_SUCCESS;
        }
    }

    for (int i=0; l->seq[i]!=0; i++) {
        char** args = l->seq[i];    // Command arguments
        char* cmd = args[0];   // Command name

        return execute_command(cmd, args, l->bg);
    }

    return EXIT_SUCCESS;
}
