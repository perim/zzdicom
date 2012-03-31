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
	struct zzfile szz, *zz, stracefile, *tracefile = NULL;

	(void) zzutil(argc, argv, 0, "", "DICOM experimental network client", opts);

	zz = zznetconnect("", "127.0.0.1", 5104, &szz, 0);
	if (zz)
	{
		if (opts[OPT_TRACE].found)
		{
			char buf[100];
			memset(buf, 0, sizeof(buf));
			tracefile = zzcreate("zzdiscu.dcm", &stracefile, "1.2.3.4", zzmakeuid(buf, sizeof(buf) - 1), 
			                     UID_LittleEndianExplicitTransferSyntax);
			if (tracefile)
			{
				zzwSQ_begin(tracefile, DCM_DiTraceSequence, NULL);
				zitee(zz->zi, tracefile->zi, ZZIO_TEE_READ | ZZIO_TEE_WRITE);
			}
		}
		strcpy(zz->net.aet, "TESTCLIENT");
		if (zzdinegotiation(zz, false, tracefile))
		{
			long sq;
			zzdisending(zz, tracefile);
			zzwSQ_begin(zz, DCM_DiNetworkServiceSequence, &sq);
			zzwCS(zz, DCM_DiNetworkService, "STORE");
			//zzwPN(zz, DCM_DiNetworkRequestor, "");
			zzwSQ_end(zz, &sq);
			zzwCS(zz, DCM_DiDisconnect, "SUCCESS");
		}
		if (tracefile)
		{
			zzwSQ_end(tracefile, NULL);
		}
	}
	tracefile = zzclose(tracefile);
	return 0;
}
