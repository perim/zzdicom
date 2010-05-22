#ifndef ZZSQL_H
#define ZZSQL_H

struct zzdb;

struct zzfile *zzdbupdate(struct zzdb *zdb, struct zzfile *zz);
struct zzdb *zzdbopen(void);
struct zzdb *zzdbclose(struct zzdb *zdb);

#endif
