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

void zzdisending(struct zznetwork *zzn)
{
	if (starteditem)
	{
		zzwItem_end(zzn->trace, NULL);
	}
	if (zzisverbose())
	{
		printf("\033[22m\033[33mSENDING -> %s: \033[0m\n", zzn->in->net.address);
	}
	if (zzn->trace)
	{
		zzwItem_begin(zzn->trace, NULL);
		zzwCS(zzn->trace, DCM_DiTraceLog, "SENT");
		starteditem = true;
	}
}

void zzdireceiving(struct zznetwork *zzn)
{
	if (starteditem)
	{
		zzwItem_end(zzn->trace, NULL);
	}
	if (zzisverbose())
	{
		printf("\033[22m\033[33mRECEIVING <- %s: \033[0m\n", zzn->in->net.address);
	}
	if (zzn->trace)
	{
		zzwItem_begin(zzn->trace, NULL);
		zzwCS(zzn->trace, DCM_DiTraceLog, "RECEIVED");
		starteditem = true;
	}
}

static void zzdireceiving2(struct zznetwork *zzn, const char *str)
{
	if (starteditem)
	{
		zzwItem_end(zzn->trace, NULL);
	}
	if (zzisverbose())
	{
		printf("\033[22m\033[33mRECEIVING <- %s: \033[0m\n", zzn->in->net.address);
	}
	if (zzn->trace)
	{
		zzwItem_begin(zzn->trace, NULL);
		zzwCS(zzn->trace, DCM_DiTraceLog, str);//"RECEIVED");
		starteditem = true;
	}
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

static void initialsending(struct zznetwork *zzn)
{
	struct timeval tv;

	zzwCS(zzn->out, DCM_DiProtocolIdentification, "DICOMDIRECT1");
	zzwLO(zzn->out, DCM_DiPeerName, zzn->out->net.aet);
	zzwLO(zzn->out, DCM_DiPeerKeyHash, "");	// TODO
	gettimeofday(&tv, NULL);
	zzwDT(zzn->out, DCM_DiPeerCurrentDateTime, tv);
	ziflush(zzn->out->zi);
}

bool zzdinegotiation(struct zznetwork *zzn)
{
	char str[80];
	bool loop;
	int group, element;
	long len;

	zzn->in->utf8 = zzn->out->utf8 = true;
	zzn->in->ladder[0].txsyn = zzn->out->ladder[0].txsyn = ZZ_EXPLICIT;
	zzn->in->ladder[0].type = zzn->out->ladder[0].type = ZZ_BASELINE;

	if (!zzn->server) // client goes first
	{
		zzdisending(zzn);
		initialsending(zzn);
	}
	zziterinit(zzn->in);
	loop = true;
	zzdireceiving2(zzn, "RECEIVED");
	while (loop && zziternext(zzn->in, &group, &element, &len))
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
		zzdiprint(zzn->in, zzgetstring(zzn->in, str, sizeof(str) - 1));
	}

	zzdisending(zzn);
	if (zzn->server)
	{
		initialsending(zzn);
	}
	zzwCS(zzn->out, DCM_DiPeerStatus, "ACCEPTED"); // TODO, no checking... should at least check datetime for testing
	zzwOB(zzn->out, DCM_DiKeyChallenge, 0, "");
	ziflush(zzn->out->zi);

	loop = true;
	zzdireceiving2(zzn, "RECEIVED");
	while (loop && zziternext(zzn->in, &group, &element, &len))
	{
		memset(str, 0, sizeof(str));
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiPeerStatus: break;
		case DCM_DiKeyChallenge: loop = false; break;
		default: printf("ignoring (%04x, %04x)\n", group, element); break;	// ignore
		}
		zzdiprint(zzn->in, zzgetstring(zzn->in, str, sizeof(str) - 1));
	}
	zzdisending(zzn);
	zzwOB(zzn->out, DCM_DiKeyResponse, 0, "");	// TODO
	ziflush(zzn->out->zi);

	loop = true;
	zzdireceiving2(zzn, "RECEIVED");
	while (loop && zziternext(zzn->in, &group, &element, &len))
	{
		memset(str, 0, sizeof(str));
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiKeyResponse: loop = false; break;
		default: break;	// ignore
		}
		zzdiprint(zzn->in, zzgetstring(zzn->in, str, sizeof(str) - 1));
	}
	zzdisending(zzn);
	zzwCS(zzn->out, DCM_DiConnectionStatus, "ACCEPTED");
	ziflush(zzn->out->zi);

	loop = true;
	zzdireceiving2(zzn, "RECEIVED");
	while (loop && zziternext(zzn->in, &group, &element, &len))
	{
		memset(str, 0, sizeof(str));
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiConnectionStatus: 
			zzgetstring(zzn->in, str, sizeof(str) - 1);
			if (strcmp(str, "CHALLENGE FAILURE") == 0)
			{
				warning("Key challenge failed during negotiation");
				return false;
			}
			else if (strcmp(str, "ACCESS DENIED") == 0)
			{
				warning("Access denied during negotiation");
				return false;
			}
			else if (strcmp(str, "ACCEPTED") == 0)
			{
				loop = false; 	// success
			}
			else
			{
				warning("Unknown connection status received: %s", str);
				return false;
			}
			break;
		default:
			break;	// ignore
		}
		zzdiprint(zzn->in, zzgetstring(zzn->in, str, sizeof(str) - 1));
	}
	// We are now connected and operational
	if (loop) printf("Ran out of data!\n");
	return !loop;
}
