#include "zznet.h"

int main(void)
{
	uint16_t group, element, mesID;
	struct zzfile szz, *zz;
	long len;

	zz = zznetwork(NULL, "ECHOSCU", &szz);
	zzlisten(zz, 5104, "TEST", 0);
	do
	{
		switch (zz->net.pdutype)
		{
		case 0x04:
			zziterinit(zz);
			while (zziternext(zz, &group, &element, &len))
			{
				switch (ZZ_KEY(group, element))
				{
				case DCM_CommandGroupLength: break;
				case DCM_AffectedSOPClassUID: break;
				case DCM_MessageID: mesID = zzgetuint16(zz, 0); break;
				default: break;
				}
			}
			// Now send back an echo response
			znwechoresp(zz, mesID);
			break;
		case 0x07:
			// FIXME, parse and log before quitting
			zz = zzclose(zz);
			return 0;
		default:
			printf("Unknown PDU type: %x\n", (unsigned)zz->net.pdutype);
			break;
		}
		zigetc(zz->zi); // Force read of PDU type for next packet
	} while (zz->net.pdutype != UNLIMITED);
	zz = zzclose(zz);
	return 0;
}
