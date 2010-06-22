#include <string.h>

#include "zz_priv.h"

#define UNLIMITED 0xffffffff

enum syntax
{
	IMPLICIT, EXPLICIT
};

static enum syntax syntax = EXPLICIT;

static void implicit(FILE *fp, uint16_t group, uint16_t element, uint32_t length)
{
	fwrite(&group, 2, 1, fp);
	fwrite(&element, 2, 1, fp);
	fwrite(&length, 4, 1, fp);
}

static void explicit1(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint16_t length)
{
	fwrite(&group, 2, 1, fp);
	fwrite(&element, 2, 1, fp);
	fwrite(&vr[0], 1, 1, fp);
	fwrite(&vr[1], 1, 1, fp);
	fwrite(&length, 2, 1, fp);
}

static void explicit2(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint32_t length)
{
	uint16_t zero = 0;

	fwrite(&group, 2, 1, fp);
	fwrite(&element, 2, 1, fp);
	fwrite(&vr[0], 1, 1, fp);
	fwrite(&vr[1], 1, 1, fp);
	fwrite(&zero, 2, 1, fp);
	fwrite(&length, 4, 1, fp);
}

static void wUL(FILE *fp, uint16_t group, uint16_t element, uint32_t value)
{
	if (syntax == EXPLICIT) explicit1(fp, group, element, "UL", sizeof(value));
	else implicit(fp, group, element, sizeof(value));
	fwrite(&value, sizeof(value), 1, fp);
}

static void wOB(FILE *fp, uint16_t group, uint16_t element, const char *string, int length)
{
	int wlen = length;
	if (length % 2 != 0) wlen++;			// padding
	if (syntax == EXPLICIT) explicit2(fp, group, element, "OB", length);
	else implicit(fp, group, element, length);
	fwrite(string, 1, length, fp);
	if (length % 2 != 0) fwrite("", 1, 1, fp);	// pad
}

static void wUI(FILE *fp, uint16_t group, uint16_t element, const char *string)
{
	int length = strlen(string) + 1;
	int wlen = length;

	if (length % 2 != 0) wlen++;			// padding
	if (syntax == EXPLICIT) explicit1(fp, group, element, "UI", wlen);
	else implicit(fp, group, element, wlen);
	fwrite(string, 1, length, fp);
	if (length % 2 != 0) fwrite("", 1, 1, fp);	// pad
}

static void header(FILE *fp, const char *sopclass, const char *transfer)
{
	char version[3];
	const char *dicm = "DICM";
	char *zeroes = malloc(128);
	uint32_t size;
	const uint32_t startpos = 128 + 4 + 8;

	version[0] = 0;
	version[1] = 1;
	version[2] = 0;
	memset(zeroes, 0, sizeof(zeroes));

	fwrite(zeroes, 1, 128, fp);
	fwrite(dicm, 1, 4, fp);

	wUL(fp, 0x0002, 0x0000, 0);	// length zero, fixing it below
	wOB(fp, 0x0002, 0x0001, version, 2);
	wUI(fp, 0x0002, 0x0002, sopclass);
	wUI(fp, 0x0002, 0x0003, "1.2.3.4.0");
	wUI(fp, 0x0002, 0x0010, transfer);
	wUI(fp, 0x0002, 0x0012, "1.2.3.4.8.2");
//	wSH(fp, 0x0002, 0x0013, "TEST");
//	wAE(fp, 0x0002, 0x0016, "TEST");

	// write group size
	size = ftell(fp) - (startpos + 4);
	fseek(fp, startpos, SEEK_SET);
	fwrite(&size, 4, 1, fp);	// set size
	fseek(fp, 0, SEEK_END);		// return position
}

static void explicitgeneric(FILE *fp, const char *sopclass)
{
	wUI(fp, 0x0008, 0x0016, sopclass);
	wUI(fp, 0x0008, 0x0018, "1.2.3.4.0");

	explicit1(fp, 0x0008, 0x0020, "DA", 0);
	explicit1(fp, 0x0008, 0x0030, "TM", 0);
	explicit1(fp, 0x0008, 0x0050, "SH", 0);
	explicit1(fp, 0x0008, 0x0090, "PN", 0);
	explicit1(fp, 0x0010, 0x0010, "PN", 0);
	explicit1(fp, 0x0010, 0x0020, "LO", 0);
	explicit1(fp, 0x0010, 0x0030, "DA", 0);
	explicit1(fp, 0x0010, 0x0040, "CS", 0);
	wUI(fp, 0x0020, 0x000d, "1.2.3.4.1");
	wUI(fp, 0x0020, 0x000e, "1.2.3.4.2");
	explicit1(fp, 0x0020, 0x0010, "SH", 0);
	explicit1(fp, 0x0020, 0x0011, "IS", 0);
	explicit1(fp, 0x0020, 0x0013, "IS", 0);
	explicit1(fp, 0x0020, 0x0060, "CS", 0);
}

static void garbfill(FILE *fp)
{
	wUL(fp, 0x0028, 0x9001, 1);
	wUL(fp, 0x0028, 0x9002, 2);
}

int main(int argc, char **argv)
{
	FILE *fp = fopen("random.dcm", "w");

	(void)argc;
	(void)argv;

	zzutil(argc, argv, 2, "<random seed>", "Generate pseudo-random DICOM file for unit testing");
	srand(atoi(argv[1]));

	if (rand() % 10 > 2) header(fp, UID_SecondaryCaptureImageStorage, UID_LittleEndianExplicitTransferSyntax);
	explicitgeneric(fp, UID_SecondaryCaptureImageStorage);

	explicit2(fp, 0x0020, 0x1115, "SQ", UNLIMITED);
	implicit(fp, 0xfffe, 0xe000, UNLIMITED);
	garbfill(fp);
	implicit(fp, 0xfffe, 0xe00d, 0);
	implicit(fp, 0xfffe, 0xe000, 24);
	garbfill(fp);
//	implicit(fp, 0xfffe, 0xe00d, 0);	-- this crashed dicom3tools; not really legal dicom
	implicit(fp, 0xfffe, 0xe000, UNLIMITED);
	garbfill(fp);
	implicit(fp, 0xfffe, 0xe00d, 0);
	implicit(fp, 0xfffe, 0xe0dd, 0);

	wUL(fp, 0x0028, 0x9001, 1);	// marker

	explicit2(fp, 0x0029, 0x0010, "UN", UNLIMITED);
	syntax = IMPLICIT;
	implicit(fp, 0xfffe, 0xe000, UNLIMITED);
	garbfill(fp);
	implicit(fp, 0xfffe, 0xe00d, 0);
	implicit(fp, 0xfffe, 0xe000, UNLIMITED);
	garbfill(fp);
	implicit(fp, 0xfffe, 0xe00d, 0);
	implicit(fp, 0xfffe, 0xe0dd, 0);
	syntax = EXPLICIT;

	fclose(fp);

	return 0;
}
