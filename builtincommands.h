/*
 * File: builtincommands.h
 * Author: Reed Evertt
 * Email: evertt@oregonstate.edu
 * Date: 2/23/24
 * Description: header file for built-in
 * commands.
 */

#ifndef BUILTINCOMMANDS_H
#define BUILTINCOMMANDS_H

#include <unistd.h>

// Defines list for keeping track of background child processes
struct childNode
{
	pid_t pid;
	struct childNode* next;
};
struct childNode* childProcesses;

// Defines interface for listing built in programs like "cd"
struct builtInProgram
{
	const char* name;
	int (*func)(char** argv, int argc);
};

// Defines list of built in programs
struct builtInProgram* builtInCommands;
int builtInCommandsNum;
void initBuiltInPrograms();

// For keeping track of exit status of last ran program
int lastForegroundStatus;

// For remembering currently running foreground process
pid_t foregroundProcess;

#endif
