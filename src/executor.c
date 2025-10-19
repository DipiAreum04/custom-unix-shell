#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/wait.h>

#include "parser.h"

/**
 * @brief Executes a single, simple command in another process
 * @param cmd
 * @param args
 * @param in
 * @param out
 * @param bg
 * @return
 */
int execute_command(char* cmd, char** args, int bg) {
    
    pid_t pid = fork();
    if (pid < 0) {
		// fork failed
		perror("fork failed\n");
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

    if (bg == 0) { 
        // if bg not set, shell waits until the command execution completes before entering new command
        wait(NULL);
    }
    // if bg is 1, shell does not wait for command execution completion

    return EXIT_SUCCESS;
}

/**
 * @brief Executes a command pipeline.
 * @param l A pointer to a cmdline structure containing the parsed command.
 * @return 0 on success, or a non-zero value on error.
 */
int execute(struct cmdline *l){
    printf("TODO: Execute the command\n");
    int i;
    if (l->in) printf("in: %s\n", l->in);
    if (l->out) printf("out: %s\n", l->out);
    if (l->bg) printf("background (&)\n");

    /* Display each command of the pipe */
    for (i=0; l->seq[i]!=0; i++) {
        char** args = l->seq[i];    // Command arguments
        char* cmd = args[0];   // Command name

        return execute_command(cmd, args, l->bg);
    }

    return EXIT_SUCCESS;
}
