#ifndef ZZ_PRIV_H
#define ZZ_PRIV_H

#include "zz.h"

#include <stdlib.h>
#include <stdio.h>

struct part6
{
	uint16_t group;		// private
	uint16_t element;	// private
	const char *VR;
	const char *VM;
	bool retired;
	const char *description;
};

struct zzfile
{
	FILE		*fp;
	uint32_t	headerSize;
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
static inline struct zzfile *zzfree(struct zzfile *zz) { fclose(zz->fp); free(zz); return NULL; }

#endif
