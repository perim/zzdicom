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

// // Convenience functions
// -- Writing
static inline void fput2(uint16_t val, FILE *fp) { uint16_t tmp = htons(val); fwrite(&tmp, 2, 1, fp); }
static inline void fput4(uint32_t val, FILE *fp) { uint32_t tmp = htonl(val); fwrite(&tmp, 4, 1, fp); }
static inline void fputaet(const char *aet, FILE *fp) { char tmp[16]; memset(tmp, 0, sizeof(tmp)); strncpy(tmp, aet, 16); fwrite(tmp, 16, 1, fp); }
static inline void fpad(int num, FILE *fp) { int i; for (i = 0; i < num; i++) fputc(0, fp); }
static inline void fput2at(uint16_t val, long pos, FILE *fp) { uint16_t tmp = htons(val); long curr = ftell(fp); fseek(fp, pos, SEEK_SET); fwrite(&tmp, 2, 1, fp); fseek(fp, curr, SEEK_SET); }
static inline void fput4at(uint32_t val, long pos, FILE *fp) { uint32_t tmp = htonl(val); long curr = ftell(fp); fseek(fp, pos, SEEK_SET); fwrite(&tmp, 4, 1, fp); fseek(fp, curr, SEEK_SET); }
// --- Reading
static inline void fskip(int num, FILE *fp) { int i; for (i = 0; i < num; i++) getc(fp); }
static inline uint32_t fget4(FILE *fp) { uint32_t tmp; fread(&tmp, 4, 1, fp); tmp = ntohl(tmp); return tmp; }
static inline uint16_t fget2(FILE *fp) { uint16_t tmp; fread(&tmp, 2, 1, fp); tmp = ntohs(tmp); return tmp; }

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

enum psctxs_list
{
	PSCTX_VERIFICATION,
	PSCTX_LAST
};
static const char *psctxs_uids[] = { UID_VerificationSOPClass };
static const char *txsyn_uids[] = { UID_LittleEndianImplicitTransferSyntax, UID_LittleEndianExplicitTransferSyntax, UID_DeflatedExplicitVRLittleEndianTransferSyntax, 
                                   UID_JPEGLSLosslessTransferSyntax };

struct zznet
{
	FILE *fp;		///< Network socket for data traffic
	FILE *buffer;		///< Memory buffer disguised as a file
	void *mem;		///< Pointer to buffer's memory
	long maxpdatasize;	///< Maximum size of other party's pdata payload
	uint16_t version;	///< Association version capability bitfield
	enum zztxsyn psctxs[PSCTX_LAST];	///< List of negotiated presentation contexts
};

static struct zznet *zznetinit(struct zznet *zn)
{
	const long size = 1024 * 1024; // 1mb
	memset(zn, 0, sizeof(*zn));
	zn->mem = malloc(size);
	zn->buffer = fmemopen(zn->mem, size, "r+");
	return zn;
}

static void zzsendbuffer(struct zznet *zn)
{
	fwrite(zn->mem, ftell(zn->buffer), 1, zn->fp);
}

static void PDATA_TF_start(struct zznet *zn, long *pos, char contextID)
{
	rewind(zn->buffer);
	fputc(4, zn->buffer);			// PDU type
	fputc(0, zn->buffer);			// reserved, shall be zero
	fput4(0, zn->buffer);			// size of packet, fill in later
	// One or more Presentation-data-value Item(s) to follow
	*pos = ftell(zn->buffer);
	fput4(0, zn->buffer);			// size of packet, fill in later
	fputc(contextID, zn->buffer);		// context id, see PS3.8 7.1.1.13
	// Add DICOM data with command header after this
}

static void PDATA_TF_next(struct zznet *zn, long *pos, char contextID)
{
	long curr = ftell(zn->buffer);

	// Set size of previous packet
	fseek(zn->buffer, *pos, SEEK_SET);
	fput4(ftell(zn->buffer) - *pos, zn->buffer);
	fseek(zn->buffer, curr, SEEK_SET);
	*pos = curr;
	fput4(0, zn->buffer);			// size of packet, fill in later
	fputc(contextID, zn->buffer);		// context id, see PS3.8 7.1.1.13
	// Add DICOM data with command header after this
}

