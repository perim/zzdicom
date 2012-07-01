#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdio.h>

#include "byteorder.h"
#include "zzwrite.h"
#include "nifti1_io.h"

#define MIN_HEADER_SIZE 348

static int write_nifti_file(char *dicomfile, char *niftifile)
{
	nifti_1_header *hdr;
	FILE *fp;
	int i, size, offset, msize;
	void *addr;
	char *bytes;
	struct zzfile szz, *zz;
	char *sopclassuid, *filename;
	long sq1, sq2, item1, item2;
	int wrongendian;
	char uid[MAX_LEN_UI];

	if (!is_nifti_file(niftifile))
	{
		fprintf(stderr, "%s is not a nifti file\n", niftifile);
		return -1;
	}
	
	zz = zzopenfile(dicomfile, "r");

	// ... lots of stuff TODO ...

	return 0;
}

int main(int argc, char **argv)
{
	zzutil(argc, argv, 2, "<dicom file> <nifti file>", "DICOM to nifti converter", NULL);
	return write_nifti_file(argv[1], argv[2]);
}
