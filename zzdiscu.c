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

#if 0
static void zzdiserviceprovider(struct zzfile *zz)
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
	struct zzfile szz, *zz;

	(void) zzutil(argc, argv, 0, "", "DICOM experimental network client", NULL);

	zz = zznetconnect("", "127.0.0.1", 5104, &szz, 0);
	if (zz)
	{
		strcpy(zz->net.aet, "TESTCLIENT");
		zzdinegotiation(zz, false);
	}
	return 0;
}
