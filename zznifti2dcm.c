#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdio.h>

#include "nifti1.h"
#include "byteorder.h"
#include "zzwrite.h"

#define MIN_HEADER_SIZE 348

static bool read_nifti_file(char *hdr_file, char *data_file)
{
	nifti_1_header hdr;
	FILE *fp;
	int ret, i, size, offset, msize;
	void *addr;
	char *bytes;
	struct zzfile szw, *zw;
	char *sopclassuid;
	long sq1, sq2, item1, item2;
	bool wrongendian;

	fp = fopen(hdr_file, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error opening header file %s: %s\n", hdr_file, strerror(ferror(fp)));
		return false;
	}
	ret = fread(&hdr, MIN_HEADER_SIZE, 1, fp);
	if (feof(fp))
	{
		fprintf(stderr, "%s - error reading header file: File too small\n", hdr_file);
		return false;
	}
	else if (ret != 1)
	{
		fprintf(stderr, "%s - error reading header file: %s\n", hdr_file, strerror(ferror(fp)));
		return false;
	}
	fclose (fp);

	wrongendian = (hdr.sizeof_hdr > 348 * 10);	// ten times the size of normal header size? must be wrong endian

	if (wrongendian)	// byte swap some fields that we use
	{
		hdr.sizeof_hdr = BSWAP_32(hdr.sizeof_hdr);
		hdr.dim[0] = BSWAP_16(hdr.dim[0]);
		hdr.dim[1] = BSWAP_16(hdr.dim[1]);
		hdr.dim[2] = BSWAP_16(hdr.dim[2]);
		hdr.dim[3] = BSWAP_16(hdr.dim[3]);
		hdr.dim[4] = BSWAP_16(hdr.dim[4]);
		hdr.datatype = BSWAP_16(hdr.datatype);
		hdr.bitpix = BSWAP_16(hdr.bitpix);
		hdr.scl_slope = BSWAP_32(hdr.scl_slope);
		hdr.scl_inter = BSWAP_32(hdr.scl_inter);
		hdr.vox_offset = BSWAP_32(hdr.vox_offset);
		hdr.pixdim[0] = BSWAP_32(hdr.pixdim[0]);
		hdr.pixdim[1] = BSWAP_32(hdr.pixdim[1]);
		hdr.pixdim[2] = BSWAP_32(hdr.pixdim[2]);
	}

#if 0
	/********** print a little header information */
	fprintf (stderr, "\n%s header information (%d):", hdr_file, hdr.sizeof_hdr);
	fprintf (stderr, "\nXYZT dimensions: %d %d %d %d", hdr.dim[1], hdr.dim[2], hdr.dim[3], hdr.dim[4]);
	fprintf (stderr, "\nDatatype code and bits/pixel: %d %d", hdr.datatype, hdr.bitpix);
	fprintf (stderr, "\nScaling slope and intercept: %.6f %.6f", hdr.scl_slope, hdr.scl_inter);
	fprintf (stderr, "\nByte offset to data in datafile: %ld", (long) (hdr.vox_offset) );
	fprintf (stderr, "\n");
#endif

	if (hdr.dim[0] != 3)
	{
		fprintf(stderr, "%s - three dimensions not found (was %d), unsupported Analyze dataset\n", hdr_file, hdr.dim[0]);
		return false;
	}

	fp = fopen(data_file, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "%s - error opening data file: %s\n", hdr_file, strerror(ferror(fp)));
		return false;
	}

	switch (hdr.datatype)
	{
	case DT_UNSIGNED_CHAR:
		sopclassuid = UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage;
		size = hdr.dim[1] * hdr.dim[2] * hdr.dim[3];
		break;
	case DT_SIGNED_SHORT:
	case DT_UINT16:
		sopclassuid = UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage;
		size = hdr.dim[1] * hdr.dim[2] * hdr.dim[3] * 2;
		break;
	case DT_RGB:
		sopclassuid = UID_MultiframeTrueColorSecondaryCaptureImageStorage;
		size = hdr.dim[1] * hdr.dim[2] * hdr.dim[3] * 3;
		break;
	default:
		fprintf(stderr, "Unsupported data type %d -- skipped\n", (int)hdr.datatype);
		return false;
	}

	// FIXME instance uid
	zw = zzcreate("niftitest.dcm", &szw, sopclassuid, "1.2.3.4", UID_JPEGLSLosslessTransferSyntax);
	if (!zw)
	{
		fprintf(stderr, "%s - could not create out file: %s\n", hdr_file, strerror(errno));
		zw = zzclose(zw);
		return false;
	}

	// just memory map all the data
	offset = (unsigned)hdr.vox_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);	// start at page aligned offset
	msize = size + zw->current.pos - offset;
	addr = mmap(NULL, msize, PROT_READ, MAP_SHARED, fileno(fp), offset);
	if (addr == MAP_FAILED)
	{
		fprintf(stderr, "%s - could not mmap file: %s\n", hdr_file, strerror(errno));
		zw = zzclose(zw);
		return false;
	}
	bytes = addr + zw->current.pos - offset;	// increment by page alignment shift
	madvise(bytes, size, MADV_SEQUENTIAL | MADV_WILLNEED);

	// Write DICOM tags
	zzwCS(zw, DCM_ImageType, "DERIVED\\SECONDARY");
	zzwUI(zw, DCM_SOPClassUID, sopclassuid);
	zzwUI(zw, DCM_SOPInstanceUID, "1.2.3.4");	// FIXME!
	// StudyDate
	// StudyTime
	zzwCS(zw, DCM_Modality, "SC");
	zzwCS(zw, DCM_ConversionType, "WSD");
	zzwLO(zw, DCM_SeriesDescription, hdr.descrip);
	zzwPN(zw, DCM_PatientsName, "Analyze Conversion");
	zzwLO(zw, DCM_PatientID, "Careful, Experimental");
	// StudyInstanceUID
	// SeriesInstanceUID
	zzwSH(zw, DCM_StudyID, "zznifti2dcm");
	zzwIS(zw, DCM_SeriesNumber, 1);
	zzwIS(zw, DCM_InstanceNumber, 1);
	// SliceLocationVector
	// PatientOrientation
	// FrameOfReferenceUID
	zzwCS(zw, DCM_PatientFrameOfReferenceSource, "ESTIMATED");
	zzwUS(zw, DCM_SamplesPerPixel, hdr.datatype != DT_RGB ? 1 : 3);
	zzwCS(zw, DCM_PhotometricInterpretation, hdr.datatype != DT_RGB ? "MONOCHROME2" : "RGB");
	zzwIS(zw, DCM_NumberOfFrames, hdr.dim[3]);
	// FrameIncrementPointer
	zzwUS(zw, DCM_Rows, hdr.dim[1]);
	zzwUS(zw, DCM_Columns, hdr.dim[2]);
	zzwUS(zw, DCM_BitsAllocated, hdr.datatype != DT_UINT16 ? 8 : 16);
	zzwUS(zw, DCM_BitsStored, hdr.datatype != DT_UINT16 ? 8 : 16);
	zzwUS(zw, DCM_HighBit, hdr.datatype != DT_UINT16 ? 7 : 15);
	zzwUS(zw, DCM_PixelRepresentation, 0);
	zzwDSf(zw, DCM_RescaleIntercept, hdr.scl_inter);
	zzwDSf(zw, DCM_RescaleSlope, hdr.scl_slope);
	zzwLO(zw, DCM_RescaleType, "US");
	zzwSQ_begin(zw, DCM_SharedFunctionalGroupsSequence, &sq1);
		zzwItem_begin(zw, &item1);
			zzwSQ_begin(zw, DCM_PlaneOrientationSequence, &sq2);
				zzwItem_begin(zw, &item2);
				// TODO
				zzwItem_end(zw, &item2);
			zzwSQ_end(zw, &sq2);
			zzwSQ_begin(zw, DCM_PixelMeasuresSequence, &sq2);
				zzwItem_begin(zw, &item2);
				// TODO
				zzwItem_end(zw, &item2);
			zzwSQ_end(zw, &sq2);
		zzwItem_end(zw, &item1);
	zzwSQ_end(zw, &sq1);
	zzwSQ_begin(zw, DCM_PerFrameFunctionalGroupsSequence, &sq1);
		for (i = 0; i < hdr.dim[3]; i++)
		{
			zzwItem_begin(zw, &item1);
				zzwSQ_begin(zw, DCM_PlanePositionSequence, &sq2);
					zzwItem_begin(zw, &item2);
					// TODO
					zzwItem_end(zw, &item2);
				zzwSQ_end(zw, &sq2);
			zzwItem_end(zw, &item1);
		}
	zzwSQ_end(zw, &sq1);

	// Now write the pixels
	switch (hdr.datatype)
	{
	case DT_UNSIGNED_CHAR:
		zzwPixelData_begin(zw, hdr.dim[3], OB);
		for (i = 0; i < hdr.dim[3]; i++)
		{
			int framesize = hdr.dim[0] * hdr.dim[1];
			zzwPixelData_frame(zw, i, bytes + i * framesize, framesize);
		}
		zzwPixelData_end(zw);
		break;
	case DT_SIGNED_SHORT:
	case DT_UINT16:
		zzwPixelData_begin(zw, hdr.dim[3], OW);
		if (!wrongendian)
		{
			for (i = 0; i < hdr.dim[3]; i++)
			{
				int framesize = hdr.dim[0] * hdr.dim[1] * 2;
				zzwPixelData_frame(zw, i, bytes + i * framesize, framesize);
			}
		}
		else
		{
			for (i = 0; i < size; i++)
			{
				// TODO, need byte swapping op
				fprintf(stderr, "16bit wrong endian data not supported yet -- skipped\n");
			}
		}
		zzwPixelData_end(zw);
		break;
	case DT_RGB:
		zzwPixelData_begin(zw, hdr.dim[3], OB);
		for (i = 0; i < hdr.dim[3]; i++)
		{
			int framesize = hdr.dim[0] * hdr.dim[1] * 3;
			zzwPixelData_frame(zw, i, bytes + i * framesize, framesize);
		}
		zzwPixelData_end(zw);
		break;
	default:
		fprintf(stderr, "Unsupported data type -- skipped\n");
		return false;
	}

	madvise(bytes, size, MADV_DONTNEED);
	munmap(addr, msize);

#if 0
	/********** scale the data buffer  */
	if (hdr.scl_slope != 0) {
		for (i = 0; i < hdr.dim[1]*hdr.dim[2]*hdr.dim[3]; i++) {
			data[i] = (data[i] * hdr.scl_slope) + hdr.scl_inter;
		}
	}
#endif

	return (0);
}

int main(int argc, char **argv)
{
	zzutil(argc, argv, 2, "<header file> [<data file>]", "nifti to DICOM converter");
	if (argc == 3)
	{
		read_nifti_file(argv[1], argv[2]);
	}
	else
	{
		read_nifti_file(argv[1], argv[1]);
	}

	return 0;
}
