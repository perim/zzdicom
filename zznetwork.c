#include "zznetwork.h"
#include "zzio.h"
#include "zzwrite.h"
#include "zzditags.h"

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
struct zznetwork *zznetlisten(const char *interface, int port, int flags)
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
	struct zznetwork *zzn = calloc(1, sizeof(*zzn));
	zzn->in = calloc(1, sizeof(*zzn->in));
	zzn->out = calloc(1, sizeof(*zzn->out));
	zzn->server = true;

	setzz(zzn->in, interface);
	setzz(zzn->out, interface);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	sprintf(portstr, "%d", port);
	if ((rv = getaddrinfo(NULL, portstr, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		goto failure;
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
		goto failure;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("listen");
		goto failure;
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
	{
		perror("sigaction");
		goto failure;
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
		memset(zzn->in->net.address, 0, sizeof(zzn->in->net.address));
		if (their_addr.sa.sa_family == AF_INET)
		{
			inet_ntop(their_addr.sa_stor.ss_family, &their_addr.sa_in.sin_addr, s, sizeof(s));	// ipv4
			strncpy(zzn->in->net.address, s, sizeof(zzn->in->net.address) - 1);
			//printf("server: got ipv4 connection from %s\n", s);
		}
		else
		{
			inet_ntop(their_addr.sa_stor.ss_family, &their_addr.sa_in6.sin6_addr, s, sizeof(s));	// ipv6
			strncpy(zzn->in->net.address, s, sizeof(zzn->in->net.address) - 1);
			//printf("server: got ipv6 connection from %s\n", s);
		}
		memcpy(zzn->out->net.address, zzn->in->net.address, sizeof(zzn->out->net.address));

		if (flags & ZZNET_NO_FORK || !fork())
		{
			// this is the child process
			close(sockfd); // child doesn't need the listener
			zzn->in->zi = ziopensocket(new_fd, 0);
			zzn->out->zi = zzn->in->zi;
			return zzn;
		}
		close(new_fd);  // parent doesn't need this
	}
failure:
	free(zzn->in);
	free(zzn->out);
	free(zzn);
	return NULL;
}

// FIXME: Use interface
struct zznetwork *zznetconnect(const char *interface, const char *host, int port, int flags)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char portstr[10];
	struct zznetwork *zzn = calloc(1, sizeof(*zzn));
	zzn->in = calloc(1, sizeof(*zzn->in));
	zzn->out = calloc(1, sizeof(*zzn->out));

	setzz(zzn->in, interface);
	setzz(zzn->out, interface);

	(void)flags;

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
		fprintf(stderr, "client: failed to connect\n");
		zznetclose(zzn);
		freeaddrinfo(servinfo); // all done with this structure
		return NULL;
	}

	inet_ntop(p->ai_family, p->ai_addr, s, sizeof(s));
	memset(zzn->in->net.address, 0, sizeof(zzn->in->net.address));
	strncpy(zzn->in->net.address, s, sizeof(zzn->in->net.address) - 1);
	memcpy(zzn->out->net.address, zzn->in->net.address, sizeof(zzn->out->net.address));
	//printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	zzn->in->zi = ziopensocket(sockfd, 0);
	zzn->out->zi = zzn->in->zi;
	zzn->server = false;
	return zzn;
}

struct zznetwork *zznetclose(struct zznetwork *zzn)
{
	zzclose(zzn->in);
	zzn->out->zi = NULL; // since we refer to same descriptor, make sure don't free twice
	zzclose(zzn->out);
	if (zzn->trace)
	{
		zzwItem_end(zzn->trace, NULL); // finalize last entry
		zzwSQ_end(zzn->trace, NULL); // finalize trace sequence
		zzclose(zzn->trace); // close last
		free(zzn->trace);
	}
	free(zzn->in);
	free(zzn->out);
	free(zzn);
	return NULL;
}

void zznettrace(struct zznetwork *zzn, const char *filename)
{
	char buf[100];
	struct zzfile *tracefile = calloc(1, sizeof(*tracefile));

	memset(buf, 0, sizeof(buf));
	zzn->trace = zzcreate(filename, tracefile, "1.2.3.4", zzmakeuid(buf, sizeof(buf) - 1),
	                      UID_LittleEndianExplicitTransferSyntax);
	if (zzn->trace)
	{
		// We only need to set it for 'in', since they share the same descriptor
		zitee(zzn->in->zi, zzn->trace->zi, ZZIO_TEE_READ | ZZIO_TEE_WRITE);
		// Start first sequence
		zzwSQ_begin(zzn->trace, DCM_DiTraceSequence, NULL);
	}
}
