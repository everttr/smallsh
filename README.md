# smallsh
A lightweight linux shell made for CS 374 at OSU in Winter 2024.
By Reed Evertt. No rights reserved.

## Features
The shell can run arbitrary programs, supports piping and running background processes, contains a few built-in commands and macros, and has been error tested within reasonability.

**Supported Syntax/Functions**
- Running programs: `[program/executable] [args]`
	- Run in foreground by default
	- Up to 511 arguments accepted
- Piping in: `[command] > [file]`
- Piping out: `[command] < [file]`
- Background processes: `[command] &`
	- The shell informs the user when the background process terminates
	- Allowance of backgrounding can be toggled with CTRL+Z
- Comments: `#[comment]`
- PID Macro: expands `$$` into the shell's process ID

**Built-In Commands**
- exit: terminates smallsh
- cd: changes working directory to supplied one, or HOME if no arguments given
- status: displays the exit status code of the last foreground process ran

## Building
Use the supplied makefile ('make'/'make clean')
