#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <signal.h>
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

	fd_set rfds;
	fd_set cur_fds;

	tcgetattr(STDIN_FILENO, &oldt);

	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	masterfd = posix_openpt(O_RDWR);
	grantpt(masterfd);
	unlockpt(masterfd);

	slavename = ptsname(masterfd);
	slavefd = open(slavename, O_RDWR);

	printf("slave name: %s, slave fd: %d\n", slavename, slavefd);

	child = fork();
	if (child == -1) {
		perror("fork failed");
		exit(EXIT_FAILURE);
	} else if (child == 0) {
		close(masterfd);
		setsid();
		ioctl(slavefd, TIOCSCTTY, 1);
		tcsetattr(slavefd, TCSANOW, &oldt);
		dup2(slavefd, STDIN_FILENO);
		execv("/usr/bin/ssh", argv);
	} else {
		close(slavefd);

		FD_ZERO(&rfds);
		FD_SET(STDIN_FILENO, &rfds);
		FD_SET(masterfd, &rfds);

		output = fopen("output", "a");
		setbuf(output, NULL);

		while (waitpid(child, NULL, WNOHANG) == 0) {
			cur_fds = rfds;
			select(FD_SETSIZE, &cur_fds, NULL, NULL, NULL);

			for (int fd = 0; fd < FD_SETSIZE; fd++) {
				if (!FD_ISSET(fd, &cur_fds))
					continue;
				if (fd == STDIN_FILENO) {
					if (read(STDIN_FILENO, &buf, 1) == 1) {
						write(masterfd, &buf, 1);
						fputc(buf, output);
					}
				} else if (fd == masterfd) {
					if (read(masterfd, &buf, 1) == 1) {
						write(STDOUT_FILENO, &buf, 1);
					}
				}
			}
			fflush(stdout);
		}

		printf("file closed\n");
		fclose(output);
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	close(masterfd);
	close(slavefd);

	return 0;
}