static void PDATA_TF_end(struct zznet *zn, long *pos)
{
	// Set size of previous packet
	fput4at(ftell(zn->buffer) - *pos, *pos, zn->buffer);

	// Set the size of entire pdata message
	fput4at(ftell(zn->buffer) - 6, 2, zn->buffer);
	zzsendbuffer(zn);
}

static void PDU_ReleaseRQ(struct zznet *zn)
{
	rewind(zn->buffer);
	fputc(5, zn->buffer);				// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput4(4, zn->buffer);				// size of packet
	fput2(0, zn->buffer);				// reserved, shall be zero
	zzsendbuffer(zn);
}

static void PDU_ReleaseRP(struct zznet *zn)
{
	rewind(zn->buffer);
	fputc(6, zn->buffer);				// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput4(4, zn->buffer);				// size of packet
	fput2(0, zn->buffer);				// reserved, shall be zero
	zzsendbuffer(zn);
}

static void PDU_Abort(struct zznet *zn, char source, char diag)
{
	rewind(zn->buffer);
	fputc(7, zn->buffer);				// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput4(4, zn->buffer);				// size of packet
	fputc(0, zn->buffer);				// reserved, shall be zero
	fputc(0, zn->buffer);				// reserved, shall be zero
	fputc(source, zn->buffer);			// source of abort, see PS 3.8, Table 9-26
	fputc(diag, zn->buffer);			// reason for abort, see PS 3.8, Table 9-26
	zzsendbuffer(zn);
}

