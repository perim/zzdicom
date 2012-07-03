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

static int conv_nifti_file(char *dcm_file, char *hdr_file, char *data_file, char *resultfile)
{
	nifti_1_header *hdr;
	FILE *fp;
	int i, size, offset, msize;
	void *addr;
	char *bytes;
	struct zzfile szw, *zw;
	char *sopclassuid, *filename;
	int wrongendian;
	char uid[MAX_LEN_UI];
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len;
	char seriesdate[MAX_LEN_DA];
	char studydate[MAX_LEN_DA];
	char studyuid[MAX_LEN_UI];
	char studyid[MAX_LEN_LO];
	char frameofrefuid[MAX_LEN_UI];
	char patientsname[MAX_LEN_PN];
	char patientid[MAX_LEN_UI];
	char value[MAX_LEN_IS];
	int number;
	bool done = false;

	memset(value, 0, sizeof(value));
	memset(seriesdate, 0, sizeof(seriesdate));
	memset(studydate, 0, sizeof(studydate));
	memset(studyid, 0, sizeof(studyid));
	memset(patientsname, 0, sizeof(patientsname));
	memset(patientid, 0, sizeof(patientid));
	zzmakeuid(frameofrefuid, sizeof(frameofrefuid)); // just in case...
	zzmakeuid(studyuid, sizeof(studyuid)); // overwritten later, hopefully!
	strcpy(patientsname, "NO NAME FOUND");
	strcpy(patientid, "NO ID FOUND");

	if (!is_nifti_file(hdr_file))
	{
		fprintf(stderr, "%s is not a nifti file\n", hdr_file);
		return -1;
	}
	hdr = nifti_read_header(hdr_file, &wrongendian, 1);

	filename = strrchr(hdr_file, '/');
	if (filename)
	{
		filename++;	// skip slash 
	}
	else
	{
		filename = hdr_file;
	}

	if (hdr->dim[0] != 3)
	{
		fprintf(stderr, "%s - three dimensions not found (was %d), unsupported Analyze dataset\n", hdr_file, hdr->dim[0]);
		return -1;
	}

	fp = fopen(data_file, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "%s - error opening data file: %s\n", hdr_file, strerror(ferror(fp)));
		return -1;
	}

	switch (hdr->datatype)
	{
	case DT_UNSIGNED_CHAR:
		sopclassuid = UID_MultiframeGrayscaleByteSecondaryCaptureImageStorage;
		size = hdr->dim[1] * hdr->dim[2] * hdr->dim[3];
		break;
	case DT_SIGNED_SHORT:
	case DT_UINT16:
		sopclassuid = UID_MultiframeGrayscaleWordSecondaryCaptureImageStorage;
		size = hdr->dim[1] * hdr->dim[2] * hdr->dim[3] * 2;
		break;
	case DT_RGB:
		sopclassuid = UID_MultiframeTrueColorSecondaryCaptureImageStorage;
		size = hdr->dim[1] * hdr->dim[2] * hdr->dim[3] * 3;
		break;
	default:
		fprintf(stderr, "Unsupported data type %d -- skipped\n", (int)hdr->datatype);
		return -1;
	}

	zw = zzcreate(resultfile, &szw, sopclassuid, zzmakeuid(uid, sizeof(uid)), UID_LittleEndianExplicitTransferSyntax);
	if (!zw)
	{
		fprintf(stderr, "%s - could not create out file: %s\n", hdr_file, strerror(errno));
		zw = zzclose(zw);
		return -1;
	}

	// Grab some data from original
	zz = zzopen(dcm_file, "r", &szz);
	if (!zz)
	{
		return -1;
	}
	zziterinit(zz);
	while (!done && zziternext(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_StudyDate:
			zzgetstring(zz, studydate, sizeof(studydate) - 1);
			break;
		case DCM_SeriesDate:
			zzgetstring(zz, seriesdate, sizeof(seriesdate) - 1);
			break;
		case DCM_PatientsName:
			zzgetstring(zz, patientsname, sizeof(patientsname) - 1);
			break;
		case DCM_PatientID:
			zzgetstring(zz, patientid, sizeof(patientid) - 1);
			break;
		case DCM_StudyInstanceUID:
			zzgetstring(zz, studyuid, sizeof(studyuid) - 1);
			break;
		case DCM_StudyID:
			zzgetstring(zz, studyid, sizeof(studyid) - 1);
			break;
		case DCM_FrameOfReferenceUID:
			zzgetstring(zz, frameofrefuid, sizeof(frameofrefuid) - 1);
			break;
		case DCM_NumberOfFrames:  // this had better be the same
			zzgetstring(zz, value, sizeof(value) - 1);
			number = atoi(value);
			if (number != hdr->dim[3])
			{
				fprintf(stderr, "Original has %d frames, capture has %d! Aborting...\n", number, (int)hdr->dim[3]);
				return -1;
			}
			break;
		case DCM_FrameIncrementPointer:
			done = true;
			break;
		}
	}
	if (!done)
	{
		fprintf(stderr, "Could not find a frame increment pointer in original DICOM file! Aborting...\n");
		fprintf(stderr, "This application only works on multi-frame files!\n");
		return -1;
	}

	// just memory map all the data
	offset = (unsigned)hdr->vox_offset & ~(sysconf(_SC_PAGE_SIZE) - 1);	// start at page aligned offset
	msize = size + zw->current.pos - offset;
	addr = mmap(NULL, msize, PROT_READ, MAP_SHARED, fileno(fp), offset);
	if (addr == MAP_FAILED)
	{
		fprintf(stderr, "%s - could not mmap file: %s\n", hdr_file, strerror(errno));
		zw = zzclose(zw);
		return -1;
	}
	bytes = addr + zw->current.pos - offset;	// increment by page alignment shift
	madvise(bytes, size, MADV_SEQUENTIAL | MADV_WILLNEED);

	// Write DICOM tags
	zzwCS(zw, DCM_ImageType, "DERIVED\\SECONDARY");
	zzwUI(zw, DCM_SOPClassUID, sopclassuid);
	zzwUI(zw, DCM_SOPInstanceUID, uid); // reusing the UID generated above
	zzwDAs(zw, DCM_StudyDate, studydate);
	zzwDAs(zw, DCM_SeriesDate, seriesdate);
	zzwCS(zw, DCM_Modality, "SC");
	zzwCS(zw, DCM_ConversionType, "WSD");
	zzwLO(zw, DCM_SeriesDescription, hdr->descrip);
	zzwPN(zw, DCM_PatientsName, patientsname);
	zzwLO(zw, DCM_PatientID, patientid);
	zzwUI(zw, DCM_StudyInstanceUID, studyuid); // link original and secondary capture on study level
	zzwUI(zw, DCM_SeriesInstanceUID, zzmakeuid(uid, sizeof(uid))); // ... but not on series level
	zzwSH(zw, DCM_StudyID, studyid);
	zzwIS(zw, DCM_SeriesNumber, 1); // FIXME, this should probably be something unique, incremented
	zzwIS(zw, DCM_InstanceNumber, 1);
	// SliceLocationVector?
	// PatientOrientation?
	zzwUI(zw, DCM_FrameOfReferenceUID, frameofrefuid);
	zzwUS(zw, DCM_SamplesPerPixel, hdr->datatype != DT_RGB ? 1 : 3);
	zzwCS(zw, DCM_PhotometricInterpretation, hdr->datatype != DT_RGB ? "MONOCHROME2" : "RGB");
	zzwIS(zw, DCM_NumberOfFrames, hdr->dim[3]);
	zzwCopy(zw, zz);	// copying the FrameIncrementPointer
	zzwUS(zw, DCM_Rows, hdr->dim[1]);
	zzwUS(zw, DCM_Columns, hdr->dim[2]);
	zzwUS(zw, DCM_BitsAllocated, hdr->datatype != DT_UINT16 ? 8 : 16);
	zzwUS(zw, DCM_BitsStored, hdr->datatype != DT_UINT16 ? 8 : 16);
	zzwUS(zw, DCM_HighBit, hdr->datatype != DT_UINT16 ? 7 : 15);
	zzwUS(zw, DCM_PixelRepresentation, 0);
	zzwDSd(zw, DCM_RescaleIntercept, hdr->scl_inter);
	zzwDSd(zw, DCM_RescaleSlope, hdr->scl_slope);
	zzwLO(zw, DCM_RescaleType, "US");
	//-- Handle functional groups sequences
	// find this part in the original
	while (zziternext(zz, &group, &element, &len) && ZZ_KEY(group, element) != DCM_SharedFunctionalGroupsSequence) {}
	if (ZZ_KEY(group, element) != DCM_SharedFunctionalGroupsSequence)
	{
		fprintf(stderr, "Could not find shared functional group sequence! Aborting...\n");
		return -1;
	}
	// copy everything in this sequence! (and, somewhat counter-intuitively, the next...)
	number = zz->ladderidx;
	while (zziternext(zz, &group, &element, &len) && zz->ladderidx >= number)
	{
		zzwCopy(zw, zz);
	}
	zz = zzclose(zz);

	// Now write the pixels
	switch (hdr->datatype)
	{
	case DT_UNSIGNED_CHAR:
		zzwPixelData_begin(zw, hdr->dim[3], 8, hdr->dim[1] * hdr->dim[2] * hdr->dim[3]);
		for (i = 0; i < hdr->dim[3]; i++)
		{
			int framesize = hdr->dim[1] * hdr->dim[2];
			zzwPixelData_frame(zw, i, bytes + i * framesize, framesize);
		}
		zzwPixelData_end(zw);
		break;
	case DT_SIGNED_SHORT:
	case DT_UINT16:
		zzwPixelData_begin(zw, hdr->dim[3], 16, hdr->dim[1] * hdr->dim[2] * hdr->dim[3] * 2);
		if (!wrongendian)
		{
			for (i = 0; i < hdr->dim[3]; i++)
			{
				int framesize = hdr->dim[1] * hdr->dim[2] * 2;
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
		zzwPixelData_begin(zw, hdr->dim[3], 8, hdr->dim[1] * hdr->dim[2] * hdr->dim[3] * 3);
		for (i = 0; i < hdr->dim[3]; i++)
		{
			int framesize = hdr->dim[1] * hdr->dim[2] * 3;
			zzwPixelData_frame(zw, i, bytes + i * framesize, framesize);
		}
		zzwPixelData_end(zw);
		break;
	default:
		fprintf(stderr, "Unsupported data type -- skipped\n");
		return -1;
	}
	zw = zzclose(zw);

	madvise(bytes, size, MADV_DONTNEED);
	munmap(addr, msize);

	return 0;
}

int main(int argc, char **argv)
{
	zzutil(argc, argv, 3, "<dicom original> <header file> [<data file>] <dicom output>", "nifti to DICOM converter", NULL);
	if (argc == 5)
	{
		return conv_nifti_file(argv[1], argv[2], argv[3], argv[4]);
	}
	else
	{
		return conv_nifti_file(argv[1], argv[2], argv[2], argv[3]);
	}
}
