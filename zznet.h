#ifndef ZZ_NET_H
#define ZZ_NET_H

#include <stdbool.h>

struct zznetwork;
struct zzfile;

// This file is for functions specific to DIMSE/ACSE support

/// Add supported presentation context
bool zznetaddctx(struct zzfile *zz, const char *uid);

/// Send an echo request.
void znwechoreq(struct zzfile *zz);

/// Send an echo response. Add echo message ID received as parameter.
void znwechoresp(struct zzfile *zz, long mesID);

void PDU_AssociateRJ(struct zzfile *zz, char result, char source, char diag);
bool PDU_Associate_Request(struct zznetwork *zn);
bool PDU_Abort_Parse(struct zznetwork *zn);
bool PDU_Release_Handle(struct zznetwork *zn);
void zznetregister(struct zznetwork *zn);

#endif
