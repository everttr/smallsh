/*
 * Program: smallsh
 * Author: Reed Evertt
 * Email: evertt@oregonstate.edu
 * Date: 2/23/24
 * Description: a simplified shell that allows running
 * built in programs, outside programs, piping I/O,
 * foreground/background processes, and a few other things
 */

#ifndef MAIN_C
#define MAIN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include "builtincommands.h"

// For debugging purposes
//#define EXPANSION_LOGS

// Defines a structure for holding a (potentially runnable)
// command
struct command
{
	// Name of program to run
	char* program;
	// Array of arguments for program
	// first one being the name of the program
	char** argv;
	// Number of arguments given
	// including name of program
	int argc;
	// File to get input from
	// NULL for default:
	// (stdin for foreground, /dev/null for background)
	char* pipeIn;
	// File to write output to
	// NULL for default:
	// (stdout for foreground, /dev/null for background)
	char* pipeOut;
	// 1 for running in foreground, 0 for background
	int isForeground;
};


// Some globals/constants
const char* LINE_STARTER = ": "; // string that begins every command line
const int LINE_STARTER_LEN = 2;
const char* PIPE_IN = ">";
const char* PIPE_OUT = "<";
const char* EXPANSION_VAR = "$$";
const char* BACKGROUND = "&";
const char* ENTER_FOREGROUND = "\nEntering foreground-only mode (& is now ignored)\n";
const int ENTER_FOREGROUND_LEN = 50;
const char* EXIT_FOREGROUND = "\nExiting foreground-only mode\n";
const int EXIT_FOREGROUND_LEN = 30;
const int MAX_INPUT_BUFFER = 2048;
const int MAX_ARGS = 512; // Maximum arguments for a command (includes name of program)
int backgroundAllowed = 1;

// Tries to find a built in command with a given name
// If found, runs it in the current process with the given arguments
// and returns 1. SPECIAL CASE WHEN RETURNS -1: means request exit of shell
// If not found, returns 0
int tryRunBuiltInCommand(struct command* com)
{
	// Loop through all commands & check equality
	for (int i = 0; i < builtInCommandsNum; i++)
	{
		// If the names match, then call the function
		if (strcmp(builtInCommands[i].name, com->program) == 0)
		{
			int result = builtInCommands[i].func(com->argv, com->argc);
			// Special case for "exit" command,
			// this is the way we communicate with the higher-level
			// shell functions
			if (result == -1)
				return -1;
			return 1;
		}
	}
	
	// If not found, it failed
	return 0;
}

// Flushes stdin because we can't use fflush(), and
// we need a way to make sure stdin is clear before
// prompting the user for input
void flushUserInput()
{
	// Repeatedly reads characters until stdin is empty
	while ((fseek(stdin, 0, SEEK_END), ftell(stdin)) > 0)
		getchar();
}

