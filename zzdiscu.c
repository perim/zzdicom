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

static inline void znwsendbuffer(struct zzfile *zz)
{
	ziflush(zz->zi);
}

#if 0
static void zzdiserviceprovider(struct zzfile *zz)
{
	char str[80];
	uint16_t group, element;
	long len;
	bool loop = true;

	while (loop && zzread(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiNetworkServiceSequence: printf("NEW SERVICE SQ"); break;
		case DCM_DiNetworkService:
			printf("Service = %s\n", zzgetstring(zz, str, sizeof(str) - 1));
			zzwCS(zz, DCM_DiNetworkServiceStatus, "ACCEPTED"); // TODO check validity of above
			znwsendbuffer(zz);
			break;
		case DCM_DiDisconnect: loop = false; break;
		default: break;	// ignore
		}
	}
	ziclose(zz->zi);
}
#endif

// static void zzdistoragerequest(struct zzfile *zz, ...

/// Reap dead child processes
static void sigchld_handler(int s)
{
	(void)s;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

bool zzconnect(struct zzfile *zz, const char *host, int port, const char *theiraetitle, int flags)
{
	int sockfd, numbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char portstr[10];

	(void)flags;
	(void)theiraetitle;

	if (!zz) return NULL;

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
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "client: failed to connect\n");
		return NULL;
	}

	inet_ntop(p->ai_family, p->ai_addr, s, sizeof(s));
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	zz->zi = ziopensocket(sockfd, 0);
	printf("Starting negotiation\n");
	zzdinegotiation(zz);
	return true;
}

int main(void)
{
	struct zzfile szz, *zz;
	zz = zzdinetwork("zzdicom", "TESTNODE", &szz);
	zzconnect(zz, "127.0.0.1", 5104, "TEST", 0);
	return 0;
}
