// Experimental alternative network protocol for transfering DICOM data

#include "zzio.h"
#include "zzwrite.h"
#include "zzditags.h"
#include "zznetwork.h"
#include "zzdinetwork.h"
#include "zzini.h"

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
	OPT_REQUEST,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--trace", "Save a network trace dump to \"zzdiscu.dcm\"", false, false, 0, 0 }, // OPT_TRACE
	  { "--request <file>", "Generate an access request from given input file", false, false, 1, 0 }, // OPT_REQUEST
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

static const char *readini(struct zzini *ini, const char *key)
{
	char buf[256];
	const char *ptr;
	memset(buf, 0, sizeof(buf));
	ptr = zzinivalue(ini, "request", key, buf, sizeof(buf) - 1);
	if (ptr)
	{
		return ptr;
	}
	else
	{
		fprintf(stderr, "Missing field \"%s\" in input file\n", key);
		return ""; // don't crash
	}
}

int main(int argc, char **argv)
{
	struct zznetwork *zzn;
	char uid[MAX_LEN_UI];
	int argstart = zzutil(argc, argv, 0, "", "DICOM experimental network client", opts);

	zzmakeuid(uid, sizeof(uid));
	zzn = zznetconnect("", "127.0.0.1", 5104, 0);
	if (zzn)
	{
		if (opts[OPT_TRACE].found)
		{
			zznettrace(zzn, "zzdiscu.dcm");
		}
		strcpy(zzn->out->net.aet, "TESTCLIENT");
		if (!zzdinegotiation(zzn))
		{
			zznetclose(zzn);
			return -1;
		}
		if (opts[OPT_REQUEST].found) // send an access request
		{
			struct zzini sini, *ini = zziniopen(&sini, argv[opts[OPT_REQUEST].argstart + 1]);
			if (!ini)
			{
				zznetclose(zzn);
				return -1;
			}
			zzdisending(zzn);
			zzwSQ_begin(zzn->out, DCM_DiNetworkServiceSequence, NULL);
			zzwItem_begin(zzn->out, NULL);
			zzwCS(zzn->out, DCM_DiNetworkService, "ACCESS");
			zzwUI(zzn->out, DCM_DiNetworkServiceUID, uid);
			zzwPN(zzn->out, DCM_DiAccessAuthorizing, readini(ini, "Authorizing"));
			zzwPN(zzn->out, DCM_DiAccessTechnicalContact, readini(ini, "TechnicalContact"));
			zzwLO(zzn->out, DCM_DiAccessTechnicalContactPhone, readini(ini, "TechnicalContactPhone"));
			zzwLT(zzn->out, DCM_DiAccessMessage, readini(ini, "Message"));
			zzwLO(zzn->out, DCM_DiAccessInstitutionDepartmentName, readini(ini, "InstitutionDepartmentName"));
			zzwSQ_begin(zzn->out, DCM_DiAccessEquipmentSequence, NULL);
			zzwItem_begin(zzn->out, NULL);
			zzwLO(zzn->out, DCM_Manufacturer, readini(ini, "Manufacturer"));
			zzwLO(zzn->out, DCM_ManufacturersModelName, readini(ini, "ModelName"));
			zzwLO(zzn->out, DCM_DeviceSerialNumber, readini(ini, "DeviceSerialNumber"));
			zzwLO(zzn->out, DCM_SoftwareVersions, readini(ini, "SoftwareVersions"));
			zzwItem_end(zzn->out, NULL);
			zzwSQ_end(zzn->out, NULL);
			zzwLO(zzn->out, DCM_DiAccessPrivilegeRequested, readini(ini, "PrivilegeRequested"));
			zzwAE(zzn->out, DCM_DiAccessAETITLE, readini(ini, "AETITLE"));
			zzwLO(zzn->out, DCM_DiAccessIP, readini(ini, "IP"));
			zzwLO(zzn->out, DCM_DiAccessPort, readini(ini, "Port"));
			if (zzinicontains(ini, "request", "ServiceContact"))
			{
				zzwPN(zzn->out, DCM_DiAccessServiceContact, readini(ini, "ServiceContact"));
				zzwLO(zzn->out, DCM_DiAccessServiceContactPhone, readini(ini, "ServiceContactPhone"));
			}
			// TODO : DCM_DiAccessPublicKey
			zzwCS(zzn->out, DCM_DiAccessAuthenticationMethods, readini(ini, "AuthenticationMethods"));
			zzwItem_end(zzn->out, NULL);
			zzwSQ_end(zzn->out, NULL);
			zziniclose(ini);
		}
		else // send an info request
		{
			zzdisending(zzn);
			zzwSQ_begin(zzn->out, DCM_DiNetworkServiceSequence, NULL);
			zzwItem_begin(zzn->out, NULL);
			zzwCS(zzn->out, DCM_DiNetworkService, "INFO");
			zzwUI(zzn->out, DCM_DiNetworkServiceUID, uid);
			zzwItem_end(zzn->out, NULL);
			zzwSQ_end(zzn->out, NULL);
			zzwCS(zzn->out, DCM_DiDisconnect, "SUCCESS");
		}
		zznetclose(zzn);
	}
	return 0;
}
