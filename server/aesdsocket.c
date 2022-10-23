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
#include <pthread.h>

#define CON_BACKLOG 5
#define FILE_NAME "/var/tmp/aesdsocketdata"
#define RECV_BUFF_SIZE  (1 << 6)

struct thread_data
{   
	pthread_t threadId;
    int filefd;
    int cfd;
	char cip_buf[INET_ADDRSTRLEN];
	int finished;
};
	

void* thread_handle_accept(void* param);

static int filefd = 0;

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

		// exit the application.
		exit(EXIT_SUCCESS);
	}
}

int main(int argc, char** argv) {

	// open syslog
	openlog(0, 0, LOG_USER);

	// and we are off!
	syslog(LOG_INFO, "***************************************");
	syslog(LOG_INFO, "aesdsocket started");

	const int is_daemon = ((getopt(argc, argv, "d") == 'd') ? 1 : 0);
	if (is_daemon)
		syslog(LOG_INFO, "Will run as daemon");

	struct sigaction action = {0};
	action.sa_handler = signal_handler;
	
	// register the signal handler.
	int res = sigaction(SIGTERM, &action, 0);
	
	if (-1 == res) {
		perror("sigaction failed: ");
		exit(EXIT_FAILURE);	
    }

	char* port_num = "9000";

	struct addrinfo gai_hints = {0};
    gai_hints.ai_family = AF_INET;
    gai_hints.ai_socktype = SOCK_STREAM;
    gai_hints.ai_flags = AI_PASSIVE; // fill in my ip address    

	struct addrinfo* gai_res = 0;

	res = getaddrinfo(0, port_num, &gai_hints, &gai_res);
	if (0 != res) {
		perror("getaddrinfo failed: ");
		exit(EXIT_FAILURE);
	}

	struct addrinfo* p = 0;
    for (p = gai_res; p != 0; p = p->ai_next)
    {
        char hip_buf[INET6_ADDRSTRLEN] = {0};

        struct sockaddr_in* sadin = (struct sockaddr_in*) (p->ai_addr);
        inet_ntop(sadin->sin_family, &(sadin->sin_addr), hip_buf, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Retrieved IP address: %s", hip_buf);
    }

	int sockfd = socket(gai_res->ai_family, gai_res->ai_socktype, gai_res->ai_protocol);
	if (-1 == sockfd) {
		perror("socket failed: ");
        exit(EXIT_FAILURE);
    }

	int yes=1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
		perror("setsockopt failed: ");
        exit(EXIT_FAILURE);
    }

	res = bind(sockfd, gai_res->ai_addr, gai_res->ai_addrlen);
	if (-1 == res) {
		perror("bind failed: ");
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

	// listen on the socket 
	if ((res = listen(sockfd, CON_BACKLOG)) == -1) {
		perror("listen failed: ");
		exit(EXIT_FAILURE);
	}

	syslog(LOG_INFO, "listening on socket");

	struct thread_data* thread_data_array[CON_BACKLOG] = {0};
	//pthread_t threadId;
	int thread_top = 0;

	while (1)
	{
		struct thread_data* data = malloc(sizeof(struct thread_data));
        if (!data)
            exit(EXIT_FAILURE);

        memset(data->cip_buf, 0, INET_ADDRSTRLEN);
        data->filefd = filefd;
		data->finished = 0;

        struct sockaddr_storage c_addr_info = {0};
        socklen_t c_addr_info_sz = sizeof(struct sockaddr_storage);

        data->cfd = accept(sockfd, (struct sockaddr*) &c_addr_info, &c_addr_info_sz);
        if (-1 == data->cfd) {
			perror("accept failed: ");

        } else {

            struct sockaddr_in* cip =  (struct sockaddr_in*) &c_addr_info;
            inet_ntop(c_addr_info.ss_family, &(cip->sin_addr), data->cip_buf, INET_ADDRSTRLEN);
            syslog(LOG_INFO, "Accepted connection from %s", data->cip_buf);
        }

		thread_data_array[thread_top] = data;

		pthread_create(&(thread_data_array[thread_top]->threadId), 0, &thread_handle_accept, data);
		
		++thread_top;

		int i;
		for (i = 0; i < thread_top; i++) {
			if ((0 != thread_data_array[i]) && (1 == thread_data_array[i]->finished)) {

				pthread_join(thread_data_array[i]->threadId, 0);

				free(thread_data_array[i]);
				
				thread_data_array[i] = thread_data_array[thread_top];	
				thread_data_array[thread_top] = 0;
				thread_top--;
			}
		}
	}

	close(sockfd);

	syslog(LOG_INFO, "aesdsocket finishing");

	// close syslog
	closelog();	

	// all good.
	exit(EXIT_SUCCESS);
}

void* thread_handle_accept(void* param) {

	struct thread_data* data = (struct thread_data*) param;

	if (!data)
		return 0;

	// open the file
    if ((data->filefd = open(FILE_NAME, O_RDWR | O_CREAT | O_APPEND, 0644)) == -1) {
		perror("open failed: ");
        exit(EXIT_FAILURE);
    }

    do {
		int bytes_read = 0;
        char readbuf[RECV_BUFF_SIZE + 1] = {0};

        if ((bytes_read = recv(data->cfd, readbuf, RECV_BUFF_SIZE, 0)) == -1) {
			perror("recv failed: ");
            exit(EXIT_FAILURE);
        }

        if (bytes_read == 0)
        	break;

        // append to the file
       	if (write(data->filefd, readbuf, bytes_read) == -1) {
			perror("accept failed: ");
        }
		
		if (strchr(readbuf, '\n') != 0)
        	break;
	}
    while (1);

	int res = 0;
    char filebuf[RECV_BUFF_SIZE + 1] = {0};
    lseek(data->filefd, 0, SEEK_SET);
    while ((res = read(data->filefd, filebuf, RECV_BUFF_SIZE)) != 0)
    {   
    	syslog(LOG_ERR, "filebuffer: %s", filebuf);

        if (send(data->cfd, filebuf, res, 0) == -1) {
			perror("send failed: ");
        }
    }

    if (close(data->filefd) == -1) {
		perror("close failed: ");
        exit(EXIT_FAILURE);
    }

   	close(data->cfd);

    syslog(LOG_INFO, "Closed connection from %s", data->cip_buf);

	data->finished = 1;

	return 0;
}	
