// Experimental alternative network protocol for transfering DICOM data

#include "zzio.h"
#include "zzwrite.h"
#include "zzditags.h"
#include "zznetwork.h"
#include "zzdinetwork.h"
#include "zzsql.h"

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

static char rbuf[4096];

extern const char *versionString;

static struct zzopts opts[] =
	{ { "--trace", "Save a network trace dump to \"zzdiscp.dcm\"", false, false, 0, 0 }, // OPT_TRACE
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

static void receive_access_request(struct zznetwork *zzn)
{
	bool processingAccess = true;
	int nesting = zzn->in->ladderidx;
	uint16_t group, element;
	long len;
	char uid[MAX_LEN_UI];
	char authorizing[MAX_LEN_PN];
	char techcontact[MAX_LEN_PN];
	char techphone[MAX_LEN_LO];
	char institution[MAX_LEN_LO];
	char *message = NULL;

	memset(uid, 0, sizeof(uid));
	memset(authorizing, 0, sizeof(authorizing));
	memset(techcontact, 0, sizeof(techcontact));
	memset(techphone, 0, sizeof(techphone));
	memset(institution, 0, sizeof(institution));
	while (processingAccess && zziternext(zzn->in, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_DiAccessEquipmentSequence: // SQ
			break; // ignore
		case DCM_DiAccessAuthorizing: // PN
			zzgetstring(zzn->in, authorizing, sizeof(authorizing) - 1);
			break;
		case DCM_DiAccessTechnicalContact: // PN
			zzgetstring(zzn->in, techcontact, sizeof(techcontact) - 1);
			break;
		case DCM_DiAccessTechnicalContactPhone: // LO
			zzgetstring(zzn->in, techphone, sizeof(techphone) - 1);
			break;
		case DCM_DiAccessMessage: // LT
			len = MIN(len, (long)4096); // safety measure
			message = calloc(1, len);
			zzgetstring(zzn->in, message, len);
			break;
		case DCM_DiAccessInstitutionDepartmentName: // LO
			zzgetstring(zzn->in, institution, sizeof(institution) - 1);
			break;
		case DCM_DiNetworkServiceUID: // UI
			zzgetstring(zzn->in, uid, sizeof(uid) - 1);
			break;
		case DCM_SequenceDelimitationItem:
			processingAccess = false; // done with request
			break;
		default:
			break; // non-recognized field, ignore it
		}
	}
	// TODO, verify that all required data sent first, and no collisions/duplicates
	if (!message || uid[0] == '\0')
	{
		// TODO, use bitfield to denote required fields? then clear missing fields in a loop while printing them to log?
		fprintf(stderr, "Missing essential data for access request\n");
	}
	zzwCS(zzn->out, DCM_DiAccessStatus, "QUEUED");
	// dump info to sql db for later processing
	struct zzdb szdb, *zdb = zzdbopen(&szdb);
	sprintf(rbuf, "INSERT INTO accessrequests(uid, message, authorizing, technicalcontactname, technicalcontactphone, message, aetitle) "
	        "VALUES(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"AETITLE\")",
	        uid, message != NULL ? message : "", authorizing, techcontact, techphone, message);
	if (!zzquery(zdb, rbuf, NULL, NULL))
	{
		fprintf(stderr, "Failed to insert access request into database\n");
	}
	zdb = zzdbclose(zdb);
	free(message);
}

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
		case DCM_DiNetworkServiceUID:
			// hack for INFO, which does not receive anything
			break;
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
				zzwCS(zzn->out, DCM_DiNetworkServiceStatus, "NO SERVICE");
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
