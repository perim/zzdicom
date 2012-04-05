// Experimental alternative network protocol for transfering DICOM data

#include "zzio.h"
#include "zzwrite.h"
#include "zzditags.h"
#include "zznetwork.h"
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

enum
{
	OPT_TRACE,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--trace", "Save a network trace dump to \"zzdiscu.dcm\"", false, false }, // OPT_TRACE
	  { NULL, NULL, false, false } };              // OPT_COUNT

#if 0
static void zzdiserviceprovider(struct zzfile *zz, struct zzfile *tracefile)
{
	char str[80];
	uint16_t group, element;
	long len;
	bool loop = true;

	while (loop && zziternext(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiNetworkServiceSequence: printf("NEW SERVICE SQ"); break;
		case DCM_DiNetworkService:
			printf("Service = %s\n", zzgetstring(zz, str, sizeof(str) - 1));
			zzwCS(zz, DCM_DiNetworkServiceStatus, "ACCEPTED"); // TODO check validity of above
			ziflush(zz->zi);
			break;
		case DCM_DiDisconnect: loop = false; break;
		default: break;	// ignore
		}
	}
	ziclose(zz->zi);
}
#endif

int main(int argc, char **argv)
{
	struct zznetwork *zzn;

	(void) zzutil(argc, argv, 0, "", "DICOM experimental network client", opts);

	zzn = zznetconnect("", "127.0.0.1", 5104, 0);
	if (zzn)
	{
		if (opts[OPT_TRACE].found)
		{
			zznettrace(zzn, "zzdiscu.dcm");
		}
		strcpy(zzn->out->net.aet, "TESTCLIENT");
		if (zzdinegotiation(zzn))
		{
			zzdisending(zzn);
			zzwSQ_begin(zzn->out, DCM_DiNetworkServiceSequence, NULL);
			zzwItem_begin(zzn->out, NULL);
			zzwCS(zzn->out, DCM_DiNetworkService, "STORE");
			//zzwPN(zzn->out, DCM_DiNetworkRequestor, "");
			zzwItem_end(zzn->out, NULL);
			zzwSQ_end(zzn->out, NULL);
			zzwCS(zzn->out, DCM_DiDisconnect, "SUCCESS");
		}
		zznetclose(zzn);
	}
	return 0;
}
