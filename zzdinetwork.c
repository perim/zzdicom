// Experimental alternative network protocol for transfering DICOM data

#include "zznet.h"
#include "zzio.h"
#include "zzwrite.h"
#include "zzditags.h"
#include "zzdinetwork.h"

#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

static bool starteditem = false;

void zzdisending(struct zzfile *zz, struct zzfile *tracefile)
{
	if (starteditem)
	{
		zzwItem_end(tracefile, NULL);
	}
	if (zzisverbose())
	{
		printf("\033[22m\033[33mSENDING -> %s: \033[0m\n", zz->net.address);
	}
	zzwItem_begin(tracefile, NULL);
	zzwCS(tracefile, DCM_DiTraceLog, "SENT");
	starteditem = true;
}

void zzdireceiving(struct zzfile *zz, struct zzfile *tracefile)
{
	if (starteditem)
	{
		zzwItem_end(tracefile, NULL);
	}
	if (zzisverbose())
	{
		printf("\033[22m\033[33mRECEIVING <- %s: \033[0m\n", zz->net.address);
	}
	zzwItem_begin(tracefile, NULL);
	zzwCS(tracefile, DCM_DiTraceLog, "RECEIVED");
	starteditem = true;
}

void zzdiprint(struct zzfile *zz, const char *str)
{
	if (zzisverbose())
	{
		char vrstr[MAX_LEN_VR];
		printf("  \033[22m\033[32m(%04x,%04x)\033[0m %s [%s]\n",
		       zz->current.group, zz->current.element, zzvr2str(zz->current.vr, vrstr), str);
	}
}

bool zzdinegotiation(struct zzfile *zz, bool server, struct zzfile *tracefile)
{
	struct timeval tv;
	char str[80];
	bool loop;
	uint16_t group, element;
	long len;

	(void)server; // FIXME

	zz->utf8 = true;
	zz->ladder[0].txsyn = ZZ_EXPLICIT;
	zz->ladder[0].type = ZZ_BASELINE;

	zzdisending(zz, tracefile);
	zzwCS(zz, DCM_DiProtocolIdentification, "DICOMDIRECT1");
	zzwLO(zz, DCM_DiPeerName, zz->net.aet);
	zzwLO(zz, DCM_DiPeerKeyHash, "");	// TODO
	gettimeofday(&tv, NULL);
	zzwDT(zz, DCM_DiPeerCurrentDateTime, tv);
	ziflush(zz->zi);

	zziterinit(zz);

	loop = true;
	zzdireceiving(zz, tracefile);
	while (loop && zziternext(zz, &group, &element, &len))
	{
		memset(str, 0, sizeof(str));
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiProtocolIdentification: break;
		case DCM_DiPeerName: break;
		case DCM_DiPeerCurrentDateTime: loop = false; break;
		case DCM_DiPeerKeyHash: break;
		default: printf("(%04x,%04x) ignored\n", (unsigned)group, (unsigned)element); break;	// ignore
		}
		zzdiprint(zz, zzgetstring(zz, str, sizeof(str) - 1));
	}

	zzdisending(zz, tracefile);
	zzwCS(zz, DCM_DiPeerStatus, "ACCEPTED"); // TODO, no checking... should at least check datetime for testing
	zzwOB(zz, DCM_DiKeyChallenge, 0, "");
	ziflush(zz->zi);

	loop = true;
	zzdireceiving(zz, tracefile);
	while (loop && zziternext(zz, &group, &element, &len))
	{
		memset(str, 0, sizeof(str));
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiPeerStatus: break;
		case DCM_DiKeyChallenge: loop = false; break;
		default: break;	// ignore
		}
		zzdiprint(zz, zzgetstring(zz, str, sizeof(str) - 1));
	}
	zzdisending(zz, tracefile);
	zzwOB(zz, DCM_DiKeyResponse, 0, "");	// TODO
	ziflush(zz->zi);

	loop = true;
	zzdireceiving(zz, tracefile);
	while (loop && zziternext(zz, &group, &element, &len))
	{
		memset(str, 0, sizeof(str));
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiKeyResponse: loop = false; break;
		default: break;	// ignore
		}
		zzdiprint(zz, zzgetstring(zz, str, sizeof(str) - 1));
	}
	zzdisending(zz, tracefile);
	zzwCS(zz, DCM_DiKeyStatus, "ACCEPTED");
	ziflush(zz->zi);

	loop = true;
	zzdireceiving(zz, tracefile);
	while (loop && zziternext(zz, &group, &element, &len))
	{
		memset(str, 0, sizeof(str));
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiKeyStatus: loop = false; break;
		default: break;	// ignore
		}
		zzdiprint(zz, zzgetstring(zz, str, sizeof(str) - 1));
	}
	// We are now connected and operational
	if (loop) printf("Ran out of data!\n");
	return !loop;
}
