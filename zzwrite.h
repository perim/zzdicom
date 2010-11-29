#ifndef ZZ_WRITE_H
#define ZZ_WRITE_H

#include "zz_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

void zzwAE(struct zzfile *zz, zzKey key, const char *string);
void zzwAS(struct zzfile *zz, zzKey key, const char *string);
void zzwCS(struct zzfile *zz, zzKey key, const char *string);
void zzwPN(struct zzfile *zz, zzKey key, const char *string);
void zzwOB(struct zzfile *zz, zzKey key, const char *string, int length);
void zzwOW(struct zzfile *zz, zzKey key, const char *string, int length);
void zzwLO(struct zzfile *zz, zzKey key, const char *string);
void zzwLT(struct zzfile *zz, zzKey key, const char *string);
void zzwSH(struct zzfile *zz, zzKey key, const char *string);
void zzwUI(struct zzfile *zz, zzKey key, const char *string);
void zzwUL(struct zzfile *zz, zzKey key, uint32_t value);
void zzwSQ(struct zzfile *zz, zzKey key, uint32_t size);
void zzwUN(struct zzfile *zz, zzKey key, uint32_t size);

void zzwHeader(struct zzfile *zz, const char *sopclass, const char *sopinstanceuid, const char *transfer);
void zzwEmpty(struct zzfile *zz, zzKey key, const char *vr);
struct zzfile *zzcreate(const char *filename, struct zzfile *zz, const char *sopclass, const char *sopinstanceuid, const char *transfer);

/// Pass in NULL below to use unlimited size
void zzwUN_begin(struct zzfile *zz, zzKey key, long *pos);

/// Pass in NULL below to use unlimited size. Must be NULL here if NULL in call above.
void zzwUN_end(struct zzfile *zz, long *pos);

/// Pass in NULL below to use unlimited size
void zzwSQ_begin(struct zzfile *zz, zzKey key, long *pos);

/// Pass in NULL below to use unlimited size. Must be NULL here if NULL in call above.
void zzwSQ_end(struct zzfile *zz, long *pos);

#ifdef __cplusplus
}
#endif

#endif