// Forks this process & either runs the process
// in the foreground/background. If foreground, only returns
// once child process finishes.
// OR just runs in foreground if it's a built in command.
// Always returns 0, unless an exit is requested, in which case it returns 1
int runCommand(struct command* com)
{
	// Check if it's a built-in command
	int res = tryRunBuiltInCommand(com);
	if (res != 0) // then it's a built-in command
	{
		// Special case for exit command to signal to shell
		// to close itself
		if (res == -1)
			return 1;
		return 0;
	}
	
	// Fork process
	pid_t spawnpid = fork();
	
	switch (spawnpid)
	{
		case -1:
			// Error case:
			// Just return, can't really do anything about it.
			return 0;

		case 0: ;
			// Child case:
			// Possibly try to redirect input/output
			// Set in to desired file, or dev/null if in background
			char* in = NULL;
			if (com->pipeIn == NULL)
			{
				if (!com->isForeground)
					in = "/dev/null";
			}
			else
				in = com->pipeIn;
			// redirect input 
			if (in != NULL)
			{
				// Open file
				int newInFD = open(in, O_RDONLY, 0644);
				// Exit if can't open it
				if (newInFD == -1)
				{
					printf("smallsh: %s: No such file or directory\n", in); fflush(stdout);
					exit(1);
				}
				// Otherwise redirect stdin
				int res = dup2(newInFD, STDIN_FILENO);
				// Exit if can't redirect
				if (res == -1)
					exit(1);
			}
			// Set out to desired file or dev/null if in background
			char* out = NULL;
			if (com->pipeOut == NULL)
			{
				if (!com->isForeground)
					out = "/dev/null";
			}
			else
				out = com->pipeOut;
			// redirect output 
			if (out != NULL)
			{
				// Open file
				int newOutFD = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
				// Exit if can't open it
				if (newOutFD == -1)
				{
					printf("smallsh: %s: No such file or directory\n", out); fflush(stdout);
					exit(1);
				}
				// Otherwise redirect stdout
				int res = dup2(newOutFD, STDOUT_FILENO);
				// Exit if can't redirect
				if (res == -1)
					exit(1);
			}
			// redirect error to not be the terminal if it's in the background
			if (!com->isForeground)
			{
				// Open file
				int newErrFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
				// Exit if can't open it
				if (newErrFD == -1)
					exit(1);
				// Otherwise redirect stderr
				int res = dup2(newErrFD, STDERR_FILENO);
				// Exit if can't redirect
				if (res == -1)
					exit(1);
			}

			
			// If foreground, make terminate on SIGINT
			if (com->isForeground)
			{
				// Handles SIGINT
				struct sigaction intAction;
				intAction.sa_flags = 0;
				// Makes it default behavior (crashing)
				intAction.sa_handler = SIG_DFL;
				// Make mask ignore SIGTSTP
				sigemptyset(&intAction.sa_mask);
				sigaddset(&intAction.sa_mask, SIGTSTP);
				// Add action
				sigaction(SIGINT, &intAction, NULL);
			}

			// Makes it NOT terminate on SIGTSTP
			struct sigaction stpAction = {0};
			// Makes it ignore it
			stpAction.sa_handler = SIG_IGN;
			// Make mask ignore SIGTSTP
			sigemptyset(&stpAction.sa_mask);
			sigaddset(&stpAction.sa_mask, SIGTSTP);
			// Add action
			sigaction(SIGTSTP, &stpAction, NULL);
			
			// Try exec as program
			execvp(com->program, com->argv);

			// If continues on (it failed), print error message,
			// then exit with error code
			printf("smallsh: %s: command not found\n", com->program); fflush(stdout);
			exit(1);

		default:
			// Parent case:
			// If foreground, wait until child finishes
			if (com->isForeground || !backgroundAllowed)
			{
				// Save foreground pid
				foregroundProcess = spawnpid;

				// Save background state, to display message on end of wait if
				// it changes
				int savedBackgroundAllowed = backgroundAllowed;

				// Then just wait for it!
				int exitStatus = -25;
				waitpid(spawnpid, &exitStatus, 0);

				// Check if process ended with a signal
				if (WIFSIGNALED(exitStatus))
				{
					// If so, inform user about this
					printf("terminated by signal %d\n", WTERMSIG(exitStatus)); fflush(stdout);
				}
				
				// Save status/signal so user can read it later
				if (WIFEXITED(exitStatus))
					lastForegroundStatus = WEXITSTATUS(exitStatus);
				else
					lastForegroundStatus = WTERMSIG(exitStatus);
				
				// Check if background state changed during waiting
				// (AKA if CTR+Z was pressed)
				if (savedBackgroundAllowed != backgroundAllowed)
				{
					// Then display message about it
					if (backgroundAllowed)
						printf(EXIT_FOREGROUND);
					else
						printf(ENTER_FOREGROUND);
					fflush(stdout);
				}

				// Remove foreground tracker
				foregroundProcess = -1;

				// Also clear input buffer, in case the other process didn't
				// clear it out (or the user spammed the keyboard during a
				// sleep call or something)
				// (actually don't do this because it breaks the test script
				// because of something to do with the end of file not being
				// reached I believe)
				//flushUserInput();
			}
			// If background, have shell just continue on
			else
			{
				// Save it in list of background processes
				// so it can be checked on later when it ends
				struct childNode* newProc = malloc(sizeof(struct childNode));
				newProc->pid = spawnpid;
				newProc->next = childProcesses;
				childProcesses = newProc;

				// Inform user about background process
				printf("background pid is %d\n", spawnpid); fflush(stdout);
			}
	}

	return 0;
}

