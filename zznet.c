// Support for the DICOM network protocol. Apologies for the code that follows, it is hard to write elegant code to support
// such a hideous, bloated and grossly inefficient, over-engineered monstrousity.

#include "zznet.h"

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

#ifndef ZZ_LINUX
// Hopefully this gets included in C99 so we can remove this hack
FILE *fmemopen(void *buffer, size_t s, const char *mode);
#include "fmemopen.c"
#endif

#define UID_StandardApplicationContext "1.2.840.10008.3.1.1.1"
#define UID_VerificationSOPClass "1.2.840.10008.1.1"

/// Multi-family socket end-point address
union socketfamily
{
    struct sockaddr sa;
    struct sockaddr_in sa_in;
    struct sockaddr_in6 sa_in6;
    struct sockaddr_storage sa_stor;
};

enum psctxs_list
{
	PSCTX_VERIFICATION,
	PSCTX_LAST
};
static const char *psctxs_uids[] = { UID_VerificationSOPClass };
static const char *txsyn_uids[] = { UID_LittleEndianImplicitTransferSyntax, UID_LittleEndianExplicitTransferSyntax, 
                                    UID_DeflatedExplicitVRLittleEndianTransferSyntax, UID_JPEGLSLosslessTransferSyntax };

// // Convenience functions
// -- Debug
//#define DEBUG
#ifdef DEBUG
#define phex(...) printf(__VA_ARGS__)
#else
#define phex(...)
#endif
// -- Writing
static inline void znw1(uint8_t val, struct zzfile *zz) { fputc(val, zz->net.buffer); phex(" %x", val); }
static inline void znw2(uint16_t val, struct zzfile *zz) { uint16_t tmp = htons(val); fwrite(&tmp, 2, 1, zz->net.buffer); phex(" %x %x", val >> 8, val & 0xff); }
static inline void znw4(uint32_t val, struct zzfile *zz) { uint32_t tmp = htonl(val); fwrite(&tmp, 4, 1, zz->net.buffer); phex(" %x %x %x %x", val >> 24, val >> 16, val >> 8, val & 0xff); }
static inline void znwaet(const char *aet, struct zzfile *zz) { char tmp[16]; memset(tmp, 0, sizeof(tmp)); strncpy(tmp, aet, 16); fwrite(tmp, 16, 1, zz->net.buffer); phex(" ..."); }
static inline void znwuid(const char *uid, struct zzfile *zz) { int size = strlen(uid) + 1; znw2(size, zz); fwrite(uid, size, 1, zz->net.buffer); phex(" uid=%d", size); }
static inline void znwpad(int num, struct zzfile *zz) { int i; for (i = 0; i < num; i++) fputc(0, zz->net.buffer); phex(" (pad %d)", num); }
static inline void znw2at(uint16_t val, long pos, struct zzfile *zz) { long curr = ftell(zz->net.buffer); fseek(zz->net.buffer, pos, SEEK_SET); znw2(val, zz); phex("<<%ld", pos); fseek(zz->net.buffer, curr, SEEK_SET); }
static inline void znw4at(uint32_t val, long pos, struct zzfile *zz) { long curr = ftell(zz->net.buffer); fseek(zz->net.buffer, pos, SEEK_SET); znw4(val, zz); phex("<<%ld", pos); fseek(zz->net.buffer, curr, SEEK_SET); }
// --- Reading
static inline void znrskip(int num, struct zzfile *zz) { int i; for (i = 0; i < num; i++) getc(zz->fp); }
static inline uint8_t znr1(struct zzfile *zz) { uint8_t tmp = fgetc(zz->fp); return tmp; }
static inline uint16_t znr2(struct zzfile *zz) { uint16_t tmp; fread(&tmp, 2, 1, zz->fp); tmp = ntohs(tmp); return tmp; }
static inline uint32_t znr4(struct zzfile *zz) { uint32_t tmp; fread(&tmp, 4, 1, zz->fp); tmp = ntohl(tmp); return tmp; }

