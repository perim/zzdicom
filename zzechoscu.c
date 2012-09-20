#include <assert.h>
#include <string.h>
#include "zznetwork.h"
#include "zznet.h"

enum
{
	OPT_AET,
	OPT_CAE,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--aet <AET>", "Our AE title", false, false, 1, 0 }, // OPT_AET
	  { "--cae <AET>", "Destination AE title", false, false, 1, 0 }, // OPT_CAE
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

#define UID_VerificationSOPClass "1.2.840.10008.1.1"

int main(int argc, char **argv)
{
	struct zznetwork *zn = NULL;
	uint16_t group, element, mesID = 1;
	long len;
	const char *aet = "ECHOSCU";
	const char *cae = "ANY-SCP";
	int argstart = zzutil(argc, argv, 2, "<host> <port>", "DICOM network test client", opts);

	zn = zznetconnect("", argv[argstart], atoi(argv[argstart + 1]), 0);
	if (!zn)
	{
		fprintf(stderr, "Failed to connect\n");
		return -1;
	}
	if (opts[OPT_AET].found)
	{
		aet = argv[opts[OPT_AET].argstart + 1];
	}
	if (opts[OPT_CAE].found)
	{
		cae = argv[opts[OPT_CAE].argstart + 1];
	}
	strcpy(zn->out->net.aet, aet);
	zznetregister(zn);
	PDU_AssociateRQ(zn->out, cae, UID_VerificationSOPClass);
	zigetc(zn->in->zi);	// trigger read of first packet
	do
	{
		printf("PDU Type %ld received (%ld)\n", zn->in->net.pdutype, zn->out->net.pdutype);
		zisetreadpos(zn->in->zi, zireadpos(zn->in->zi) - 1);	// reset read marker
		switch (zn->in->net.pdutype)
		{
		case 0x01:
			printf("ASSOCIATE-RQ received\n");
			PDU_Associate_Request(zn);
			break;
		case 0x02:
			printf("ASSOCIATE-AC received\n");
			PDU_Associate_Accept(zn->in);
			break;
		case 0x04:
			zziterinit(zn->in);
			while (zziternext(zn->in, &group, &element, &len))
			{
				switch (ZZ_KEY(group, element))
				{
				case DCM_CommandGroupLength: break;
				case DCM_AffectedSOPClassUID: break;
				case DCM_MessageID: mesID = zzgetuint16(zn->in, 0); break;
				default: break;
				}
			}
			// Now send back an echo response
			znwechoresp(zn->out, mesID);
			break;
		case 0x05:
			PDU_Release_Handle(zn);
			zznetclose(zn);
			return 0;
		case 0x07:
			PDU_Abort_Parse(zn);
			zznetclose(zn);
			return 0;
		default:
			printf("Unknown PDU type: %x\n", (unsigned)zn->in->net.pdutype);
			assert(false);
			break;
		}
		zigetc(zn->in->zi); // Force read of PDU type for next packet
	} while (zn->in->net.pdutype != UNLIMITED);
	zznetclose(zn);
	return 0;
}
