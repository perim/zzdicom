#include <string.h>

#include "zz_priv.h"
#include "byteorder.h"

// Magic values
#define MAGIC1 0xfffee00d
#define MAGIC2 BSWAP_32(0xfffee00d)
#define MAGIC3 0xfffee0dd
#define MAGIC4 BSWAP_32(0xfffee0dd)
#define MAGIC5 ((0x0010 << 24) | (0x0001 << 16) | ('S' << 8) | ('S'))

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
	int length = MIN(strlen(string), 64);
	int wlen = length;

	if (length % 2 != 0) wlen++;			// padding
	if (syntax == EXPLICIT) explicit1(fp, group, element, "UI", wlen);
	else implicit(fp, group, element, wlen);
	fwrite(string, 1, length, fp);
	if (length % 2 != 0) fwrite("", 1, 1, fp);	// pad with null
}

static void wstr(FILE *fp, uint16_t group, uint16_t element, const char *string, size_t maxlen)
{
	int length = MIN(strlen(string), maxlen);
	int wlen = length;

	if (length % 2 != 0) wlen++;			// padding
	if (syntax == EXPLICIT) explicit1(fp, group, element, "UI", wlen);
	else implicit(fp, group, element, wlen);
	fwrite(string, 1, length, fp);
	if (length % 2 != 0) fwrite(" ", 1, 1, fp);	// pad with spaces
}
static inline void wSH(FILE *fp, uint16_t group, uint16_t element, const char *string) { wstr(fp, group, element, string, 16); }

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
	wSH(fp, 0x0002, 0x0013, "TEST");
//	wAE(fp, 0x0002, 0x0016, "TEST");

	// write group size
	size = ftell(fp) - (startpos + 4);
	fseek(fp, startpos, SEEK_SET);
	fwrite(&size, 4, 1, fp);	// set size
	fseek(fp, 0, SEEK_END);		// return position
}

static void emptyvalue(FILE *fp, uint16_t group, uint16_t element, const char *vr)
{
	if (syntax == EXPLICIT) explicit1(fp, group, element, vr, 0);
	else implicit(fp, group, element, 0);
}

static void genericfile(FILE *fp, const char *sopclass)
{
	wUI(fp, 0x0008, 0x0016, sopclass);
	wUI(fp, 0x0008, 0x0018, "1.2.3.4.0");
	emptyvalue(fp, 0x0008, 0x0020, "DA");
	emptyvalue(fp, 0x0008, 0x0030, "TM");
	wSH(fp, 0x0008, 0x0050, "1234567890123456");
	emptyvalue(fp, 0x0008, 0x0090, "PN");
	emptyvalue(fp, 0x0010, 0x0010, "PN");
	emptyvalue(fp, 0x0010, 0x0020, "LO");
	emptyvalue(fp, 0x0010, 0x0030, "DA");
	emptyvalue(fp, 0x0010, 0x0040, "CS");
	wUI(fp, 0x0020, 0x000d, "1.2.3.4.1");
	wUI(fp, 0x0020, 0x000e, "1.2.3.4.2");
	emptyvalue(fp, 0x0020, 0x0010, "SH");
	emptyvalue(fp, 0x0020, 0x0011, "IS");
	emptyvalue(fp, 0x0020, 0x0013, "IS");
	emptyvalue(fp, 0x0020, 0x0060, "CS");
}

static void garbfill(FILE *fp)
{
	switch (rand() % 5)
	{
	case 0: wUL(fp, 0x0028, 0x9001, MAGIC1); wUL(fp, 0x0028, 0x9002, MAGIC2); break;
	case 1: wUL(fp, 0x0028, 0x9001, MAGIC3); wUL(fp, 0x0028, 0x9002, MAGIC4); break;
	case 2: wUL(fp, 0x0028, 0x9001, MAGIC2); wUL(fp, 0x0028, 0x9002, MAGIC5); break;
	case 3: wUL(fp, 0x0028, 0x9001, MAGIC5); wUL(fp, 0x0028, 0x9002, MAGIC3); break;
	default:
	case 4: wUL(fp, 0x0028, 0x9001, MAGIC4); wUL(fp, 0x0028, 0x9002, MAGIC1); break;
	}
}

int main(int argc, char **argv)
{
	FILE *fp = fopen("random.dcm", "w");

	(void)argc;
	(void)argv;

	zzutil(argc, argv, 1, "<random seed>", "Generate pseudo-random DICOM file for unit testing");
	if (argc > 1)
	{
		srand(atoi(argv[1]));
	}
	else
	{
		srand(1);
	}

	switch (rand() % 15)
	{
	case 0: syntax = EXPLICIT; break;	// no header, explicit
	case 1: syntax = IMPLICIT; break;	// no header, implicit
	case 2: syntax = EXPLICIT; header(fp, UID_SecondaryCaptureImageStorage, UID_LittleEndianImplicitTransferSyntax); syntax = IMPLICIT; break;	// implicit with explicit header
	case 3: syntax = IMPLICIT; header(fp, UID_SecondaryCaptureImageStorage, UID_LittleEndianImplicitTransferSyntax); break;	// implicit header, buggy like CTN
	default: syntax = EXPLICIT; header(fp, UID_SecondaryCaptureImageStorage, UID_LittleEndianExplicitTransferSyntax); break;	// normal case
	}
	genericfile(fp, UID_SecondaryCaptureImageStorage);

	if (syntax == EXPLICIT && rand() % 10 > 2)	// add SQ block
	{
		explicit2(fp, 0x0020, 0x1115, "SQ", UNLIMITED);
		implicit(fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(fp);
		implicit(fp, 0xfffe, 0xe00d, 0);
		implicit(fp, 0xfffe, 0xe000, 24);
		garbfill(fp);
		if (rand() % 10 > 9) implicit(fp, 0xfffe, 0xe00d, 0);	// this crashed dicom3tools; not really legal dicom
		implicit(fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(fp);
		implicit(fp, 0xfffe, 0xe00d, 0);
		implicit(fp, 0xfffe, 0xe0dd, 0);
	}

	if (rand() % 10 > 5) garbfill(fp);

	if (syntax == EXPLICIT && rand() % 10 > 2)	// add UN block
	{
		enum syntax old_syntax = syntax;
		explicit2(fp, 0x0029, 0x0010, "UN", UNLIMITED);
		syntax = IMPLICIT;
		implicit(fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(fp);
		implicit(fp, 0xfffe, 0xe00d, 0);
		implicit(fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(fp);
		implicit(fp, 0xfffe, 0xe00d, 0);
		implicit(fp, 0xfffe, 0xe0dd, 0);
		syntax = old_syntax;
	}

	fclose(fp);

	return 0;
}
