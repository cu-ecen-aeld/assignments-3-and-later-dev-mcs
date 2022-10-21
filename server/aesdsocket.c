#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <linux/fs.h>

#define DEBUG_AESD

#define CON_BACKLOG 5
#define FILE_NAME "/var/tmp/aesdsocketdata"
#define RECV_BUFF_SIZE  (1 << 8)
#define CREATE_DAEMON 1

static int filefd = 0;
static int finish_loop = 0;

void signal_handler(int signum) {
	if (signum == SIGTERM || SIGINT == signum) {

		syslog(LOG_INFO, "Caught signal, exiting");

		struct stat sa = {0};

		if (stat(FILE_NAME, &sa) == 0) {
			close(filefd);

			// delete the file
			if (unlink(FILE_NAME) == -1) {
				syslog(LOG_ERR, "unlink failed");
				exit(EXIT_FAILURE);
			}
		}

//		finish_loop = true;

		// exit the application.
		exit(EXIT_SUCCESS);
	}
}

int main(int argc, char** argv) {

	char* port_num = "9000";

	// open syslog
	openlog(0, 0, LOG_USER);

	// and we are off!
	syslog(LOG_INFO, "***************************************");
	syslog(LOG_INFO, "aesdsocket started");

#ifdef CREATE_DAEMON
	const int is_daemon = ((getopt(argc, argv, "d") == 'd') ? 1 : 0);
	if (is_daemon)
		syslog(LOG_INFO, "Will run as daemon");
#endif

	struct sigaction action = {0};
	action.sa_handler = signal_handler;
	
	// register the signal handler.
	int res = sigaction(SIGTERM, &action, 0);
	
	if (-1 == res) {
        syslog(LOG_ERR, "sigaction: %s", strerror(errno));
		exit(EXIT_FAILURE);	
    }

	struct addrinfo gai_hints = {0};
    gai_hints.ai_family = AF_INET;
    gai_hints.ai_socktype = SOCK_STREAM;
    gai_hints.ai_flags = AI_PASSIVE; // fill in my ip address    

	struct addrinfo* gai_res = 0;

	res = getaddrinfo(0, port_num, &gai_hints, &gai_res);
	if (0 != res) {
		syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(res));
		exit(EXIT_FAILURE);
	}

