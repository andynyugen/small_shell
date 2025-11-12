# Small Unix Shell 

This project was for my Operating Systems course. The goal of this assignment was to implement a simplified Unix shell, called smallsh, in C. 
This shell supports a subset of common features from shells like bash.

---

## Features

### Command Prompt
- Prompts the user with a single colon (:) for input.
- Supports commands of up to 2048 characters and 512 arguments.
- Ignores blank lines and lines starting with #.

### Built-in Commands
- exit: Terminates the shell and kills background processes.
- cd [directory]: Changes the current working directory. Defaults to $HOME if no argument is given.
- status: Displays the exit status or terminating signal of the last foreground process.

### External Commands
- Executes non-built-in commands via fork() and execvp().
- Supports input < and output > redirection.
- Executes commands in foreground or background using &.

### Signal Handling
- SIGINT (CTRL+C): Ignored by the shell; terminates foreground child processes.
- SIGTSTP (CTRL+Z): Toggles foreground-only mode, where background execution (&) is ignored.
- Background processes ignore both SIGINT and SIGTSTP.

### Variable Expansion
- Expands $$ in commands to the PID of the smallsh process.

---

## 🚀 How to Run

1. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/nfl-game-predictor.git](https://github.com/andynyugen/small_shell.git
   cd small_shell

 2. Compile
   ```bash
   gcc smallsh.c -o smallsh

  Or using GNU99 standard:
  ```bash
  gcc -std=gnu99 smallsh.c -o smallsh

3. Run
  ```bash
  ./smallsh

---

## Commands

Type commands after the : prompt.
Example built-in commands:
```bash
: cd
: pwd
: status
: exit

Run other commands, with optional input/output redirection:
```bash
: ls > output.txt
: wc < input.txt
: sleep 10 &

Toggle foreground-only mode with CTRL+Z.

---

## Testing

The repository includes a test script p3testscript:
```bash
chmod u+x p3testscript
./p3testscript >&1

This script tests built-in commands, background execution, input/output redirection, and signal handling.
