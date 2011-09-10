// Experimental alternative network protocol for transfering DICOM data

#include "zzio.h"
#include "zzwrite.h"
#include "zzditags.h"
#include "zzdinetwork.h"

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

// UID_LittleEndianExplicitTransferSyntax
// UID_JPEGLSLosslessTransferSyntax

#define BACKLOG 10

static inline void znwsendbuffer(struct zzfile *zz)
{
	ziflush(zz->zi);
}

static void zzdiserviceprovider(struct zzfile *zz)
{
	char str[80];
	uint16_t group, element;
	long len;
	bool loop = true;

	zzdinegotiation(zz);

	while (loop && zzread(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiNetworkServiceSequence: printf("NEW SERVICE SQ\n"); break;
		case DCM_DiNetworkService:
			printf("Service = %s\n", zzgetstring(zz, str, sizeof(str) - 1));
			zzwCS(zz, DCM_DiNetworkServiceStatus, "ACCEPTED"); // TODO check validity of above
			znwsendbuffer(zz);
			break;
		case DCM_DiDisconnect: loop = false; printf("-- disconnect --\n"); break;
		default: printf("unknown tag\n"); break;	// ignore
		}
	}
	ziclose(zz->zi);
}

/// Reap dead child processes
static void sigchld_handler(int s)
{
	(void)s;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

bool zzlisten(struct zzfile *zz, int port, const char *myaetitle, int flags)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	char portstr[10];
	union socketfamily their_addr; // connector's address information

	(void)myaetitle;
	(void)flags;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	sprintf(portstr, "%d", port);
	if ((rv = getaddrinfo(NULL, portstr, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return false;
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
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL) 
	{
		fprintf(stderr, "server: failed to bind\n");
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

	printf("server: waiting for connections...\n");

	while (true)
	{
		sin_size = sizeof(their_addr);
		new_fd = accept(sockfd, &their_addr.sa, &sin_size);
		if (new_fd == -1)
		{
			perror("accept");
			continue;
		}

		if (their_addr.sa.sa_family == AF_INET)
		{
			inet_ntop(their_addr.sa_stor.ss_family, &their_addr.sa_in.sin_addr, s, sizeof(s));	// ipv4
			printf("server: got ipv4 connection from %s\n", s);
		}
		else
		{
			inet_ntop(their_addr.sa_stor.ss_family, &their_addr.sa_in6.sin6_addr, s, sizeof(s));	// ipv6
			printf("server: got ipv6 connection from %s\n", s);
		}

//		if (!fork())
		{
			// this is the child process
			close(sockfd); // child doesn't need the listener
			zz->zi = ziopensocket(new_fd, 0);
			printf("Starting negotiation\n");
			zzdiserviceprovider(zz);
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return true;
}

int main(void)
{
	struct zzfile szz, *zz;
	zz = zzdinetwork("zzdicom", "TESTNODE", &szz);
	zzlisten(zz, 5104, "TEST", 0);
	return 0;
}
