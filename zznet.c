// Support for the DICOM network protocol. Apologies for the code that follows, it is hard to write elegant code to support
// such a hideous, bloated and grossly inefficient, over-engineered monstrousity.

#if defined(__linux__) || defined(__linux)
#define ZZ_LINUX
#endif

#include "zz_priv.h"

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

enum psctxs_list
{
	PSCTX_VERIFICATION,
	PSCTX_LAST
};
static const char *psctxs_uids[] = { UID_VerificationSOPClass };
static const char *txsyn_uids[] = { UID_LittleEndianImplicitTransferSyntax, UID_LittleEndianExplicitTransferSyntax, 
                                    UID_DeflatedExplicitVRLittleEndianTransferSyntax, UID_JPEGLSLosslessTransferSyntax };

struct zznet
{
	FILE *fp;		///< Network socket for data traffic
	FILE *buffer;		///< Memory buffer disguised as a file
	void *mem;		///< Pointer to buffer's memory
	long maxpdatasize;	///< Maximum size of other party's pdata payload
	uint16_t version;	///< Association version capability bitfield
	enum zztxsyn psctxs[PSCTX_LAST];	///< List of negotiated presentation contexts
};

// // Convenience functions
// -- Debug
//#define DEBUG
#ifdef DEBUG
#define phex(...) printf(__VA_ARGS__)
#else
#define phex(...)
#endif
// -- Writing
static inline void znw1(uint8_t val, struct zznet *zn) { fputc(val, zn->buffer); phex(" %x", val); }
static inline void znw2(uint16_t val, struct zznet *zn) { uint16_t tmp = htons(val); fwrite(&tmp, 2, 1, zn->buffer); phex(" %x %x", val >> 8, val & 0xff); }
static inline void znw4(uint32_t val, struct zznet *zn) { uint32_t tmp = htonl(val); fwrite(&tmp, 4, 1, zn->buffer); phex(" %x %x %x %x", val >> 24, val >> 16, val >> 8, val & 0xff); }
static inline void znwaet(const char *aet, struct zznet *zn) { char tmp[16]; memset(tmp, 0, sizeof(tmp)); strncpy(tmp, aet, 16); fwrite(tmp, 16, 1, zn->buffer); phex(" ..."); }
static inline void znwuid(const char *uid, struct zznet *zn) { int size = strlen(uid) + 1; znw2(size, zn); fwrite(uid, size, 1, zn->buffer); phex(" uid=%d", size); }
static inline void znwpad(int num, struct zznet *zn) { int i; for (i = 0; i < num; i++) fputc(0, zn->buffer); phex(" (pad %d)", num); }
static inline void znw2at(uint16_t val, long pos, struct zznet *zn) { long curr = ftell(zn->buffer); fseek(zn->buffer, pos, SEEK_SET); znw2(val, zn); phex("<<%ld", pos); fseek(zn->buffer, curr, SEEK_SET); }
static inline void znw4at(uint32_t val, long pos, struct zznet *zn) { long curr = ftell(zn->buffer); fseek(zn->buffer, pos, SEEK_SET); znw4(val, zn); phex("<<%ld", pos); fseek(zn->buffer, curr, SEEK_SET); }
// --- Reading
static inline void znrskip(int num, struct zznet *zn) { int i; for (i = 0; i < num; i++) getc(zn->fp); }
static inline uint8_t znr1(struct zznet *zn) { uint8_t tmp = fgetc(zn->fp); return tmp; }
static inline uint16_t znr2(struct zznet *zn) { uint16_t tmp; fread(&tmp, 2, 1, zn->fp); tmp = ntohs(tmp); return tmp; }
static inline uint32_t znr4(struct zznet *zn) { uint32_t tmp; fread(&tmp, 4, 1, zn->fp); tmp = ntohl(tmp); return tmp; }

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

static struct zznet *zznetinit(struct zznet *zn)
{
	const long size = 1024 * 1024; // 1mb
	memset(zn, 0, sizeof(*zn));
	zn->mem = malloc(size);
	zn->buffer = fmemopen(zn->mem, size, "w");
	return zn;
}

