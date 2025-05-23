#include <assert.h>
#include <string.h>
#include "zznetwork.h"
#include "zznet.h"

enum
{
	OPT_REJECT,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--reject", "Reject all incoming transmissions", false, false, 0, 0 }, // OPT_REJECT
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

int main(int argc, char **argv)
{
	struct zznetwork *zn = NULL;
	uint16_t group, element, mesID = 1;
	long len;

	(void) zzutil(argc, argv, 0, "", "DICOM network test server", opts);

	zn = zznetlisten("", 5104, ZZNET_NO_FORK);
	strcpy(zn->out->net.aet, "ECHOSCP");
	zznetregister(zn);
	zigetc(zn->in->zi);	// trigger read of first packet
	do
	{
		printf("PDU Type %ld received (%ld)\n", zn->in->net.pdutype, zn->out->net.pdutype);
		zisetreadpos(zn->in->zi, zireadpos(zn->in->zi) - 1);	// reset read marker
		switch (zn->in->net.pdutype)
		{
		case 0x01:
			printf("ASSOCIATE-RQ received\n");
			if (opts[OPT_REJECT].found)
			{
				PDU_AssociateRJ(zn->out, 0x01, 0x01, 0x01);
				zznetclose(zn);
				return 0;
				break;
			}
			PDU_Associate_Request(zn);
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
