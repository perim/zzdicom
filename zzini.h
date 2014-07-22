#ifndef ZZ_INI_H
#define ZZ_INI_H

#ifdef __cplusplus
extern "C" {
#endif

// quick and dirty read-only ini parser

#include <stdbool.h>

struct zzini
{
	const char *addr;
	FILE *fp;
	const char *cursection;
	long size;
};

struct zzini *zziniopen(struct zzini *zi, const char *filename);
const char *zzinivalue(struct zzini *zi, const char *section, const char *key, char *buffer, long size);
static inline bool zzinicontains(struct zzini *zi, const char *section, const char *key) { return zzinivalue(zi, section, key, NULL, 0) != NULL; }
struct zzini *zziniclose(struct zzini *zi);

/// Iterate over sections in an INI file. Pass NULL for previous to start, then pass in the previously found section to find the next one.
/// The resulting string is ']' delimited!
const char *zzinisection(struct zzini *zi, const char *previous);

/// Open an INI file from a memory buffer
struct zzini *zzinibuffer(struct zzini *zi, const char *buffer, int size);

#ifdef __cplusplus
}
#endif

#endif