#ifdef DEBUG_AESD
	struct addrinfo* p = 0;
    for (p = gai_res; p != 0; p = p->ai_next)
    {
        char hip_buf[INET6_ADDRSTRLEN] = {0};

        struct sockaddr_in* sadin = (struct sockaddr_in*) (p->ai_addr);
        inet_ntop(sadin->sin_family, &(sadin->sin_addr), hip_buf, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Retrieved IP address: %s", hip_buf);
    }
#endif

	// Create the socket
	int sockfd = socket(gai_res->ai_family, gai_res->ai_socktype, gai_res->ai_protocol);
	if (-1 == sockfd) {
		syslog(LOG_ERR, "socket: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	int yes=1;

	// lose the pesky "Address already in use" error message
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
    	syslog(LOG_ERR, "setsockopt failed");
    	exit(EXIT_FAILURE);
	} 

	if (-1 == sockfd) {
		syslog(LOG_ERR, "socket: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	res = bind(sockfd, gai_res->ai_addr, gai_res->ai_addrlen);

	if (-1 == res) {
		syslog(LOG_ERR, "bind: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (gai_res->ai_addr->sa_family == AF_INET) {

		struct sockaddr_in* sadin = (struct sockaddr_in*) (gai_res->ai_addr);
		char hip_buf[INET_ADDRSTRLEN] = {0};
		inet_ntop(AF_INET, &(sadin->sin_addr), hip_buf, INET_ADDRSTRLEN);
		syslog(LOG_INFO, "Host IP address: %s", hip_buf);
    } 
	
	// Free the host info.
	freeaddrinfo(gai_res);

#ifdef CREATE_DAEMON
	if (is_daemon)
	{
		syslog(LOG_INFO, "Creating daemon");

		// Create daemon
		int pid = fork();
		if (-1 == pid) {
			exit(EXIT_FAILURE);
		}
		else if (0 != pid) {
			// Close the parent
			exit(EXIT_SUCCESS);
		}

		// Create a new session.
		if (setsid() == -1) {
			exit(EXIT_FAILURE);
		}

		// set directory to /
		if (chdir("/") == -1) {
			exit(EXIT_FAILURE);
		}

		// close open file descriptors
		int i;
		#define NR_OPEN 4
		for (i = 0 ; i < NR_OPEN; i++)
			close(i);

		// redirect to /dev/null.
		open("/dev/null", O_RDWR); 	// stdin
		dup(0); 					// stdout
		dup(0);						// stderr
	}
#endif

	// listen on the socket 
	if ((res = listen(sockfd, CON_BACKLOG)) == -1) {
		syslog(LOG_ERR, "Listen: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	syslog(LOG_INFO, "listening on socket");

	while (0 == finish_loop)
	{
		char cip_buf[INET_ADDRSTRLEN] = {0};
		struct sockaddr_storage c_addr_info = {0};
    	socklen_t c_addr_info_sz = sizeof(struct sockaddr_storage);

		int cfd = accept(sockfd, (struct sockaddr*) &c_addr_info, &c_addr_info_sz);

		if (-1 == cfd) {
			syslog(LOG_ERR, "accept: %s", strerror(errno));

		} else {

			struct sockaddr_in* cip =  (struct sockaddr_in*) &c_addr_info;
			inet_ntop(c_addr_info.ss_family, &(cip->sin_addr), cip_buf, INET_ADDRSTRLEN);
			syslog(LOG_INFO, "Accepted connection from %s", cip_buf);
		}
	
		char* cr = 0;
		int bytes_read = 0;
		int cur_packet_size = 0;
		char* packet_buf = 0;
		do {
			char readbuf[RECV_BUFF_SIZE + 1] = {0};
			if ((bytes_read = recv(cfd, readbuf, RECV_BUFF_SIZE, 0)) == -1) {
				syslog(LOG_ERR, "readv: %s", strerror(errno));
				exit(EXIT_FAILURE);
			}

			if (bytes_read == 0)
				break;

			cur_packet_size += bytes_read;

			// Create a buffer of the correct size.
			//char* n_packet_buf = (char*) realloc(packet_buf, cur_packet_size + 1);
			if (0 == packet_buf)
				packet_buf = (char*) calloc(1, cur_packet_size + 1);
			else {
				char* tmp_buff = (char*) calloc(1, cur_packet_size + 1);
				if (tmp_buff) {
					strcpy(tmp_buff, packet_buf);
					free(packet_buf);
					syslog(LOG_INFO, "realloced");
				}
				packet_buf = tmp_buff;
			}
				
			if (packet_buf == 0) {
				syslog(LOG_ERR, "realloc failed: returned NULL (0)");
				break;
			}
			
			// Copy the read buffer into the (re)allocated buffer.
			packet_buf = strcat(packet_buf, readbuf);

			cr = strchr(readbuf, '\n');
			if (cr) {
				// we found a carriage return;
				syslog(LOG_INFO, "cr found");
					
			}
		}
		while (!cr);
			
		syslog(LOG_INFO, "bytes_read: %d", cur_packet_size);

		// open the file
    	if ((filefd = open(FILE_NAME, O_RDWR | O_CREAT | O_APPEND, 0644)) == -1) {
        	syslog(LOG_ERR, "open: %s", strerror(errno));
        	exit(EXIT_FAILURE);
    	}
	
		// append to the file
		if (write(filefd, packet_buf, cur_packet_size) == -1) {
			syslog(LOG_ERR, "write: %s", strerror(errno));
		} 	
		
		// Free up memory
        free(packet_buf);

		char filebuf[RECV_BUFF_SIZE + 1] = {0};
		lseek(filefd, 0, SEEK_SET);
		while ((res = read(filefd, filebuf, RECV_BUFF_SIZE)) != 0)
		{
			syslog(LOG_ERR, "filebuffer: %s", filebuf);

			if (send(cfd, filebuf, res, 0) == -1) {
                syslog(LOG_ERR, "send: %s", strerror(errno));
            }
		}

		if (close(filefd) == -1) {
            syslog(LOG_ERR, "close: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

		close(cfd);

		syslog(LOG_INFO, "Closed connection from %s", cip_buf);
	}

	close(sockfd);

	syslog(LOG_INFO, "aesdsocket finishing");

	// close syslog
	closelog();	

	// all good.
	exit(EXIT_SUCCESS);
}
