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
	{ { "--trace", "Save a network trace dump to \"zzdiscp.dcm\"", false }, // OPT_TRACE
	  { NULL, NULL, false } };              // OPT_COUNT

static void zzdiserviceprovider(struct zzfile *zz)
{
	char str[80];
	uint16_t group, element;
	long len;
	bool loop = true;

	zzdireceiving(zz);
	while (loop && zziternext(zz, &group, &element, &len))
	{
printf("scp loop!\n");
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiNetworkServiceSequence:
			printf("NEW SERVICE SQ\n");
			break;
		case DCM_DiNetworkService:
			printf("Service = %s\n", zzgetstring(zz, str, sizeof(str) - 1));
			zzwCS(zz, DCM_DiNetworkServiceStatus, "ACCEPTED"); // TODO check validity of above
			ziflush(zz->zi);
			break;
		case DCM_DiDisconnect:
			loop = false;
			printf("-- disconnect --\n");
			break;
		default:
			printf("unknown tag\n");
			break;	// ignore
		}
	}
	ziclose(zz->zi);
}

int main(int argc, char **argv)
{
	struct zzfile szz, *zz, stracefile, *tracefile = NULL;

	(void) zzutil(argc, argv, 0, "", "DICOM experimental network server", opts);

	zz = zznetlisten("", 5104, &szz, ZZNET_NO_FORK);
	if (zz)
	{
		if (opts[OPT_TRACE].found)
		{
			char buf[100];
			memset(buf, 0, sizeof(buf));
			tracefile = zzcreate("zzdiscp.dcm", &stracefile, "1.2.3.4", zzmakeuid(buf, sizeof(buf) - 1), 
			                     UID_LittleEndianExplicitTransferSyntax);
			if (tracefile)
			{
				zitee(zz->zi, tracefile->zi);
			}
		}
		strcpy(zz->net.aet, "TESTNODE");
		if (zzdinegotiation(zz, true, tracefile))
		{
			zzdiserviceprovider(zz);
		}
	}
	tracefile = zzclose(tracefile);
	return 0;
}
