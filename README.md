# Simple Shell

A Unix shell program written in C, built from scratch as part of a systems programming lab.

## Features

- Execute any system command with or without arguments (`ls`, `ls -l`, `pwd` etc.)
- Run commands in the background with `&` (e.g. `firefox &`)
- Built-in commands:
  - `cd` — change directory (supports `~`, `..`, absolute and relative paths)
  - `echo` — print text with variable expansion
  - `export` — set environment variables (`export x=5`, `export y="hello world"`)
  - `exit` — close the shell
- Variable expansion with `$` anywhere in a command (`ls $x`)
- SIGCHLD signal handler that reaps background processes and logs every child termination to `shell_log.txt`
- No zombie processes

## Build & Run
```bash
gcc -Wall -o simple_shell simple_shell.c
./simple_shell
```

## Example Usage
```bash
Shell> ls
Shell> ls -a -l -h
Shell> export x="-a -l -h"
Shell> ls $x
Shell> cd ~
Shell> echo "Hello $x"
Shell> firefox &
Shell> exit
```

## Concepts Covered

fork(), execvp(), waitpid(), chdir(), signal handling, SIGCHLD, zombie processes