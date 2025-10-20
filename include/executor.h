//
// Created by Paula on 2025-08-11.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#endif // EXECUTOR_H

#include <sys/fcntl.h>
#include "parser.h"

// Struct Job to track background processes
struct job {
    int job_id;       // The job ID
    pid_t pid;        // The process ID
    char* cmd;        // The command string
    int active;       // Flag to mark if the process is running (1) or finished (0)
};

// Job management functions
void add_job(pid_t pid, const char* cmd);
void update_job_states();
void list_jobs();

int execute_command(char* cmd, char** args, char* in, char* out, int bg);

int execute_pipe(struct cmdline* l, int num_cmds);

int execute(struct cmdline* l);