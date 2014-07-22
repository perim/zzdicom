// quick and dirty read-only ini parser

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "zzini.h"

struct zzini *zziniopen(struct zzini *zi, const char *filename)
{
	struct stat st;

	memset(zi, 0, sizeof(*zi));
	if (stat(filename, &st) != 0)
	{
		//fprintf(stderr, "%s could not be found: %s\n", filename, strerror(errno));
		return NULL;
	}
	zi->size = st.st_size;
	zi->fp = fopen(filename, "r");
	zi->addr = mmap(NULL, zi->size, PROT_READ, MAP_SHARED, fileno(zi->fp), 0);
	if (zi->addr == MAP_FAILED)
	{
		fprintf(stderr, "%s - could not mmap file: %s\n", filename, strerror(errno));
		fclose(zi->fp);
		return NULL;
	}
	if (madvise((void *)zi->addr, zi->size, MADV_RANDOM | MADV_WILLNEED) != 0)
	{
		fprintf(stderr, "madvise failed: %s\n", strerror(errno));
	}
	return zi;
}

const char *zzinisection(struct zzini *zi, const char *previous)
{
	const char *ptr = previous ? previous++ : zi->addr;
	const char *endptr = zi->addr + zi->size;
	while (ptr < endptr && *ptr != '[') // scan for next section, line starting with '['
	{
		while (ptr < endptr && *ptr++ != '\n') ; // find next line
	}
	return ptr < endptr ? ++ptr : NULL;
}

const char *zzinivalue(struct zzini *zi, const char *section, const char *key, char *buffer, long bufsize)
{
	const char *ptr = zi->addr, *endptr = zi->addr + zi->size, *spanptr;
	long secsize, keysize = strlen(key);

	if (section && zi->cursection && strcmp(section, zi->cursection) == 0)
	{
		ptr = zi->cursection;	// we are already in the correct section
	}
	else if (section) // sections are optional, but recommended
	{
		secsize = strlen(section);
		// scan for right section, line starting with '[' and section key
		while (ptr + secsize < endptr
		       && !(*ptr == '[' && *(ptr + secsize + 1) == ']' && strncmp(ptr + 1, section, secsize) == 0))
		{
			while (ptr < endptr && *ptr++ != '\n') ;
		}
		ptr += secsize + 2; // skip section
	}
	// scan for the right key
	while (ptr + keysize < endptr
	       && !((*(ptr + keysize) == ' ' || *(ptr + keysize) == '=') && strncmp(ptr, key, keysize) == 0))
	{
		while (ptr < endptr && *ptr++ != '\n') ;
	}
	ptr += keysize; // skip key
	while (ptr < endptr && *ptr++ != '=') ; // skip past = sign
	while (ptr < endptr && *ptr == ' ') ptr++; // skip any leading spaces
	if (buffer && ptr < endptr)
	{
		memset(buffer, 0, bufsize);
		for (spanptr = ptr; *spanptr != '\n' && spanptr != endptr; spanptr++) ;
		strncpy(buffer, ptr, spanptr - ptr);
		return buffer;
	}
	return NULL;
}

struct zzini *zziniclose(struct zzini *zi)
{
	if (zi->fp)
	{
		munmap((void *)zi->addr, zi->size);
		fclose(zi->fp);
	}
	return NULL;
}

struct zzini *zzinibuffer(struct zzini *zi, const char *buffer, int size)
{
	memset(zi, 0, sizeof(*zi));
	zi->size = size;
	zi->fp = NULL;
	zi->addr = buffer;
	zi->cursection = NULL;
	return zi;
}
