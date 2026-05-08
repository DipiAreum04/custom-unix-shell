# Custom Unix Shell

A POSIX-compatible command-line shell implemented in C, demonstrating hands-on use of core Unix process management and inter-process communication APIs. Built as part of the Operating Systems course at Concordia University.

## Features

- **Command execution** — runs any command available on `PATH` via `fork()` + `execvp()`
- **Background execution** — append `&` to run a process in the background while keeping the shell responsive
- **Job management** — list background jobs with the built-in **`jobs`** command (shows Process ID + command)
- **I/O redirection** — supports `cmd < infile` and `cmd > outfile` using `dup2()`, `open()`, and `ftruncate()` (existing files are truncated on write)
- **Single pipe** — connects two commands via `|`, routing stdout of one to stdin of the next
- **Multiple pipes** — chains multiple numbers of commands (e.g. `cat file | sort | uniq | wc -l`), with full support for I/O redirection and background execution at either end of the pipeline

## System calls used

`fork`, `execvp`, `wait`, `waitpid`, `pipe`, `dup2`, `open`, `close`, `ftruncate`

## Usage examples
 
**Run a command**
```bash
$ echo hello world
hello world
```
 
**Redirect output to a file, then read it back**
```bash
$ echo hello > out.txt
$ cat out.txt
hello
```
 
**Redirect input from a file**
```bash
$ cat names.txt
Charlie
Alice
Bob
 
$ sort < names.txt
Alice
Bob
Charlie
```
 
**Run in the background, then check on it**
```bash
$ sleep 60 &
$ jobs
[JOB ID: 1] Executing sleep (PID: 3421)
```
 
**Pipe two commands**
```bash
$ cat records.txt
banana
apple
cherry
apple
 
$ cat records.txt | sort
apple
apple
banana
cherry
```
 
**Chain multiple pipes and save to a file**
```bash
$ cat records.txt | sort | uniq > result.txt
$ cat result.txt
apple
banana
cherry
```
 
**More complex example**
```bash
$ ls -l | awk '{ print $9, $3, $4 }'
```


## Project structure

```
.
├── include/
│   ├── executor.h      # execute(), execute_command(), execute_pipe()
│   ├── parser.h        # cmdline struct definition
│   ├── shell.h         # readline(), terminate()
│   └── utils.h         # xmalloc(), xrealloc(), memory_error()
├── src/
│   ├── main.c          # main function
│   ├── executor.c      # command + pipeline execution, job tracking
│   ├── parser.c        # tokenizer and grammar parser
│   ├── shell.c         # prompt and line reader
│   └── utils.c         # safe memory allocation helpers
├── tests/
│   └── test_main.c
└── Makefile
```

## Build & run

```bash
make
./shell
```

## Implementation highlights

- **Pipeline execution**: a single loop forks one child per command, wiring `pipefd` read/write ends via `dup2` before each `execvp`. The parent tracks all child PIDs and `waitpid`s on each of them for foreground pipelines.
- **Job tracking**: background jobs are stored in a fixed-size `struct job` array. `WNOHANG` is used on every `waitpid` call to reap finished jobs non-blockingly before each new command runs.
- **File truncation**: output redirection opens files with `O_WRONLY | O_CREAT` then calls `ftruncate(..., 0)` to clear existing content, matching standard shell behavior.

## Language & environment

C · Linux · GCC · Make

## Author

**Dipita Sinha**  
Concordia University — Gina Cody School of Engineering and Computer Science  
COEN 346 - Operating Systems

## License

Developed for academic purposes. Not licensed for commercial use.
