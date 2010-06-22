#ifndef ZZ_PRIV_H
#define ZZ_PRIV_H

#include "zz.h"

#include <stdlib.h>
#include <stdio.h>

// From dcmtk:

/// Implicit VR Little Endian: Default Transfer Syntax for DICOM
#define UID_LittleEndianImplicitTransferSyntax  "1.2.840.10008.1.2"
/// Explicit VR Little Endian
#define UID_LittleEndianExplicitTransferSyntax  "1.2.840.10008.1.2.1"
/// Explicit VR Big Endian
#define UID_BigEndianExplicitTransferSyntax     "1.2.840.10008.1.2.2"

#define MAX_LEN_UID		(64 + 1)
#define MAX_LEN_PN		(64 * 5 + 1)
#define MAX_LEN_DATETIME	(26 + 1)
#define MAX_LEN_CS		(16 + 1)

/// Enumerant for Value Representations. Approach taken from XMedCon.
enum VR
{
	AE = ('A'<<8)|'E',
	AS = ('A'<<8)|'S',
	AT = ('A'<<8)|'T',
	CS = ('C'<<8)|'S',
	DA = ('D'<<8)|'A',
	DS = ('D'<<8)|'S',
	DT = ('D'<<8)|'T',
	FL = ('F'<<8)|'L',
	FD = ('F'<<8)|'D',
	IS = ('I'<<8)|'S',
	LO = ('L'<<8)|'O',
	LT = ('L'<<8)|'T',
	OB = ('O'<<8)|'B',
	OW = ('O'<<8)|'W',
	OF = ('O'<<8)|'F',
	PN = ('P'<<8)|'N',
	SH = ('S'<<8)|'H',
	SL = ('S'<<8)|'L',
	SQ = ('S'<<8)|'Q',
	SS = ('S'<<8)|'S',
	ST = ('S'<<8)|'T',
	TM = ('T'<<8)|'M',
	UI = ('U'<<8)|'I',
	UL = ('U'<<8)|'L',
	US = ('U'<<8)|'S',
	UN = ('U'<<8)|'N',
	UT = ('U'<<8)|'T',
	/* special tag (multiple choices) */
	OX = ('O'<<8)|'X'
};

#define ZZ_VR(_m1, _m2) ((_m1 << 8) | _m2)

struct part6
{
	uint16_t group;		// private
	uint16_t element;	// private
	const char *VR;
	const char *VM;
	bool retired;
	const char *description;
};

enum zzbasetype
{
	ZZ_IMPLICIT,
	ZZ_TEMPORARY_EXPLICIT,
	ZZ_EXPLICIT,
	ZZ_TEMPORARY_IMPLICIT	// anything higher than this is recursively ZZ_TEMPORARY_IMPLICIT, used for UN VR
};

struct zzfile
{
	FILE		*fp;
	uint64_t	fileSize;
	uint32_t	headerSize;
	uint32_t	startPos;
	char		*fullPath;
	char		sopClassUid[MAX_LEN_UID];
	char		seriesInstanceUid[MAX_LEN_UID];
	char		sopInstanceUid[MAX_LEN_UID];
	char		transferSyntaxUid[MAX_LEN_UID];
	bool		acrNema;
	time_t		modifiedTime;
	enum zzbasetype	baseType;
	int		currNesting, nextNesting;
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

uint32_t zzgetuint32(struct zzfile *zz);
uint16_t zzgetuint16(struct zzfile *zz);
int32_t zzgetint32(struct zzfile *zz);
int16_t zzgetint16(struct zzfile *zz);

struct zzfile *zzopen(const char *filename, const char *mode);
bool zzread(struct zzfile *zz, uint16_t *group, uint16_t *element, uint32_t *len);
const struct part6 *zztag(uint16_t group, uint16_t element);
static inline struct zzfile *zzclose(struct zzfile *zz) { if (zz) { fclose(zz->fp); free(zz->fullPath); free(zz); } return NULL; }

/// Utility function to process some common command-line arguments. Returns the number of initial arguments to ignore.
int zzutil(int argc, char **argv, int minArgs, const char *usage, const char *help);

#endif