static void zzsendbuffer(struct zznet *zn)
{
	fwrite(zn->mem, ftell(zn->buffer), 1, zn->fp);
}

static void PDATA_TF_start(struct zznet *zn, long *pos, char contextID)
{
	rewind(zn->buffer);
	znw1(4, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znw4(0, zn);				// size of packet, fill in later
	// One or more Presentation-data-value Item(s) to follow
	*pos = ftell(zn->buffer);
	znw4(0, zn);				// size of packet, fill in later
	znw1(contextID, zn);			// context id, see PS3.8 7.1.1.13
	// Add DICOM data with command header after this
}

static void PDATA_TF_next(struct zznet *zn, long *pos, char contextID)
{
	long curr = ftell(zn->buffer);

	// Set size of previous packet
	fseek(zn->buffer, *pos, SEEK_SET);
	znw4(ftell(zn->buffer) - *pos, zn);
	fseek(zn->buffer, curr, SEEK_SET);
	*pos = curr;
	znw4(0, zn);				// size of packet, fill in later
	znw1(contextID, zn);			// context id, see PS3.8 7.1.1.13
	// Add DICOM data with command header after this
}

static void PDATA_TF_end(struct zznet *zn, long *pos)
{
	// Set size of previous packet
	znw4at(ftell(zn->buffer) - *pos, *pos, zn);

	// Set the size of entire pdata message
	znw4at(ftell(zn->buffer) - 6, 2, zn);
	zzsendbuffer(zn);
}

static void PDU_ReleaseRQ(struct zznet *zn)
{
	rewind(zn->buffer);
	znw1(5, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znw4(4, zn);				// size of packet
	znw2(0, zn);				// reserved, shall be zero
	zzsendbuffer(zn);
}

static void PDU_ReleaseRP(struct zznet *zn)
{
	rewind(zn->buffer);
	znw1(6, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znw4(4, zn);				// size of packet
	znw2(0, zn);				// reserved, shall be zero
	zzsendbuffer(zn);
}

static void PDU_Abort(struct zznet *zn, char source, char diag)
{
	rewind(zn->buffer);
	znw1(7, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znw4(4, zn);				// size of packet
	znw1(0, zn);				// reserved, shall be zero
	znw1(0, zn);				// reserved, shall be zero
	znw1(source, zn);			// source of abort, see PS 3.8, Table 9-26
	znw1(diag, zn);				// reason for abort, see PS 3.8, Table 9-26
	zzsendbuffer(zn);
}

static void PDU_AssociateRJ(struct zznet *zn, char result, char source, char diag)
{
	rewind(zn->buffer);
	znw1(3, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znw4(4, zn);				// size of packet
	znw2(1, zn);				// version bitfield
	znw1(0, zn);				// reserved, shall be zero
	znw1(result, zn);			// result
	znw1(source, zn);			// source
	znw1(diag, zn);				// reason/diagnosis
	zzsendbuffer(zn);
}

static bool PDU_Associate_Accept(struct zznet *zn)
{
	uint32_t msize, size;
	char callingaet[17];
	char calledaet[17];
	char pdu;
	long pos;

	memset(callingaet, 0, sizeof(callingaet));
	memset(calledaet, 0, sizeof(calledaet));

	// Read input
	// PDU type already parsed
	znr1(zn);
	msize = znr4(zn);
	zn->version = znr2(zn);
	znrskip(2, zn);
	fread(calledaet, 16, 1, zn->fp);
	fread(callingaet, 16, 1, zn->fp);
	znrskip(32, zn);
	pdu = znr1(zn);
	znr1(zn);
	size = znr2(zn);
	znrskip(size, zn);				// skip Application Context Item

	// Start writing response
	rewind(zn->buffer);
	znw1(0x02, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znw4(0, zn);				// size of packet, fill in later
	znw2(1, zn);				// version bitfield
	znw2(0, zn);				// reserved, shall be zero
	znwaet(calledaet, zn);			// called AE Title
	znwaet(callingaet, zn);			// calling AE Title
	znwpad(32, zn);				// reserved, shall be zero

	// Application Context Item (useless fluff)
	znw1(0x10, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znwuid(UID_StandardApplicationContext, zn);	// See PS 3.8, section 7.1.1.2.

	// Read presentation Contexts
	pdu = znr1(zn);
	while (pdu == 0x20)
	{
		uint32_t psize, isize, i;
		char cid;
		char auid[MAX_LEN_UI];
		char tuid[MAX_LEN_UI];
		bool txsyns[ZZ_TX_LAST], foundAcceptableTx = false;

		memset(txsyns, 0, sizeof(txsyns));	// set all to false
		znr1(zn);				// reserved, shall be zero
		psize = znr2(zn);			// size of packet
		cid = znr1(zn);			// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
		znrskip(3, zn);

		// Abstract Syntax (SOP Class) belonging to Presentation Syntax, see PS 3.8 Table 9-14
		pdu = znr1(zn);			// PDU type
		if (pdu != 0x30) printf("weirdo pdu\n");
		znr1(zn);				// reserved, shall be zero
		isize = znr2(zn);			// size of packet
		memset(auid, 0, sizeof(auid));
		fread(auid, isize, 1, zn->fp);  	// See PS 3.8, section 7.1.1.2.

		pdu = znr1(zn);	// loop
		while (pdu == 0x40)
		{
			// Transfer Syntax(es) belonging to Abstract Syntax
			// (for AC, only one transfer syntax is allowed), see PS 3.8 Table 9-15
			znr1(zn);			// reserved, shall be zero
			isize = znr2(zn);		// size of packet
			memset(tuid, 0, sizeof(tuid));
			fread(tuid, isize, 1, zn->fp);	// See PS 3.8, section 7.1.1.2.
			for (i = 0; i < ZZ_TX_LAST; i++)
			{
				txsyns[i] = ZZ_TX_LAST;
				if (strcmp(txsyn_uids[i], tuid) == 0 && i < 2)
				{
					txsyns[i] = true;
					foundAcceptableTx = true;
				}
			}
			pdu = znr1(zn);	// loop
		}

		// Now generate a response to this presentation context
		znw1(0x21, zn);		// PDU type
		znw1(0, zn);			// reserved, shall be zero
		pos = ftell(zn->buffer);
		znw2(0, zn);			// size of packet, fill in later
		znw1(cid, zn);			// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
		znw1(0, zn);			// reserved, shall be zero
		if (!foundAcceptableTx)
		{
			znw1(4, zn);		// 4 == we found no acceptable transfer syntax for this presentation context
		}
		else
		{
			// FIXME, check for not supported abstract syntaxes as well, and respond with 3 if not supported
			znw1(0, zn);		// 0 == accepted presentation context
		}
		znw1(0, zn);			// reserved, shall be zero

		// Chosen transfer Syntax, see PS 3.8 Table 9-15
		znw1(0x40, zn);			// PDU type
		znw1(0, zn);			// reserved, shall be zero
		if (txsyns[ZZ_EXPLICIT])
		{
			znwuid(UID_LittleEndianExplicitTransferSyntax, zn); // See PS 3.8, section 7.1.1.2.
		}
		else
		{
			znwuid(UID_LittleEndianImplicitTransferSyntax, zn);  // See PS 3.8, section 7.1.1.2.
		}

		// Set size of this presentation context item
		znw2at(ftell(zn->buffer) - pos - 2, pos, zn);
	}

	// Read User Information (fluff)
	if (pdu != 0x50) printf("wtf=%x\n", pdu);
	znrskip(7, zn);				// skip to interesting part
	zn->maxpdatasize = znr4(zn);

	// Write User Information Item Fields, see PS 3.8, Table 9-16
	znw1(0x50, zn);
	znw1(0, zn);				// reserved, shall be zero
	znw2(8, zn);				// size of payload that follows
	znw1(0x51, zn);				// define maximum length of data field sub-item
	znw1(0, zn);				// reserved, shall be zero
	znw2(4, zn);				// size of following field
	znw4(0, zn);				// zero means unlimited size allowed

	// Now set size of entire reply
	znw4at(ftell(zn->buffer) - 6, 2, zn);

	zzsendbuffer(zn);
	return true;
}

// FIXME, the UIDs probably need to be padded to even size, like everything else
static void PDU_AssociateRQ(struct zznet *zn, const char *calledAET, const char *callingAET, const char *sopclass)
{
	long pos;

	rewind(zn->buffer);
	znw1(0x01, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znw4(0, zn);				// size of packet, fill in later
	znw2(1, zn);				// version bitfield
	znw2(0, zn);				// reserved, shall be zero
	znwaet(calledAET, zn);			// called AE Title
	znwaet(callingAET, zn);			// calling AE Title
	znwpad(32, zn);				// reserved, shall be zero

	// Application Context Item (useless fluff)
	znw1(0x10, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znwuid(UID_StandardApplicationContext, zn);	// See PS 3.8, section 7.1.1.2.

	// Presentation Context Item(s)
	// note that Abstract Syntax == SOP Class
	znw1(0x20, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	pos = ftell(zn->buffer);
	znw2(0, zn);				// size of packet, fill in later
	znw1(1, zn);				// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
	znw1(0, zn);				// reserved, shall be zero
	znw1(0, zn);				// reserved, shall be zero
	znw1(0, zn);				// reserved, shall be zero

	// Abstract Syntax (SOP Class) belonging to Presentation Syntax, see PS 3.8 Table 9-14
	znw1(0x30, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znwuid(sopclass, zn); 			// See PS 3.8, section 7.1.1.2.

	// Transfer Syntax(es) belonging to Abstract Syntax (for AC, only one transfer syntax is allowed), see PS 3.8 Table 9-15
	znw1(0x40, zn);				// PDU type
	znw1(0, zn);				// reserved, shall be zero
	znwuid(UID_LittleEndianExplicitTransferSyntax, zn);  // See PS 3.8, section 7.1.1.2.

	// Set size of this presentation context item
	znw2at(ftell(zn->buffer) - pos - 4, pos, zn);

	// User Information Item Fields, see PS 3.8, Table 9-16
	znw1(0x50, zn);
	znw1(0, zn);				// reserved, shall be zero
	znw2(8, zn);				// size of packet
	znw1(0x51, zn);				// define maximum length of data field sub-item
	znw1(0, zn);				// reserved, shall be zero
	znw2(4, zn);				// size of following field
	znw4(0, zn);				// zero means unlimited size allowed

	// Now set size of entire message
	znw4at(ftell(zn->buffer) - 6, 2, zn);
	zzsendbuffer(zn);
}

/// Reap dead child processes
static void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

/// get sockaddr, IPv4 or IPv6
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

bool zznetlisten(int port)
{
	struct zznet czn, *zn;
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	char portstr[10];

	zn = zznetinit(&czn);
	memset(&hints, 0, sizeof hints);
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
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1)
		{
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork())
		{
			// this is the child process
			close(sockfd); // child doesn't need the listener
			zn->fp = fdopen(new_fd, "a+");
			while (true)
			{
				char type = fgetc(zn->fp);
				switch (type)
				{
				case 0x01: printf("Associate RQ received\n"); PDU_Associate_Accept(zn); break;
				case 0x04: printf("PData received\n"); abort(); break;
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

struct zznet *zznetconnect(struct zznet *zn, const char *host, int port)
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	char portstr[10];

	if (!zn) return NULL;
	zn = zznetinit(zn);

	memset(&hints, 0, sizeof hints);
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

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0)) == -1)
	{
		perror("recv");
		return NULL;
	}

	buf[numbytes] = '\0';

	printf("client: received '%s'\n", buf);

	zn->fp = fdopen(sockfd, "a+");
	return zn;
}

void zznetloop(struct zznet *zn)
{
}

struct zznet *zznetclose(struct zznet *zn)
{
	if (zn)
	{
		fclose(zn->fp);
		fclose(zn->buffer);
		free(zn->mem);
	}
	return NULL;
}

int main(int argc, char **argv)
{
	zznetlisten(5104);
	return 0;
}
