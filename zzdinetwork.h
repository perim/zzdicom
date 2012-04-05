#ifndef ZZ_DINETWORK_H
#define ZZ_DINETWORK_H

#include "zznetwork.h"
#include "zz_priv.h"

/// Negotiate a Direct DICOM network connection
bool zzdinegotiation(struct zznetwork *zzn);

// debug functions
void zzdireceiving(struct zznetwork *zzn);
void zzdisending(struct zznetwork *zzn);

#endif