enum PDU_State
{
	STA_NONE,
	STA_IDLE,
	STA_CONNECTION_OPEN,	// Awaiting A-ASSOCIATE-RQ PDU
	STA_ASSOCIATION_RESPONSE, // Awaiting local A-ASSOCIATE response primitive (from local user)
	STA_TRANSPORT_OPEN, // Awaiting transport connection opening to complete (from local transport service)
	STA_ASSOCIATION_OPEN, // Awaiting A-ASSOCIATE-AC or A-ASSOCIATE-RJ PDU
	STA_READY,	// Association established and ready for data transfer
	STA_RELEASE, 	// Awaiting A-RELEASE-RP PDU
	STA_RELEASE_RESPONSE, // Awaiting local A-RELEASE response primitive (from local user)
	STA_COLLISION_REQ, // Release collision requestor side; awaiting A-RELEASE response (from local user)
	STA_COLLISION_ACC, // Release collision acceptor side; awaiting A-RELEASE-RP PDU
	STA_COLLISION_REQ2, // Release collision requestor side; awaiting A-RELEASE-RP PDU
	STA_COLLISION_ACC2, // Release collision acceptor side; awaiting A-RELEASE response primitive (from local user)
	STA_ASSOCIATION_CLOSED, // Awaiting Transport Connection Close Indication (Association no longer exists)
};

#define BACKLOG 10
#define MAXDATASIZE 100 // max number of bytes we can get at once

struct zzfile *zznetwork(const char *interface, const char *myaetitle, struct zzfile *zz)
{
	const long size = 1024 * 1024; // 1mb
	memset(zz, 0, sizeof(*zz));
	zz->net.mem = malloc(size);
	zz->net.buffer = fmemopen(zz->net.mem, size, "w");
	if (interface)
	{
		strcpy(zz->net.interface, interface);	// TODO use sstrcpy
	}
	strcpy(zz->net.callingaet, myaetitle);
	zz->ladder[0].item = -1;
	zz->current.frame = -1;
	zz->frames = -1;
	zz->pxOffsetTable = -1;
	return zz;
}

static void zzsendbuffer(struct zzfile *zz)
{
	fwrite(zz->net.mem, ftell(zz->net.buffer), 1, zz->fp);
}

static void PData_receive(struct zzfile *zz)
{
	uint32_t msize, psize = 0, received = 0;
	char contextID;

	// PDU type already parsed
	znr1(zz);			// empty
	msize = znr4(zz);		// size of packet

	// Get list of presentation-data-values 
	while (received < msize)
	{
		psize = znr4(zz);	// size of item
		contextID = znr1(zz);	// context id, see PS3.8 7.1.1.13

		// Command data follows
		// HACK
		znrskip(psize - 1, zz);

		received += psize + 4;
	}
	if (received != msize)
	{
		printf("received=%u expected=%u psize=%u tell=%ld\n", received, msize, psize, ftell(zz->fp));
	}
}

