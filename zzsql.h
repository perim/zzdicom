#ifndef ZZSQL_H
#define ZZSQL_H

#define _XOPEN_SOURCE
#include <time.h>
#include <stdbool.h>
#include <sqlite3.h>

#include "zz_priv.h"

struct zzdb
{
	sqlite3 *sqlite;
};

bool zzdbupdate(struct zzdb *zdb, struct zzfile *zz);
struct zzdb *zzdbopen(void);
struct zzdb *zzdbclose(struct zzdb *zdb);


// TODO move
time_t zzundatetime(const char *datetime);
const char *zzdatetime(time_t value);

#endif
