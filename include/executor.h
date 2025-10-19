//
// Created by Paula on 2025-08-11.
//

#ifndef EXECUTOR_H
#define EXECUTOR_H

#endif //EXECUTOR_H

#include <sys/fcntl.h>
#include "parser.h"

int execute_command(char* cmd, char** args, int bg);

int execute(struct cmdline *l);

// Struct Job to track background processes
struct job {
    int job_id;       // The job ID
    pid_t pid;        // The process ID
    char* cmd;        // The command string
    int active;       // Flag to mark if the process is running (1) or finished (0)
};

// Job management functions
int add_job(pid_t pid, const char* cmd);
void update_job_states();
void list_jobs();