static void PDATA_TF_start(struct zzfile *zz, long *pos, char contextID)
{
	rewind(zz->net.buffer);
	znw1(4, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znw4(0, zz);				// size of packet, fill in later
	// One or more Presentation-data-value Item(s) to follow
	*pos = ftell(zz->net.buffer);
	znw4(0, zz);				// size of packet, fill in later
	znw1(contextID, zz);			// context id, see PS3.8 7.1.1.13
	// Add DICOM data with command header after this
}

static void PDATA_TF_next(struct zzfile *zz, long *pos, char contextID)
{
	long curr = ftell(zz->net.buffer);

	// Set size of previous packet
	fseek(zz->net.buffer, *pos, SEEK_SET);
	znw4(ftell(zz->net.buffer) - *pos, zz);
	fseek(zz->net.buffer, curr, SEEK_SET);
	*pos = curr;
	znw4(0, zz);				// size of packet, fill in later
	znw1(contextID, zz);			// context id, see PS3.8 7.1.1.13
	// Add DICOM data with command header after this
}

static void PDATA_TF_end(struct zzfile *zz, long *pos)
{
	// Set size of previous packet
	znw4at(ftell(zz->net.buffer) - *pos, *pos, zz);

	// Set the size of entire pdata message
	znw4at(ftell(zz->net.buffer) - 6, 2, zz);
	zzsendbuffer(zz);
}

static void PDU_ReleaseRQ(struct zzfile *zz)
{
	rewind(zz->net.buffer);
	znw1(5, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znw4(4, zz);				// size of packet
	znw2(0, zz);				// reserved, shall be zero
	zzsendbuffer(zz);
}

static void PDU_ReleaseRP(struct zzfile *zz)
{
	rewind(zz->net.buffer);
	znw1(6, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znw4(4, zz);				// size of packet
	znw2(0, zz);				// reserved, shall be zero
	zzsendbuffer(zz);
}

static void PDU_Abort(struct zzfile *zz, char source, char diag)
{
	rewind(zz->net.buffer);
	znw1(7, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znw4(4, zz);				// size of packet
	znw1(0, zz);				// reserved, shall be zero
	znw1(0, zz);				// reserved, shall be zero
	znw1(source, zz);			// source of abort, see PS 3.8, Table 9-26
	znw1(diag, zz);				// reason for abort, see PS 3.8, Table 9-26
	zzsendbuffer(zz);
}

static void PDU_AssociateRJ(struct zzfile *zz, char result, char source, char diag)
{
	rewind(zz->net.buffer);
	znw1(3, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znw4(4, zz);				// size of packet
	znw2(1, zz);				// version bitfield
	znw1(0, zz);				// reserved, shall be zero
	znw1(result, zz);			// result
	znw1(source, zz);			// source
	znw1(diag, zz);				// reason/diagnosis
	zzsendbuffer(zz);
}

static bool PDU_Associate_Accept(struct zzfile *zz)
{
	uint32_t msize, size;
	char callingaet[17];
	char calledaet[17];
	char pdu;
	long pos;

	memset(calledaet, 0, sizeof(calledaet));

	// Read input
	// PDU type already parsed
	znr1(zz);
	msize = znr4(zz);
	zz->net.version = znr2(zz);
	znrskip(2, zz);
	fread(calledaet, 16, 1, zz->fp);	// TODO verify
	fread(callingaet, 16, 1, zz->fp);
	znrskip(32, zz);
	pdu = znr1(zz);
	znr1(zz);
	size = znr2(zz);
	znrskip(size, zz);				// skip Application Context Item

	// Start writing response
	rewind(zz->net.buffer);
	znw1(0x02, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znw4(0, zz);				// size of packet, fill in later
	znw2(1, zz);				// version bitfield
	znw2(0, zz);				// reserved, shall be zero
	znwaet(calledaet, zz);			// called AE Title
	znwaet(zz->net.callingaet, zz);		// calling AE Title
	znwpad(32, zz);				// reserved, shall be zero

	// Application Context Item (useless fluff)
	znw1(0x10, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znwuid(UID_StandardApplicationContext, zz);	// See PS 3.8, section 7.1.1.2.

	// Read presentation Contexts
	pdu = znr1(zz);
	while (pdu == 0x20)
	{
		uint32_t psize, isize, i;
		char cid;
		char auid[MAX_LEN_UI];
		char tuid[MAX_LEN_UI];
		bool txsyns[ZZ_TX_LAST], foundAcceptableTx = false;

		memset(txsyns, 0, sizeof(txsyns));	// set all to false
		znr1(zz);				// reserved, shall be zero
		psize = znr2(zz);			// size of packet
		cid = znr1(zz);				// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
		znrskip(3, zz);

		// Abstract Syntax (SOP Class) belonging to Presentation Syntax, see PS 3.8 Table 9-14
		pdu = znr1(zz);				// PDU type
		znr1(zz);				// reserved, shall be zero
		isize = znr2(zz);			// size of packet
		memset(auid, 0, sizeof(auid));
		fread(auid, isize, 1, zz->fp);  	// See PS 3.8, section 7.1.1.2.

		pdu = znr1(zz);	// loop
		while (pdu == 0x40)
		{
			// Transfer Syntax(es) belonging to Abstract Syntax
			// (for AC, only one transfer syntax is allowed), see PS 3.8 Table 9-15
			znr1(zz);			// reserved, shall be zero
			isize = znr2(zz);		// size of packet
			memset(tuid, 0, sizeof(tuid));
			fread(tuid, isize, 1, zz->fp);	// See PS 3.8, section 7.1.1.2.
			for (i = 0; i < ZZ_TX_LAST; i++)
			{
				txsyns[i] = ZZ_TX_LAST;
				if (strcmp(txsyn_uids[i], tuid) == 0 && i < 2)
				{
					txsyns[i] = true;
					foundAcceptableTx = true;
				}
			}
			pdu = znr1(zz);	// loop
		}

		// Now generate a response to this presentation context
		znw1(0x21, zz);			// PDU type
		znw1(0, zz);			// reserved, shall be zero
		pos = ftell(zz->net.buffer);
		znw2(0, zz);			// size of packet, fill in later
		znw1(cid, zz);			// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
		znw1(0, zz);			// reserved, shall be zero
		if (!foundAcceptableTx)
		{
			znw1(4, zz);		// 4 == we found no acceptable transfer syntax for this presentation context
		}
		else
		{
			// FIXME, check for not supported abstract syntaxes as well, and respond with 3 if not supported
			znw1(0, zz);		// 0 == accepted presentation context
		}
		znw1(0, zz);			// reserved, shall be zero

		// Chosen transfer Syntax, see PS 3.8 Table 9-15
		znw1(0x40, zz);			// PDU type
		znw1(0, zz);			// reserved, shall be zero
		if (txsyns[ZZ_EXPLICIT])
		{
			znwuid(UID_LittleEndianExplicitTransferSyntax, zz); // See PS 3.8, section 7.1.1.2.
		}
		else
		{
			znwuid(UID_LittleEndianImplicitTransferSyntax, zz);  // See PS 3.8, section 7.1.1.2.
		}

		// Set size of this presentation context item
		znw2at(ftell(zz->net.buffer) - pos - 2, pos, zz);
	}

	// Read User Information (fluff)
	if (pdu != 0x50) printf("wtf=%x\n", pdu);
	znrskip(7, zz);				// skip to interesting part
	zz->net.maxpdatasize = znr4(zz);

	// Write User Information Item Fields, see PS 3.8, Table 9-16
	znw1(0x50, zz);
	znw1(0, zz);				// reserved, shall be zero
	znw2(8, zz);				// size of payload that follows
	znw1(0x51, zz);				// define maximum length of data field sub-item
	znw1(0, zz);				// reserved, shall be zero
	znw2(4, zz);				// size of following field
	znw4(0, zz);				// zero means unlimited size allowed

	// Now set size of entire reply
	znw4at(ftell(zz->net.buffer) - 6, 2, zz);

	zzsendbuffer(zz);
	return true;
}

// FIXME, the UIDs probably need to be padded to even size, like everything else
static void PDU_AssociateRQ(struct zzfile *zz, const char *calledAET, const char *sopclass)
{
	long pos;

	rewind(zz->net.buffer);
	znw1(0x01, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znw4(0, zz);				// size of packet, fill in later
	znw2(1, zz);				// version bitfield
	znw2(0, zz);				// reserved, shall be zero
	znwaet(calledAET, zz);			// called AE Title
	znwaet(zz->net.callingaet, zz);		// calling AE Title
	znwpad(32, zz);				// reserved, shall be zero

	// Application Context Item (useless fluff)
	znw1(0x10, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znwuid(UID_StandardApplicationContext, zz);	// See PS 3.8, section 7.1.1.2.

	// Presentation Context Item(s)
	// note that Abstract Syntax == SOP Class
	znw1(0x20, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	pos = ftell(zz->net.buffer);
	znw2(0, zz);				// size of packet, fill in later
	znw1(1, zz);				// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
	znw1(0, zz);				// reserved, shall be zero
	znw1(0, zz);				// reserved, shall be zero
	znw1(0, zz);				// reserved, shall be zero

	// Abstract Syntax (SOP Class) belonging to Presentation Context, see PS 3.8 Table 9-14
	znw1(0x30, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znwuid(sopclass, zz); 			// See PS 3.8, section 7.1.1.2.

	// Transfer Syntax(es) belonging to Abstract Syntax, see PS 3.8 Table 9-15
	znw1(0x40, zz);				// PDU type
	znw1(0, zz);				// reserved, shall be zero
	znwuid(UID_LittleEndianExplicitTransferSyntax, zz);  // See PS 3.8, section 7.1.1.2.

	// Set size of this presentation context item
	znw2at(ftell(zz->net.buffer) - pos - 4, pos, zz);

	// User Information Item Fields, see PS 3.8, Table 9-16
	znw1(0x50, zz);
	znw1(0, zz);				// reserved, shall be zero
	znw2(8, zz);				// size of packet
	znw1(0x51, zz);				// define maximum length of data field sub-item
	znw1(0, zz);				// reserved, shall be zero
	znw2(4, zz);				// size of following field
	znw4(0, zz);				// zero means unlimited size allowed

	// Now set size of entire message
	znw4at(ftell(zz->net.buffer) - 6, 2, zz);
	zzsendbuffer(zz);
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

		if (!fork())
		{
			// this is the child process
			close(sockfd); // child doesn't need the listener
			zz->fp = fdopen(new_fd, "a+");
			while (true)
			{
				char type = fgetc(zz->fp);
				switch (type)
				{
				case 0x01: printf("Associate RQ received\n"); PDU_Associate_Accept(zz); break;
				case 0x04: printf("PData received\n"); PData_receive(zz); break;
				default: printf("Unknown type: %x\n", type); abort(); break;
				}
			}
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return true;
}

bool zzconnect(struct zzfile *zz, const char *host, int port, const char *theiraetitle, const char *sopclass, int flags)
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char portstr[10];

	(void)flags;
	(void)sopclass;
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

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
	{
		perror("recv");
		return NULL;
	}

	buf[numbytes] = '\0';

	printf("client: received '%s'\n", buf);

	zz->fp = fdopen(sockfd, "a+");
	return true;
}
