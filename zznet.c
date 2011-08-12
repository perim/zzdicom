// Support for the DICOM network protocol. Apologies for the code that follows, it is hard to write elegant code to support
// such a hideous, bloated and grossly inefficient, over-engineered monstrousity.

#include "zznet.h"
#include "zzio.h"
#include "zzwrite.h"

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
#define DEBUGPRINT
#ifdef DEBUGPRINT
#define phex(...) printf(__VA_ARGS__)
#else
#define phex(...)
#endif
// -- Writing
static inline void znw1(uint8_t val, struct zzfile *zz) { ziputc(zz->zi, val); phex(" %x", val); }
static inline void znw2(uint16_t val, struct zzfile *zz) { uint16_t tmp = htons(val); ziwrite(zz->zi, &tmp, 2); phex(" %x %x", val >> 8, val & 0xff); }
static inline void znw4(uint32_t val, struct zzfile *zz) { uint32_t tmp = htonl(val); ziwrite(zz->zi, &tmp, 4); phex(" %x %x %x %x", val >> 24, val >> 16, val >> 8, val & 0xff); }
static inline void znwaet(const char *aet, struct zzfile *zz) { char tmp[16]; memset(tmp, 0, sizeof(tmp)); strncpy(tmp, aet, 16); ziwrite(zz->zi, tmp, 16); phex(" ..."); }
static inline void znwuid(const char *uid, struct zzfile *zz) { int size = strlen(uid) + 1; znw2(size, zz); ziwrite(zz->zi, uid, size); phex(" uid=%d", size); }
static inline void znwpad(int num, struct zzfile *zz) { int i; for (i = 0; i < num; i++) ziputc(zz->zi, 0); phex(" (pad %d)", num); }
static inline void znw2at(uint16_t val, long pos, struct zzfile *zz) { ziwriteu16at(zz->zi, htons(val), pos); phex("<<%ld@%ld", (long)val, pos); }
static inline void znw4at(uint32_t val, long pos, struct zzfile *zz) { ziwriteu16at(zz->zi, htons(val), pos); phex("<<%ld@%ld", (long)val, pos); }
static inline void znwstart(struct zzfile *zz, int pdutype) { zisetwritepos(zz->zi, 0); zz->net.pdutype = pdutype; }
static inline void znwsendbuffer(struct zzfile *zz) { ziflush(zz->zi); }
// --- Reading
static inline void znrskip(int num, struct zzfile *zz) { int i; for (i = 0; i < num; i++) zigetc(zz->zi); }
static inline uint8_t znr1(struct zzfile *zz) { uint8_t tmp = zigetc(zz->zi); return tmp; }
static inline uint16_t znr2(struct zzfile *zz) { uint16_t tmp; ziread(zz->zi, &tmp, 2); tmp = ntohs(tmp); return tmp; }
static inline uint32_t znr4(struct zzfile *zz) { uint32_t tmp; ziread(zz->zi, &tmp, 4); tmp = ntohl(tmp); return tmp; }

#define BACKLOG 10
#define MAXDATASIZE 100 // max number of bytes we can get at once

static long headerwrite(long bytes, char *buffer, const void *data)
{
	struct zzfile *zz = (struct zzfile *)data;
	uint32_t *size1 = (uint32_t *)(buffer + 2);
	uint32_t *size2 = (uint32_t *)(buffer + 6);

	buffer[0] = zz->net.pdutype;	// PDU type
	buffer[1] = 0x00;		// reserved
	*size1 = htonl(bytes + 5);	// put size of data + PDV header size
	if (zz->net.pdutype == 0x04)	// P-DATA needs PDV fragmenting for some inexplicable reason
	{
		// We do the only sane thing, however, and keep only one PDV for each PDU
		*size2 = htonl(bytes);		// put size of PDV
		buffer[10] = zz->net.psctx;	// current presentation context
		buffer[11] = 0x00;		// FIXME -- message control header!
		return 12;
	}
	return 6;
}

