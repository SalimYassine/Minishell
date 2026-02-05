# Minishell - A Minimal UNIX Shell Implementation

## Overview
Minishell is a custom command-line interpreter developed in **C**. It replicates core functionalities of standard shells (like Bash or Zsh), demonstrating deep understanding of **system calls**, **process management**, and **inter-process communication**.

This project handles command execution, pipelines, I/O redirections, and signal management manually without relying on high-level system libraries.

## Features
* **Command Execution:** Parses and executes commands using `fork`, `execvp`, and `waitpid`.
* **Pipelines (`|`):** Supports chaining multiple commands where the output of one becomes the input of the next using `pipe` and `dup2`.
* **Redirections (`<`, `>`):** Handles input and output redirection to/from files.
* **Background Processes (`&`):** Ability to run jobs in the background using `setpgid`.
* **Signal Handling:**
    * `SIGINT` (Ctrl+C) and `SIGTSTP` (Ctrl+Z) are trapped to prevent the shell from crashing.
    * `SIGCHLD` is handled to clean up zombie processes asynchronously.
* **Built-in Commands:**
    * `cd`: Change directory.
    * `exit`: Terminate the shell.
    * `dir`: List directory contents (custom implementation using `opendir`).

## Installation & Usage

### Prerequisites
* GCC Compiler
* Make utility
* Linux/Unix environment

### Compilation
Use the provided Makefile to build the project:
```bash
make
./minishell
make test
./test_readcmd
```
## Technical details

* Language: C
* Memory Management: Dynamic allocation for command parsing structures.
* System Calls: fork, exec, pipe, dup2, signal/sigaction.

## Author

Yassine Salim - Engineering Student at ENSEEIHT (Computer Science & Telecommunications)
