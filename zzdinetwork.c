// Experimental alternative network protocol for transfering DICOM data

#include "zznet.h"
#include "zzio.h"
#include "zzwrite.h"
#include "zzditags.h"

#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

void zzdinegotiation(struct zzfile *zz)
{
	struct timeval tv;
	char str[80];
	bool loop;
	uint16_t group, element;
	long len;

	zz->utf8 = true;
	zz->ladder[0].txsyn = ZZ_EXPLICIT;
	zz->ladder[0].type = ZZ_BASELINE;

	zzwCS(zz, DCM_DiProtocolIdentification, "DICOMDIRECT1");
	zzwLO(zz, DCM_DiPeerName, zz->net.aet);
	zzwLO(zz, DCM_DiPeerKeyHash, "");	// TODO
	gettimeofday(&tv, NULL);
	zzwDT(zz, DCM_DiPeerCurrentDateTime, tv);
	ziflush(zz->zi);

	zziterinit(zz);

	// FIXME: use zziternext() here instead of zzread directly, otherwise we have to read
	// every value to the full
	loop = true;
	while (loop && zzread(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiProtocolIdentification: printf("Protocol identified: %s\n", zzgetstring(zz, str, sizeof(str) - 1)); break;
		case DCM_DiPeerName: printf("Connected to %s\n", zzgetstring(zz, str, sizeof(str) - 1)); break;
		case DCM_DiPeerCurrentDateTime: loop = false; break;
		case DCM_DiPeerKeyHash: printf("KeyHash received\n"); break; // TODO
		default: printf("(%04x,%04x) ignored\n", (unsigned)group, (unsigned)element); break;	// ignore
		}
	}

	zzwCS(zz, DCM_DiPeerStatus, "ACCEPTED"); // TODO, no checking... should at least check datetime for testing
	zzwOB(zz, DCM_DiKeyChallenge, 0, "");
	ziflush(zz->zi);

	loop = true;
	while (loop && zzread(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiPeerStatus: printf("Peer status = %s\n", zzgetstring(zz, str, sizeof(str) - 1)); break;
		case DCM_DiKeyChallenge: loop = false; break;
		default: break;	// ignore
		}
	}
	zzwOB(zz, DCM_DiKeyResponse, 0, "");	// TODO
	ziflush(zz->zi);

	loop = true;
	while (loop && zzread(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiKeyResponse: loop = false; break;
		default: break;	// ignore
		}
	}
	zzwCS(zz, DCM_DiKeyStatus, "ACCEPTED");
	ziflush(zz->zi);

	loop = true;
	while (loop && zzread(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiKeyStatus: printf("Key status = %s\n", zzgetstring(zz, str, sizeof(str) - 1)); loop = false; break;
		default: break;	// ignore
		}
	}
	// We are now connected and operational
}