// Parses a command line given by the user into a
// potentially runnable command. If line is invalid for some reason,
// returns NULL
struct command* parseCommand(char* line)
{
	// Parse first arg
	char** toks = malloc(sizeof(char*));
	char* delim = " ";
	toks[0] = strtok(line, delim);

	if (toks[0] == NULL) // Exit if no first arg somehow
		return NULL;
	
	// Parse every line until end
	int len = 1;
	char* curr = NULL;
	while ((curr = strtok(NULL, delim)) != NULL)
	{
		// Increase size of array
		// (don't want to use a smarter algorithm. sue me)
		len++;
		toks = realloc(toks, sizeof(char*) * len);

		// Add new arg
		toks[len-1] = curr;
	}

	// Create object for command data
	struct command* com = malloc(sizeof(struct command));
	com->argv = NULL;
	com->argc = 0;
	com->pipeIn = NULL;
	com->pipeOut = NULL;
	com->isForeground = 1;
	int endI = len - 1; // for keeping track of last element evaluated
	int evaled = 0; // for keeping track of how many elements evaled

	// Use first token as program name
	com->program = toks[0];
	evaled++;

	// Evaluate foreground/background state
	// by presence of '&' at the end
	if (len >= 2 && strcmp(toks[len-1], BACKGROUND) == 0)
	{
		if (backgroundAllowed)
			com->isForeground = 0;
		evaled++;
		endI--;
	}
	
	// try find pipe in
	if ((len - evaled) >= 2 && strcmp(toks[endI-1], PIPE_IN) == 0)
	{
		com->pipeOut = toks[endI];
		evaled += 2;
		endI -= 2;
	}

	// try find pipe out
	if ((len - evaled) >= 2 && strcmp(toks[endI-1], PIPE_OUT) == 0)
	{
		com->pipeIn = toks[endI];
		evaled += 2;
		endI -= 2;
	}
	
	// Interprets the rest as misc arguments
	com->argc = (len - evaled);
	// Clamps argc just in case it's too big
	if (com->argc > MAX_ARGS - 1)
	{
		com->argc = MAX_ARGS - 1;
		endI = MAX_ARGS - 1;
	}
	if (com->argc > 0)
	{
		com->argv = malloc(sizeof(char*) * (com->argc + 2));
		for (int i = 1; i <= endI; i++)
		{
			com->argv[i] = toks[i];
		}
	}
	// Special case for 0 args, AKA only program name
	else
	{
		com->argv = malloc(sizeof(char*) * 2);
	}
	
	// Add 1 for program name
	com->argc++;
	com->argv[0] = com->program;
	
	// Make array null terminating
	com->argv[com->argc] = NULL;

	// Free memory
	free(toks);

	return com;
}

// Simply returns 0 if command is an empty line or
// a comment. Returns 1 otherwise
int isCommand(char* line)
{
	// Empty strings or those starting with comments aren't commands
	if (line[0] == '\0' || line[0] == '#')
		return 0;
	
	// Check against all whitespace characters
	int i = 0;
	do
	{
		// It's a command if there is some non-whitespace
		// characters
		if (line[i] != ' ' && line[i] != '\t'
			&& line[i] != '\n')
			return 1;
		// It is NOT a command if it begins with whitespace
		// (technically a command like " ls" might be valid if we
		// cut off the leading whitespace, but the assignment
		// specs don't clarify this so it's easier just to
		// interpret it as a comment)
		else if (i == 0)
			return 0;

	} while (line[i] != '\0');

	// If found only whitespace, not a command
	return 0;
}

