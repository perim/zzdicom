#ifndef ZZ_PRIV_H
#define ZZ_PRIV_H

#include "zz.h"

#include <stdlib.h>
#include <stdio.h>

#define MAX_LEN_UID		64
#define MAX_LEN_PN		(64 * 5)
#define MAX_LEN_DATETIME	26
#define MAX_LEN_CS		16

struct part6
{
	uint16_t group;		// private
	uint16_t element;	// private
	const char *VR;
	const char *VM;
	bool retired;
	const char *description;
};

struct zzfile // TODO rename to zzframe
{
	FILE		*fp;
	uint32_t	headerSize;
	uint32_t	startPos;
	char		*fullPath;
	char		sopClassUid[MAX_LEN_UID];
	char		seriesInstanceUid[MAX_LEN_UID];
	char		sopInstanceUid[MAX_LEN_UID];
	char		transferSyntaxUid[MAX_LEN_UID];
	bool		acrNema;
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
static inline struct zzfile *zzclose(struct zzfile *zz) { fclose(zz->fp); free(zz->fullPath); free(zz); return NULL; }

#endif
