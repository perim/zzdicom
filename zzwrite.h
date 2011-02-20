#ifndef ZZ_WRITE_H
#define ZZ_WRITE_H

#include "zz_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

bool zzwAE(struct zzfile *zz, zzKey key, const char *string);
bool zzwAS(struct zzfile *zz, zzKey key, const char *string);
bool zzwAT(struct zzfile *zz, zzKey key, zzKey key2);
bool zzwCS(struct zzfile *zz, zzKey key, const char *string);
bool zzwDA(struct zzfile *zz, zzKey key, time_t datestamp);
bool zzwDT(struct zzfile *zz, zzKey key, struct timeval datetimestamp);
bool zzwDSs(struct zzfile *zz, zzKey key, const char *string);
bool zzwDSd(struct zzfile *zz, zzKey key, double value);
bool zzwDSdv(struct zzfile *zz, zzKey key, int len, const double *value);
bool zzwFL(struct zzfile *zz, zzKey key, float value);
bool zzwFD(struct zzfile *zz, zzKey key, double value);
bool zzwIS(struct zzfile *zz, zzKey key, int value);
bool zzwLO(struct zzfile *zz, zzKey key, const char *string);
bool zzwLT(struct zzfile *zz, zzKey key, const char *string);
bool zzwOB(struct zzfile *zz, zzKey key, int len, const char *string);
bool zzwOF(struct zzfile *zz, zzKey key, int len, const float *string);
bool zzwOW(struct zzfile *zz, zzKey key, int len, const uint16_t *string);
bool zzwPN(struct zzfile *zz, zzKey key, const char *string);
bool zzwSH(struct zzfile *zz, zzKey key, const char *string);
bool zzwSL(struct zzfile *zz, zzKey key, int32_t value);
bool zzwSS(struct zzfile *zz, zzKey key, int16_t value);
bool zzwST(struct zzfile *zz, zzKey key, const char *string);
bool zzwTM(struct zzfile *zz, zzKey key, struct timeval datetimestamp);
bool zzwUI(struct zzfile *zz, zzKey key, const char *string);
bool zzwUL(struct zzfile *zz, zzKey key, uint32_t value);
bool zzwULv(struct zzfile *zz, zzKey key, int len, const uint32_t *value);
bool zzwUS(struct zzfile *zz, zzKey key, uint16_t value);
bool zzwUT(struct zzfile *zz, zzKey key, const char *string);

void zzwHeader(struct zzfile *zz, const char *sopclass, const char *sopinstanceuid, const char *transfer);
bool zzwEmpty(struct zzfile *zz, zzKey key, enum VR vr);
struct zzfile *zzcreate(const char *filename, struct zzfile *zz, const char *sopclass, const char *sopinstanceuid, const char *transfer);

/// Pass in NULL below to use unlimited size
void zzwUN_begin(struct zzfile *zz, zzKey key, long *pos);

/// Pass in NULL below to use unlimited size. Must be NULL here if NULL in call above.
void zzwUN_end(struct zzfile *zz, long *pos);

/// Pass in NULL below to use unlimited size. pos will contain start position of SQ.
void zzwSQ_begin(struct zzfile *zz, zzKey key, long *pos);

/// Pass in NULL below to use unlimited size. Must be NULL here if NULL in call above. pos will contain size of sequence.
void zzwSQ_end(struct zzfile *zz, long *pos);

void zzwItem_begin(struct zzfile *zz, long *pos);
void zzwItem_end(struct zzfile *zz, long *pos);

void zzwPixelData_begin(struct zzfile *zz, long frames, enum VR vr);
void zzwPixelData_frame(struct zzfile *zz, int frame, const char *data, uint32_t size);
void zzwPixelData_end(struct zzfile *zz);

#ifdef __cplusplus
}
#endif

#endif