static void PDU_AssociateRJ(struct zznet *zn, char result, char source, char diag)
{
	rewind(zn->buffer);
	fputc(3, zn->buffer);				// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput4(4, zn->buffer);				// size of packet
	fput2(1, zn->buffer);				// version bitfield
	fputc(0, zn->buffer);				// reserved, shall be zero
	fputc(result, zn->buffer);			// result
	fputc(source, zn->buffer);			// source
	fputc(diag, zn->buffer);			// reason/diagnosis
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
	fgetc(zn->fp);
	msize = fget4(zn->fp);
	zn->version = fget2(zn->fp);
	fskip(2, zn->fp);
	fread(calledaet, 16, 1, zn->fp);
	fread(callingaet, 16, 1, zn->fp);
printf("called AET: %s\n", calledaet);
printf("calling AET: %s\n", callingaet);
	fskip(32, zn->fp);
	pdu = fgetc(zn->fp);
	fgetc(zn->fp);
	size = fget2(zn->fp);
	fskip(size, zn->fp);				// skip Application Context Item

	// Start writing response
	rewind(zn->buffer);
	fputc(0x02, zn->buffer);			// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput4(0, zn->buffer);				// size of packet, fill in later
	fput2(1, zn->buffer);				// version bitfield
	fput2(0, zn->buffer);				// reserved, shall be zero
	fputaet(calledaet, zn->buffer);			// called AE Title
	fputaet(callingaet, zn->buffer);		// calling AE Title
	fpad(32, zn->buffer);				// reserved, shall be zero

	// Application Context Item (useless fluff)
	fputc(0x10, zn->buffer);			// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	size = sizeof(UID_StandardApplicationContext);
	fput2(size, zn->buffer);			// size of packet
	fwrite(UID_StandardApplicationContext, size, 1, zn->buffer);	// See PS 3.8, section 7.1.1.2.

	// Read presentation Contexts
	pdu = fgetc(zn->fp);
	while (pdu == 0x20)
	{
		uint32_t psize, isize, i;
		char cid;
		char auid[MAX_LEN_UI];
		char tuid[MAX_LEN_UI];
		bool txsyns[ZZ_TX_LAST], foundAcceptableTx = false;

		memset(txsyns, 0, sizeof(txsyns));	// set all to false
		fgetc(zn->fp);				// reserved, shall be zero
		psize = fget2(zn->fp);			// size of packet
		cid = fgetc(zn->fp);			// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
printf("pres id = %u\n", cid);
		fskip(3, zn->fp);

		// Abstract Syntax (SOP Class) belonging to Presentation Syntax, see PS 3.8 Table 9-14
		pdu = fgetc(zn->fp);			// PDU type
		if (pdu != 0x30) printf("weirdo pdu\n");
		fgetc(zn->fp);				// reserved, shall be zero
		isize = fget2(zn->fp);			// size of packet
		memset(auid, 0, sizeof(auid));
		fread(auid, isize, 1, zn->fp);  	// See PS 3.8, section 7.1.1.2.
printf("auid=%s\n", auid);

		pdu = fgetc(zn->fp);	// loop
		while (pdu == 0x40)
		{
			// Transfer Syntax(es) belonging to Abstract Syntax
			// (for AC, only one transfer syntax is allowed), see PS 3.8 Table 9-15
			fgetc(zn->fp);			// reserved, shall be zero
			isize = fget2(zn->fp);		// size of packet
			memset(tuid, 0, sizeof(tuid));
			fread(tuid, isize, 1, zn->fp);	// See PS 3.8, section 7.1.1.2.
printf("tuid=%s\n", tuid);
			for (i = 0; i < ZZ_TX_LAST; i++)
			{
				txsyns[i] = ZZ_TX_LAST;
				if (strcmp(txsyn_uids[i], tuid) == 0)
				{
					txsyns[i] = true;
					foundAcceptableTx = true;
printf("tx[%s] for tuid=%s is TRUE\n", txsyn_uids[i], tuid);
				}
			}
			pdu = fgetc(zn->fp);	// loop
		}

		// Now generate a response to this presentation context
		fputc(0x21, zn->buffer);		// PDU type
		fputc(0, zn->buffer);			// reserved, shall be zero
		pos = ftell(zn->buffer);
		fput2(0, zn->buffer);			// size of packet, fill in later
		fputc(cid, zn->buffer);			// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
		fputc(0, zn->buffer);			// reserved, shall be zero
		if (!foundAcceptableTx)
		{
			fputc(4, zn->buffer);		// 4 == we found no acceptable transfer syntax for this presentation context
		}
		else
		{
			// FIXME, check for not supported abstract syntaxes as well, and respond with 3 if not supported
			fputc(0, zn->buffer);		// 0 == accepted presentation context
		}
		fputc(0, zn->buffer);			// reserved, shall be zero

		// Abstract Syntax (SOP Class) belonging to Presentation Syntax, see PS 3.8 Table 9-14
		fputc(0x30, zn->buffer);		// PDU type
		fputc(0, zn->buffer);			// reserved, shall be zero
		fput2(strlen(auid), zn->buffer);	// size of packet
		fwrite(auid, strlen(auid), 1, zn->buffer);  // See PS 3.8, section 7.1.1.2.

		// Chosen transfer Syntax for Abstract Syntax, see PS 3.8 Table 9-15
		fputc(0x40, zn->buffer);		// PDU type
		fputc(0, zn->buffer);			// reserved, shall be zero
		if (txsyns[ZZ_EXPLICIT])
		{
			size = sizeof(UID_LittleEndianExplicitTransferSyntax);
			fput2(size, zn->buffer);	// size of packet
			fwrite(UID_LittleEndianExplicitTransferSyntax, size, 1, zn->buffer);  // See PS 3.8, section 7.1.1.2.
		}
		else if (txsyns[ZZ_IMPLICIT])
		{
			size = sizeof(UID_LittleEndianImplicitTransferSyntax);
			fput2(size, zn->buffer);	// size of packet
			fwrite(UID_LittleEndianImplicitTransferSyntax, size, 1, zn->buffer);  // See PS 3.8, section 7.1.1.2.
		}

		// Set size of this presentation context item
		fput2at(ftell(zn->buffer) - pos - 2, pos, zn->buffer);
	}

	// Read User Information (fluff)
	if (pdu != 0x50) printf("wtf=%x\n", pdu);
	fskip(7, zn->fp);				// skip to interesting part
	zn->maxpdatasize = fget4(zn->fp);

	// Write User Information Item Fields, see PS 3.8, Table 9-16
	fputc(0x50, zn->buffer);
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput2(8, zn->buffer);
	fputc(0x51, zn->buffer);			// define maximum length of data field sub-item
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput2(4, zn->buffer);				// size of following field
	fput4(0, zn->buffer);				// zero means unlimited size allowed

	// Now set size of entire reply
	fput4at(ftell(zn->buffer) - 6, 2, zn->buffer);

	zzsendbuffer(zn);
	return true;
}

