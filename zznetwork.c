#include "zznetwork.h"
#include "zzio.h"

#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

/// Multi-family socket end-point address
union socketfamily
{
    struct sockaddr sa;
    struct sockaddr_in sa_in;
    struct sockaddr_in6 sa_in6;
    struct sockaddr_storage sa_stor;
};

#define BACKLOG 10

static void setzz(struct zzfile *zz, const char *interface)
{
	memset(zz, 0, sizeof(*zz));
	if (interface)
	{
		strncpy(zz->net.interface, interface, sizeof(zz->net.interface) - 1);
	}
	zz->ladder[0].item = -1;
	zz->current.frame = -1;
	zz->frames = -1;
	zz->pxOffsetTable = -1;
	zz->fileSize = UNLIMITED;
}

/// Reap dead child processes
static void sigchld_handler(int s)
{
	(void)s;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// FIXME: Use interface
struct zzfile *zznetlisten(const char *interface, int port, struct zzfile *zz, int flags)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	socklen_t sin_size;
	struct sigaction sa;
	char s[INET6_ADDRSTRLEN];
	int yes = 1;
	int rv;
	char portstr[10];
	union socketfamily their_addr; // connector's address information

	setzz(zz, interface);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	sprintf(portstr, "%d", port);
	if ((rv = getaddrinfo(NULL, portstr, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return NULL;
	}

	// loop through all the results and bind to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
		{
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			//perror("server: bind");
			continue;
		}
		break;
	}
	if (p == NULL) 
	{
		//fprintf(stderr, "server: failed to bind\n");
		return false;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		return false;
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
		return false;
	}

	//printf("server: waiting for connections...\n");

	while (true)
	{
		sin_size = sizeof(their_addr);
		new_fd = accept(sockfd, &their_addr.sa, &sin_size);
		if (new_fd == -1)
		{
			perror("accept");
			continue;
		}
		memset(zz->net.address, 0, sizeof(zz->net.address));
		if (their_addr.sa.sa_family == AF_INET)
		{
			inet_ntop(their_addr.sa_stor.ss_family, &their_addr.sa_in.sin_addr, s, sizeof(s));	// ipv4
			strncpy(zz->net.address, s, sizeof(zz->net.address) - 1);
			//printf("server: got ipv4 connection from %s\n", s);
		}
		else
		{
			inet_ntop(their_addr.sa_stor.ss_family, &their_addr.sa_in6.sin6_addr, s, sizeof(s));	// ipv6
			strncpy(zz->net.address, s, sizeof(zz->net.address) - 1);
			//printf("server: got ipv6 connection from %s\n", s);
		}

		if (flags & ZZNET_NO_FORK || !fork())
		{
			// this is the child process
			close(sockfd); // child doesn't need the listener
			zz->zi = ziopensocket(new_fd, 0);
			return zz;
		}
		close(new_fd);  // parent doesn't need this
	}
	return NULL; // we never get here
}

// FIXME: Use interface
struct zzfile *zznetconnect(const char *interface, const char *host, int port, struct zzfile *zz,	int flags)
{
	int sockfd, numbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char portstr[10];

	(void)flags;

	setzz(zz, interface);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	sprintf(portstr, "%d", port);
	if ((rv = getaddrinfo(host, portstr, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return NULL;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			//perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL)
	{
		//fprintf(stderr, "client: failed to connect\n");
		return NULL;
	}

	inet_ntop(p->ai_family, p->ai_addr, s, sizeof(s));
	memset(zz->net.address, 0, sizeof(zz->net.address));
	strncpy(zz->net.address, s, sizeof(zz->net.address) - 1);
	//printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	zz->zi = ziopensocket(sockfd, 0);
	return zz;
}
