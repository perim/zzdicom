#ifndef ZZSQL_H
#define ZZSQL_H

#include <sqlite3.h>

struct zzdb
{
	sqlite3 *sqlite;
};

bool zzdbupdate(struct zzdb *zdb, struct zzfile *zz);
struct zzdb *zzdbopen(void);
struct zzdb *zzdbclose(struct zzdb *zdb);

#endif