static long readbuffer(char **bufptr, long *buflen, void *data)
{
	struct zzfile *zz = (struct zzfile *)data;
	int fd = zifd(zz->zi);
	char buf[6];
	long result = read(fd, buf, 6);	// FIXME -- loop
	uint32_t *size1 = (uint32_t *)(buf + 2);

	assert(result == 6);
	zz->net.pdutype = buf[0];		// PDU type
	zz->net.pdusize = ntohl(*size1);	// PDU size
	if (!*bufptr || zz->net.pdusize > *buflen)	// FIXME -- use realloc?
	{
		free(*bufptr);
		*bufptr = malloc(zz->net.pdusize);
		*buflen = zz->net.pdusize;
	}
	if (zz->net.pdutype == 0x04)
	{
		// Stitch together PDVs, then pretend they never were there, which makes the network part of the
		// DICOM standard go from Cthulhu level insane to just stupid. Also look for the oddball eof bit.
		long count = 0;
		while (count < zz->net.pdusize)
		{
			char *modptr = *bufptr + count;

			result = read(fd, buf, 6);
			assert(result == 6);
			uint32_t *size2 = (uint32_t *)(buf + 1);	// PDV size
			uint8_t msgctrlhdr = buf[5];			// message control header
			result = read(fd, modptr, *size2);
			assert(result == *size2);
			count += result;
			if (msgctrlhdr & 0x02) // if second bit is set, this is the last PDV
			{
				ziseteof(zz->zi);	// end of data stream after this buffer
			}
		}
	}
	else
	{
		result = read(fd, *bufptr, zz->net.pdusize);
		assert(result == zz->net.pdusize);
	}
	return zz->net.pdusize;
}

