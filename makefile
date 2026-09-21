CC=gcc --std=gnu99 -g
exe=smallsh

${exe}: builtincommands.o
	${CC} builtincommands.o main.c -o ${exe}

builtincommands.o:
	${CC} -c builtincommands.c

clean:
	rm -f *.o ${exe}