// Expands a line with all possible expansions that can be done
// (which is just $$ => PID)
char* expandLine(char* line)
{
	// Counts the number of $$'s in the line
	int pidCount = 0;
	int i = 0;
	while (line[i] != '\0')
	{
		// If "$$" is matched, increase count
		// and skip next character
		if (line[i] == EXPANSION_VAR[0] && line[i+1] == EXPANSION_VAR[1])
		{
			++pidCount;
			++i;
		}
		++i;
	}
	
	// Quits if nothing needs to be replaced
	if (pidCount == 0)
		return line;

	// Otherwise, creates new buffer & adds in replacements
	
	// Create string from PID
	int pid = getpid();
	char pidStr[32];
	int pidLen = sprintf(pidStr, "%d", pid);
	// Creates the new string buffer
	int newLen = i + (pidLen - 2) * pidCount;
	char* newBuffer = malloc(newLen * sizeof(char));
	
#if EXPANSION_LOGS
	// Info for debugging expansion
	printf("# of $$'s: %d. oldLen: %d, newLen: %d, pidLen: %d, pid: %d, pidStr: %s\n", pidCount, i, newLen, pidLen, pid, pidStr);
#endif

	// Create/reset new indices
	int newI = 0;
	i = 0;

	// Loop through string again, this time copying the original string
	// && placing the PID in place of the expansion variable
	while (newI < newLen)
	{
#if EXPANSION_LOGS
		printf("i: %d, newI: %d. orig char: %c. ", i, newI, line[i]);
#endif

		// Check if this is where PID is needed
		if (line[i] == EXPANSION_VAR[0] && line[i+1] == EXPANSION_VAR[1])
		{
			// Write PID to buffer
			int endOfWrite = 0;
			for (int pidI = 0; pidI < pidLen; pidI++)
			{
				endOfWrite = newI + pidI;
				newBuffer[endOfWrite] = pidStr[pidI];
			}

			// Increment counters to skip $$ and PID chars respectively
			++i;
			newI = endOfWrite;
		}
		// Otherwise just copy string like normal
		else
			newBuffer[newI] = line[i];
		
		++i;
		++newI;
#if EXPANSION_LOGS
		printf("| NOW, i: %d, newI: %d. last placed char: %c\n", i, newI, newBuffer[newI-1]);
#endif
	}
	// Add null terminator
	newBuffer[newI] = '\0';

	// Frees old buffer
	free(line);

	return newBuffer;
}

// Prompt user for command, make user enter command,
// parse & then try to run command.
// Always returns 0 UNLESS exit command is run & shell should
// shut itself down
int takeAndExecuteSingleLine()
{
	int returnVal = 0;

	// Prompts user for input
	printf(LINE_STARTER); fflush(stdout);

	// Creates buffer for input
	ssize_t bufferSize = MAX_INPUT_BUFFER;
	char* buffer = malloc(bufferSize * sizeof(char));

	// Read input from user
	errno = 0;
	int len = getline(&buffer, &bufferSize, stdin);
	
	// Exit if line not read
	if (len == -1)
	{
		// Also clear error if it encountered one
		if (errno == EINTR)
		{
			printf("\n"); fflush(stdout);
			errno = 0;
		}
		goto End;
	}
	
	// Ends if input is comment/empty
	if (!isCommand(buffer))
		goto End;
	
	// Removes newline at end if it exists
	// we don't want to call the program "ls\n"
	if (len >= 2)
	{
		len--;
		buffer[len] = '\0';
	}

	// Expands input
	buffer = expandLine(buffer);	
	
	// Tries to parse input as command
	struct command* com;
	com = parseCommand(buffer);
	
	// Ends if command failed to parse
	if (com == NULL)
		goto End;
	
	// Attempts to run command
	returnVal = runCommand(com);
	free(com->argv);
	free(com);

	End:
	free(buffer);
	return returnVal;
}