// FIXME, the UIDs probably need to be padded to even size, like everything else
static void PDU_AssociateRQ(struct zznet *zn, const char *calledAET, const char *callingAET, const char *sopclass)
{
	long pos, size;

	rewind(zn->buffer);
	fputc(0x01, zn->buffer);			// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput4(0, zn->buffer);				// size of packet, fill in later
	fput2(1, zn->buffer);				// version bitfield
	fput2(0, zn->buffer);				// reserved, shall be zero
	fputaet(calledAET, zn->buffer);			// called AE Title
	fputaet(callingAET, zn->buffer);		// calling AE Title
	fpad(32, zn->buffer);				// reserved, shall be zero

	// Application Context Item (useless fluff)
	fputc(0x10, zn->buffer);			// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	size = sizeof(UID_StandardApplicationContext);
	fput2(size, zn->buffer);			// size of packet
	fwrite(UID_StandardApplicationContext, size, 1, zn->buffer);	// See PS 3.8, section 7.1.1.2.

	// Presentation Context Item(s)
	// note that Abstract Syntax == SOP Class
	fputc(0x20, zn->buffer);			// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	pos = ftell(zn->buffer);
	fput2(0, zn->buffer);				// size of packet, fill in later
	fputc(1, zn->buffer);				// presentation context ID; must be odd; see PS 3.8 section 7.1.1.13
	fputc(0, zn->buffer);				// reserved, shall be zero
	fputc(0, zn->buffer);				// reserved, shall be zero
	fputc(0, zn->buffer);				// reserved, shall be zero

	// Abstract Syntax (SOP Class) belonging to Presentation Syntax, see PS 3.8 Table 9-14
	fputc(0x30, zn->buffer);			// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput2(strlen(sopclass), zn->buffer);		// size of packet
	fwrite(sopclass, strlen(sopclass), 1, zn->buffer);  // See PS 3.8, section 7.1.1.2.

	// Transfer Syntax(es) belonging to Abstract Syntax (for AC, only one transfer syntax is allowed), see PS 3.8 Table 9-15
	fputc(0x40, zn->buffer);			// PDU type
	fputc(0, zn->buffer);				// reserved, shall be zero
	size = sizeof(UID_LittleEndianExplicitTransferSyntax);
	fput2(size, zn->buffer);			// size of packet
	fwrite(UID_LittleEndianExplicitTransferSyntax, size, 1, zn->buffer);  // See PS 3.8, section 7.1.1.2.

	// Set size of this presentation context item
	fput2at(ftell(zn->buffer) - pos - 4, pos, zn->buffer);

	// User Information Item Fields, see PS 3.8, Table 9-16
	fputc(0x50, zn->buffer);
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput2(8, zn->buffer);				// size of packet
	fputc(0x51, zn->buffer);			// define maximum length of data field sub-item
	fputc(0, zn->buffer);				// reserved, shall be zero
	fput2(4, zn->buffer);				// size of following field
	fput4(0, zn->buffer);				// zero means unlimited size allowed

	// Now set size of entire message
	fput4at(ftell(zn->buffer) - 6, 2, zn->buffer);
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