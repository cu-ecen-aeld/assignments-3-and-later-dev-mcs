#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv)
{
	openlog(NULL, 0, LOG_USER);
	
	// 1. file name, 2. string text to add.
	if (argc != 3) 	{
		syslog(LOG_ERR, "Usage: file path, text to write\n");
		return 1;
	}

	int fd = -1;
	if ((fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
		syslog(LOG_ERR, "Could not open/create: %s", strerror(errno));
		return 1;
	}

	if (write(fd, argv[2], strlen(argv[2])) == -1) 	{
		syslog(LOG_ERR, "Could not write: %s", strerror(errno));
		return 1;
	}

	syslog(LOG_DEBUG, "Writing %s to %s\n", argv[2], argv[1]);

	if (close(fd) == -1) {
		syslog(LOG_ERR,  "Could not close: %s", strerror(errno));
		return 1;
	}

	// Not actually required to close the syslog according to the man page
	// assume this is handled by the kernel.
	closelog();

	return 0;
}