struct zzfile *zznetwork(const char *interface, const char *myaetitle, struct zzfile *zz)
{
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

/// Special built-in support for echo requests
static void C_ECHO_req_recv(struct zzfile *zz)
{
	uint16_t group, element, mesID;
	long len;

	// Read ping
	while (zzread(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_MessageID: mesID = zzgetuint16(zz, 0); break;
		default: break;	// ignore
		}
	}
}

static void PData_receive(struct zzfile *zz)
{
	printf("PDATA receipt - not implemented yet\n");
}

void znwechoreq(struct zzfile *zz)
{
	// first presentation context is always verification, and is always accepted, no need to check
	znwstart(zz, 0x04);
	zzwUL(zz, DCM_CommandGroupLength, 0);		// value to be written later
	zzwUI(zz, DCM_AffectedSOPClassUID, UID_VerificationSOPClass);
	zzwUS(zz, DCM_CommandField, 0x0030);
	zzwUS(zz, DCM_MessageID, zz->net.lastMesID++);
	zzwUS(zz, DCM_DataSetType, 0x0101);		// "null value"
	znw4at(ziwritepos(zz->zi) - 8, 8, zz);		// set command length
	znwsendbuffer(zz);
}

void znwechoresp(struct zzfile *zz, long mesID)
{
	// first presentation context is always verification, and is always accepted, no need to check
	znwstart(zz, 0x04);
	zzwUL(zz, DCM_CommandGroupLength, 0);		// value to be written later
	zzwUI(zz, DCM_AffectedSOPClassUID, UID_VerificationSOPClass);
	zzwUS(zz, DCM_CommandField, 0x8030);
	zzwUS(zz, DCM_MessageIDBeingRespondedTo, mesID);
	zzwUS(zz, DCM_DataSetType, 0x0101);		// "null value"
	zzwUS(zz, DCM_Status, 0x0000);			// "Success", always
	znw4at(ziwritepos(zz->zi) - 8, 8, zz);	// set command length
	znwsendbuffer(zz);
}

static void PDU_ReleaseRQ(struct zzfile *zz)
{
	znwstart(zz, 0x05);
	znw4(0, zz);				// reserved, shall be zero
	znwsendbuffer(zz);
}

static void PDU_ReleaseRP(struct zzfile *zz)
{
	znwstart(zz, 0x06);
	znw4(0, zz);				// reserved, shall be zero
	znwsendbuffer(zz);
}

static void PDU_Abort(struct zzfile *zz, char source, char diag)
{
	znwstart(zz, 0x07);
	znw1(0, zz);				// reserved, shall be zero
	znw1(0, zz);				// reserved, shall be zero
	znw1(source, zz);			// source of abort, see PS 3.8, Table 9-26
	znw1(diag, zz);				// reason for abort, see PS 3.8, Table 9-26
	znwsendbuffer(zz);
}

static void PDU_AssociateRJ(struct zzfile *zz, char result, char source, char diag)
{
	znwstart(zz, 0x03);
	znw2(1, zz);				// version bitfield
	znw1(0, zz);				// reserved, shall be zero
	znw1(result, zz);			// result
	znw1(source, zz);			// source
	znw1(diag, zz);				// reason/diagnosis
	znwsendbuffer(zz);
}

static bool PDU_Associate_Accept(struct zzfile *zz)
{
	uint32_t msize, size;
	char callingaet[17];
	char calledaet[17];
	char pdu;
	long pos;

	memset(calledaet, 0, sizeof(calledaet));
	memset(callingaet, 0, sizeof(callingaet));

	zz->net.version = znr2(zz);
	znr2(zz);
	ziread(zz->zi, calledaet, 16);		// TODO verify
	ziread(zz->zi, callingaet, 16);
	znrskip(32, zz);
	pdu = znr1(zz);
	assert(pdu == 0x10);
	znr1(zz);
	size = znr2(zz);
	assert(size < zz->net.pdusize);
	znrskip(size, zz);				// skip Application Context Item

	// Start writing response
	znwstart(zz, 0x02);
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
		ziread(zz->zi, auid, isize);	  	// See PS 3.8, section 7.1.1.2.

		pdu = znr1(zz);	// loop
		while (pdu == 0x40)
		{
			// Transfer Syntax(es) belonging to Abstract Syntax
			// (for AC, only one transfer syntax is allowed), see PS 3.8 Table 9-15
			znr1(zz);			// reserved, shall be zero
			isize = znr2(zz);		// size of packet
			memset(tuid, 0, sizeof(tuid));
			ziread(zz->zi, tuid, isize);	// See PS 3.8, section 7.1.1.2.
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
		pos = ziwritepos(zz->zi);
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
		znw2at(ziwritepos(zz->zi) - pos - 2, pos, zz);
	}

	// Read User Information (fluff)
	assert(pdu == 0x50);
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

	znwsendbuffer(zz);
	return true;
}

// FIXME, the UIDs probably need to be padded to even size, like everything else
static void PDU_AssociateRQ(struct zzfile *zz, const char *calledAET, const char *sopclass)
{
	long pos;

	znwstart(zz, 0x01);
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
	pos = ziwritepos(zz->zi);
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
	znw2at(ziwritepos(zz->zi) - pos - 4, pos, zz);

	// User Information Item Fields, see PS 3.8, Table 9-16
	znw1(0x50, zz);
	znw1(0, zz);				// reserved, shall be zero
	znw2(8, zz);				// size of packet
	znw1(0x51, zz);				// define maximum length of data field sub-item
	znw1(0, zz);				// reserved, shall be zero
	znw2(4, zz);				// size of following field
	znw4(0, zz);				// zero means unlimited size allowed

	// Now set size of entire message
	znw4at(ziwritepos(zz->zi) - 6, 2, zz);
	znwsendbuffer(zz);
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
			zz->zi = ziopensocket(new_fd, 0);
			zisetreader(zz->zi, readbuffer, zz);
			zisetwriter(zz->zi, headerwrite, 12, zz);
			while (true) // FIXME -- while (ziconnected(zz->zi))
			{
				printf("Awaiting type\n");
				zigetc(zz->zi);					// trigger read of packet
				zisetreadpos(zz->zi, zireadpos(zz->zi) - 1);	// reset read marker
				switch (zz->net.pdutype)
				{
				case 0x01: printf("Associate RQ received\n"); PDU_Associate_Accept(zz); break;
				case 0x04: printf("PData received\n"); PData_receive(zz); break;
				default: printf("Unknown type: %x\n", (unsigned)zz->net.pdutype); abort(); break;
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

	zz->zi = ziopensocket(sockfd, 0);
	zisetreader(zz->zi, readbuffer, zz);
	zisetwriter(zz->zi, headerwrite, 12, zz);
	return true;
}
