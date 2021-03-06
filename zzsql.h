#ifndef ZZSQL_H
#define ZZSQL_H

#define _XOPEN_SOURCE
#include <stdbool.h>
#include <sqlite3.h>

#include "zz_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DATETIME_FORMAT "%Y-%m-%d %H:%M:%S"

struct zzdb
{
	sqlite3 *sqlite;
};

struct zzdbiter
{
	sqlite3_stmt *stmt;
	int index;
	int retval;
};

bool zzdbupdate(struct zzdb *zdb, struct zzfile *zz);
struct zzdb *zzdbopen(struct zzdb *indb);
struct zzdb *zzdbclose(struct zzdb *zdb);

bool zzquery(struct zzdb *zdb, const char *statement, int (*callback)(void*,int,char**,char**), void *cbdata);

// TODO move
time_t zzundatetime(const char *datetime);
const char *zzdatetime(time_t value);

struct zzdbiter zzdbquery(struct zzdb *zdb, char const *fmt, ...);
bool zzdbnext(struct zzdb *zdb, struct zzdbiter *zq, const char *fmt, ...);
void zzdbdone(struct zzdbiter zq);

#ifdef __cplusplus
}
#endif

#endif
