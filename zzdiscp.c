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

extern const char *versionString;

static struct zzopts opts[] =
	{ { "--trace", "Save a network trace dump to \"zzdiscp.dcm\"", false, false, 0, 0 }, // OPT_TRACE
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

static void receive_access_request(struct zznetwork *zzn)
{
	bool processingAccess = true;
	int nesting = zzn->in->ladderidx;
	int group, element;
	long len;

	while (processingAccess && zziternext(zzn->in, &group, &element, &len))
	{
		// TODO -- handle fake delimiters
		switch (ZZ_KEY(group, element))
		{
		// TODO, collect, verify then dump info to sql db for later processing
		case DCM_DiAccessAuthorizing: // PN
		case DCM_DiAccessTechnicalContact: // PN
		case DCM_DiAccessTechnicalContactPhone: // LO
		case DCM_DiAccessMessage: // LT
		case DCM_DiAccessInstitutionDepartmentName: // LO
		case DCM_DiAccessEquipmentSequence: // SQ
			break;
		case DCM_SequenceDelimitationItem:
			processingAccess = false; // done with request
			break;
		default:
			break; // non-recognized field, ignore it
		}
	}
	zzwCS(zzn->out, DCM_DiAccessStatus, "QUEUED");
}

static void zzdiserviceprovider(struct zznetwork *zzn)
{
	char str[80];
	int group, element;
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
				zzwLO(zzn->out, DCM_SoftwareVersions, versionString);
				zzwItem_end(zzn->out, NULL);
				zzwSQ_end(zzn->out, NULL);
				zzwCS(zzn->out, DCM_DiInfoStatus, "SUCCESS"); // signal done
			}
			else if (strcmp(str, "ACCESS") == 0)
			{
				// TODO : Queue up access request for later processing
				zzwCS(zzn->out, DCM_DiNetworkServiceStatus, "ACCEPTED");
				receive_access_request(zzn);
				zzwCS(zzn->out, DCM_DiAccessStatus, "QUEUED");
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
