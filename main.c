#define _DEFAULT_SOURCE
#include <unistd.h>
#include <wait.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <pty.h>
#include <poll.h>
#include "config.h"

#ifdef DEBUG
#define dbg_printf(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#else
#define dbg_printf(format, ...)                                                \
	do {                                                                   \
	} while (0)
#endif

pid_t child;
int pty_fdm;
int pty_fds;
char pty_name[20];

int ssh_executor(int argc __attribute__((unused)), char **argv)
{
	close(pty_fdm);

	/* create new session and set pgid */
	if (setsid() < 0)
		perror("setsid");

	/* set new controlling terminal */
	ioctl(pty_fds, TIOCSCTTY, 0);

	/* redirect stdin, stdout, and stderr to pty_fds */
	dup2(pty_fds, STDIN_FILENO);
	dup2(pty_fds, STDOUT_FILENO);
	dup2(pty_fds, STDERR_FILENO);

	dbg_printf("child: stdin is a tty? %d\n", isatty(STDIN_FILENO));

	dbg_printf("child: executing ssh\n");
	execv(SSH_PATH, argv);
	return 0;
}

int main(int argc, char **argv)
{
	/* get current terminal attributes (assume stdin is terminal) */
	struct termios term_init;
	if (tcgetattr(STDIN_FILENO, &term_init) < 0)
		perror("tcgetattr");

	/* get current terminal window size */
	struct winsize term_ws;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &term_ws); /* TODO: handle error */

	/* init pty */
	if (openpty(&pty_fdm, &pty_fds, pty_name, &term_init, &term_ws) < 0)
		perror("openpty");
	dbg_printf("pty {fdm=%d, fds=%d, name=%s}\n", pty_fdm, pty_fds,
		   pty_name);

	child = fork();
	if (child == 0) { /* child process */
		return ssh_executor(argc, argv);
	}

	/* -- main process -- */
	close(pty_fds);

	/* init log file */
	FILE *logf = fopen(LOG_FILE, "a");
	if (logf == NULL) {
		perror("fopen");
	}

	fprintf(logf, "ssh: args=");
	for (int i = 0; i < argc; i++) {
		fprintf(logf, "%s ", argv[i]);
	}
	fprintf(logf, "\n");

	/* set terminal mode to raw */
	dbg_printf("main: set terminal mode to raw\n");
	struct termios term_raw = term_init;
	cfmakeraw(&term_raw);
	tcsetattr(STDIN_FILENO, TCSANOW, &term_raw);

	/* init poll poll fds for pty_fdm and stdin */
	struct pollfd pfds[2];
	pfds[0].fd = pty_fdm;
	pfds[0].events = POLLIN;
	pfds[1].fd = STDIN_FILENO;
	pfds[1].events = POLLIN;

	char *buf[BUF_SIZE];
	while (waitpid(child, NULL, WNOHANG) == 0) {
		/* TODO: handle waitpid() error */

		if (poll(pfds, 2, 0) < 0) {
			perror("poll");
			break;
		}

		if (pfds[0].revents & POLLIN) {
			int rv = read(pty_fdm, buf, BUF_SIZE);
			if (rv == 0)
				break;
			else if (rv == -1)
				perror("read");
			else
				write(STDOUT_FILENO, buf, rv);
		}

		if (pfds[1].revents & POLLIN) {
			int rv = read(STDIN_FILENO, buf, BUF_SIZE);
			if (rv == 0) {
				break;
			} else if (rv == -1) {
				perror("read");
			} else {
				write(pty_fdm, buf, rv);
				fwrite(buf, sizeof(char), rv, logf);
				fflush(logf);
			}
		}
	}

	/* close log file */
	fclose(logf);

	/* restore terminal attributes */
	dbg_printf("main: restore terminal attributes\n");
	tcsetattr(STDIN_FILENO, TCSANOW, &term_init);
	return 0;
}
