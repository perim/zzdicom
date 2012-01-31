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

static void zzdiserviceprovider(struct zzfile *zz)
{
	char str[80];
	uint16_t group, element;
	long len;
	bool loop = true;

	zzdinegotiation(zz);

	while (loop && zzread(zz, &group, &element, &len))
	{
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

int main(void)
{
	struct zzfile szz, *zz;
	zz = zznetlisten("", 5104, &szz, ZZNET_NO_FORK);
	strcpy(zz->net.aet, "TESTNODE");
	printf("Starting negotiation\n");
	zzdiserviceprovider(zz);
	return 0;
}
