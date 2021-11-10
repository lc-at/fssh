#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int pd[2];
	pid_t pid;

	if (pipe(pd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(2);
	} else if (pid == 0) {
		execv("/usr/bin/ssh", argv);
	} else {
		// wait(NULL);
	}
	return 0;
}
