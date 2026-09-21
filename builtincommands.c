/*
 * File: builtincommands
 * Author: Reed Evertt
 * Email: evertt@oregonstate.edu
 * Date: 2/23/24
 * Description: contains code for all the built-in
 * commands for smallsh. Separated out just to shorten
 * main.c's line count mostly.
 */

#ifndef BUILTINCOMMANDS_C
#define BUILTINCOMMANDS_C

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "builtincommands.h"

// Just exits the program & closes all child
// processes
int exitFunc(char** argv, int argc)
{
	// Kill foreground process (if present)
	if (foregroundProcess != -1)
		kill(foregroundProcess, SIGTERM);
	foregroundProcess = -1;
	
	// Kill all background processes (if present)
	struct childNode* curr = childProcesses;
	while (curr != NULL)
	{
		kill(curr->pid, SIGTERM);

		// Might as well clean up memory even if the shell
		// is about to die
		struct childNode* temp = curr;
		curr = curr->next;
		free(temp);
	}
	
	// Then just return code for exiting
	return -1;
}

// Changes the current working directory
int cdFunc(char** argv, int argc)
{
	// If no argument is given
	if (argc == 1)
	{
		// Then redirect to home
		chdir(getenv("HOME"));
		return 0;
	}
	// If 1 argument, attempt to change dir to given one
	else if (argc == 2)
	{
		// chdir already accepts both relative &
		// absolute paths, so just pass it the argument
		int result = chdir(argv[1]);
		// Return error if dir couldn't be accessed
		if (result == -1)
			return 1;
		return 0;
	}
	// Error if more than 1 argument
	else
	{
		return 1;
	}
}

// Prints the exit status of the last foreground process
int statusFunc(char** argv, int argc)
{
	printf("exit value %d\n", lastForegroundStatus);
	fflush(stdout);
	return 0;
}

// Initializes the array of built in commands
// for main to call
void initBuiltInPrograms()
{
	builtInCommandsNum = 3;
	builtInCommands = malloc(sizeof(struct builtInProgram)
		* builtInCommandsNum);
	builtInCommands[0].name = "exit";
	builtInCommands[0].func = exitFunc;
	builtInCommands[1].name = "cd";
	builtInCommands[1].func = cdFunc;
	builtInCommands[2].name = "status";
	builtInCommands[2].func = statusFunc;
}

#endif
