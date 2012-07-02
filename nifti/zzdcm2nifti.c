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
	int num;
	struct zzfile szz, *zz;
	char value[MAX_LEN_LO];
	double imageposvector[3];
	double imageorientation[9];
	znzFile outfp = NULL;
	nifti_image hdr;
	double tmpd[2];
	uint16_t group, element;
	long len;

	zz = zzopen(dicomfile, "r", &szz);
	if (!zz)
	{
		return -1;
	}

	memset(&hdr, 0, sizeof(hdr));
	hdr.nz = 1;
	hdr.nt = 1;
	hdr.nu = 1;
	hdr.ndim = 3;
	hdr.nv = 1;
	hdr.nw = 1;
	hdr.dt = 0.0;
	hdr.du = 1.0;
	hdr.dv = 1.0;
	hdr.dw = 1.0;
	hdr.qform_code = NIFTI_XFORM_SCANNER_ANAT;
	hdr.freq_dim  = 1;
	hdr.phase_dim = 2;
	hdr.slice_dim = 3;

	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_SamplesPerPixel:
			// zzgetuint16(src, 0);
			break;
		case DCM_BitsAllocated:
			num = zzgetuint16(zz, 0);
			hdr.datatype = (num == 16) ? DT_UINT16 : DT_UINT8;
			break;
		case DCM_NumberOfFrames:
			zzgetstring(zz, value, sizeof(value) - 1);
			hdr.nz = atoi(value);
			break;
		case DCM_Rows:
			hdr.ny = zzgetuint16(zz, 0);
			break;
		case DCM_Columns:
			hdr.nx = zzgetuint16(zz, 0);
			break;
		case DCM_PatientsName:
			memset(hdr.descrip, 0, sizeof(hdr.descrip));
			zzgetstring(zz, hdr.descrip, sizeof(hdr.descrip) - 1);
			break;
		case DCM_ImagePositionPatient:		// DS, 3 values
			zzrDS(zz, 3, imageposvector);
			break;
		case DCM_ImageOrientationPatient:	// DS, 6 values
			zzrDS(zz, 6, imageorientation);
			break;
		case DCM_RescaleIntercept:	// DS, the b in m*SV + b
			zzgetstring(zz, value, sizeof(value) - 1);
			hdr.scl_inter = atof(value);
			break;
		case DCM_RescaleSlope:		// DS, the m in m*SV + b
			zzgetstring(zz, value, sizeof(value) - 1);
			hdr.scl_slope = atof(value);
			break;
		case DCM_SliceThickness:
			// TODO, calculate it if missing, or perhaps do it anyway
			zzrDS(zz, 1, tmpd);
			hdr.dz = tmpd[0];
			break;
		case DCM_PixelSpacing:
			zzrDS(zz, 2, tmpd);
			hdr.dx = tmpd[0];
			hdr.dy = tmpd[1];
			break;
		case DCM_PixelData:
			nifti_datatype_sizes(hdr.datatype, &(hdr.nbyper), &(hdr.swapsize));
			hdr.slice_end = hdr.nz - 1;
			hdr.nvox = hdr.nx * hdr.ny * hdr.nz * hdr.nt * hdr.nu * hdr.nv * hdr.nw;

			// Generate quaternion
			hdr.qto_xyz.m[0][0] = -imageorientation[0];
			hdr.qto_xyz.m[0][1] = -imageorientation[3];
			hdr.qto_xyz.m[0][2] = -(imageorientation[1] * imageorientation[5]
						- imageorientation[2] * imageorientation[4]);

			hdr.qto_xyz.m[1][0] = -imageorientation[1];
			hdr.qto_xyz.m[1][1] = -imageorientation[4];
			hdr.qto_xyz.m[1][2] = -(imageorientation[2] * imageorientation[3]
						- imageorientation[0] * imageorientation[5]);

			hdr.qto_xyz.m[2][0] = imageorientation[2];
			hdr.qto_xyz.m[2][1] = imageorientation[5];
			hdr.qto_xyz.m[2][2] = (imageorientation[0] * imageorientation[4]
					      - imageorientation[1] * imageorientation[3]);

			hdr.qoffset_x = -(imageposvector[0]);
			hdr.qoffset_y = -(imageposvector[1]);
			hdr.qoffset_z =  (imageposvector[2]);

			nifti_mat44_to_quatern(hdr.qto_xyz, &(hdr.quatern_b), &(hdr.quatern_c), &(hdr.quatern_d),
					       NULL, NULL, NULL, NULL, NULL, NULL, &(hdr.qfac));

			hdr.xyz_units = NIFTI_UNITS_MM;
			hdr.time_units = NIFTI_UNITS_MSEC;
			hdr.intent_code = NIFTI_INTENT_NONE;
			hdr.byteorder = nifti_short_order();
			hdr.nifti_type = 1;
			hdr.fname = strdup(niftifile);

			outfp = nifti_image_write_hdr_img(&hdr, 2, "wb");
			if (outfp == NULL)
			{
				printf("Failed to write nifti header file\n");
				return -1;
			}
			else
			{
				size_t size = hdr.nx * hdr.ny * hdr.nz * hdr.nbyper;
				size_t length = zz->fileSize - zz->current.pos;
				void *bytes = zireadbuf(zz->zi, length);
				size_t result = nifti_write_buffer(outfp, bytes, length);
				if (result != size)
				{
					fprintf(stderr, "Failed to write nifti data (size = %d, result = %d, expected = %d)\n", (int)length, (int)result, (int)size);
				}
				zifreebuf(zz->zi, bytes, length);
			}
			break;
		}
	}
	zz = zzclose(zz);

	return 0;
}

int main(int argc, char **argv)
{
	zzutil(argc, argv, 2, "<dicom file> <nifti file>", "DICOM to nifti converter", NULL);
	return write_nifti_file(argv[1], argv[2]);
}
