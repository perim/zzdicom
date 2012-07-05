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
	{ { "--trace", "Save a network trace dump to \"zzdiscp.dcm\"", false, false, 0, 0 }, // OPT_TRACE
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

static void zzdiserviceprovider(struct zznetwork *zzn)
{
	char str[80];
	uint16_t group, element;
	long len;
	bool loop = true;
	bool newservice = false;

	zzdireceiving(zzn);
	while (loop && zziternext(zzn->in, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiNetworkServiceSequence:
			newservice = true; // now waiting for DiNetworkService
			break;
		case DCM_DiNetworkService:
			assert(newservice);
			printf("Service = %s\n", zzgetstring(zzn->in, str, sizeof(str) - 1));
			zzdisending(zzn);
			if (strcmp(str, "STORE") == 0)
			{
				zzwCS(zzn->out, DCM_DiNetworkServiceStatus, "ACCEPTED"); // then await data
			}
			else if (strcmp(str, "INFO") == 0)
			{
				zzwCS(zzn->out, DCM_DiNetworkServiceStatus, "ACCEPTED");
				zzwSQ_begin(zzn->out, DCM_DiInfoEquipmentSequence, NULL);
				zzwItem_begin(zzn->out, NULL);
				zzwLO(zzn->out, DCM_Manufacturer, "zzdicom");
				zzwLO(zzn->out, DCM_ManufacturersModelName, "zzdicom");
				zzwLO(zzn->out, DCM_DeviceSerialNumber, "1"); // FIXME, read from setting?
				zzwLO(zzn->out, DCM_SoftwareVersions, "0.0.2"); // FIXME, read from file
				zzwItem_end(zzn->out, NULL);
				zzwSQ_end(zzn->out, NULL);
				zzwCS(zzn->out, DCM_DiInfoStatus, "SUCCESS"); // signal done
			}
			else
			{
				zzwCS(zzn->out, DCM_DiNetworkServiceStatus, "NO SUCH SERVICE");
			}
			ziflush(zzn->out->zi);
			zzdireceiving(zzn);
			newservice = false;
			break;
		case DCM_DiDisconnect:
			loop = false;
			printf("-- disconnect --\n");
			break;
		case DCM_ItemDelimitationItem:
		case DCM_SequenceDelimitationItem:
		case DCM_Item:
			break;
		default:
			printf("unknown tag (%04x,%04x)\n", group, element);
			break;	// ignore
		}
	}
}

int main(int argc, char **argv)
{
	struct zznetwork *zzn;

	(void) zzutil(argc, argv, 0, "", "DICOM experimental network server", opts);

	zzn = zznetlisten("", 5104, ZZNET_NO_FORK);
	if (zzn)
	{
		if (opts[OPT_TRACE].found)
		{
			zznettrace(zzn, "zzdiscp.dcm");
		}
		strcpy(zzn->out->net.aet, "TESTNODE");
		if (zzdinegotiation(zzn))
		{
			zzdiserviceprovider(zzn);
		}
		zznetclose(zzn);
	}
	return 0;
}