// Checks if any child processes have died since the last time checked,
// if so, prints out info to user
void checkForFinishedBackgrounds()
{
	// Loops through all child processes & asks OS if
	// they've died. If so, tell the user
	struct childNode** currRef = &childProcesses;
	while (*currRef != NULL)
	{
		// Ask OS if this child has died. Make sure to not actually
		// wait during the titular function
		int exitStatus = -5;
		int returnVal = waitpid((*currRef)->pid, &exitStatus, WNOHANG);

		// if return value is 0 then it is still running
		if (returnVal != 0)
		{
			// Tell user it ended and how it ended
			if (WIFSIGNALED(exitStatus))
				printf("background pid %d is done: terminated by signal %d\n",
					(*currRef)->pid,
					WTERMSIG(exitStatus));
			else
				printf("background pid %d is done: exit value %d\n",
					(*currRef)->pid,
					WEXITSTATUS(exitStatus));
			fflush(stdout);

			// Remove this child process from the list
			struct childNode* temp = *currRef;
			*currRef = (*currRef)->next;
			free(temp);
		}
		else
		{
			// Otherwise just start checking the next process as normal
			currRef = &(*currRef)->next;
		}
	}
}

// Called on CTRL+Z. Toggles allowing background processes
void toggleBackgroundProcesses()
{
	// Toggle variable
	backgroundAllowed = !backgroundAllowed;
	
	// If there is no foreground process, output message
	// (if there is one it prints in a separate way)
	// (but be sure to not use printf)
	if (foregroundProcess == -1)
	{
		// Output message based on boolean
		if (backgroundAllowed)
			write(STDOUT_FILENO, EXIT_FOREGROUND, EXIT_FOREGROUND_LEN);
		else
			write(STDOUT_FILENO, ENTER_FOREGROUND, ENTER_FOREGROUND_LEN);
		// Also have to write line starter,
		// because it resumes in the middle of the getline call
		write(STDOUT_FILENO, LINE_STARTER, LINE_STARTER_LEN);
	}
}

// Initializes signal handling for shell
void initSignalHandling()
{
	// Handles SIGINT
	struct sigaction intAction;
	intAction.sa_flags = 0;
	// Makes it ignore it
	intAction.sa_handler = SIG_IGN;
	// Make mask ignore SIGINT
	sigemptyset(&intAction.sa_mask);
	sigaddset(&intAction.sa_mask, SIGINT);
	// Add action
	sigaction(SIGINT, &intAction, NULL);

	// Handles SIGTSTP
	struct sigaction outAction;
	// Make it restart signal calls (so getline doesn't
	// stop blocking all of a sudden because the OS call
	// was interrupted and it takes 20 hours to debug :)
	outAction.sa_flags = SA_RESTART;
	// Adds handler to prevent background
	outAction.sa_handler = toggleBackgroundProcesses;
	// Makes it ignore SIGINT and itself
	sigemptyset(&outAction.sa_mask);
	sigaddset(&outAction.sa_mask, SIGINT);
	sigaddset(&outAction.sa_mask, SIGTSTP);
	// Add action
	sigaction(SIGTSTP, &outAction, NULL);
}

int main()
{
	// Init program & globals
	initSignalHandling();
	foregroundProcess = -1;
	childProcesses = NULL;
	initBuiltInPrograms();

	// Command loop (continue until exit command run,
	// which returns 1)
	while (takeAndExecuteSingleLine() == 0)
	{
		// Before next line, check for background processes that have finished
		// and possibly inform user about them
		checkForFinishedBackgrounds();
	}

	// Shut down process
	return 0;
}

#endif
