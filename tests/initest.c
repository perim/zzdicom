#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include "zzini.h"

struct test_t {
	const char *file;
	const char *section;
	const char *key;
	const char *result;
};
struct test_t tests[] = 
{
	// "file", section, key, desired result
	{ "]", NULL, "anything", NULL },
	{ "[sec", "section", "anything", NULL },
	{ "[only]", "only", "anything", NULL },
	{ "only]", "only", "anything", NULL },
	{ "[]", NULL, "anything", NULL },
	{ "other]", "this", "anything", NULL },
	{ "key = value", NULL, "key", "value" },
	{ "[section]\nkey = value\n\n\n", "section", "key", "value" },
	{ "[section]\nkey = value", NULL, "key", "value" }, // not sure about this one...
	{ "sdflkj\n[section]\nkey = value\nsdfklj", "section", "key", "value" },
	{ "[sect", "section", "key", NULL },
	{ "key = value", "section", "key", NULL },
	{ "ke", NULL, "key", NULL },
	{ "[section]\nkeyed = valued", "section", "key", NULL },
	{ NULL, NULL, NULL, NULL } // terminator
};

struct zzini *zzinimem(struct zzini *ini, const char *mem)
{
	ini->addr = (char *)mem;
	ini->fp = NULL;
	ini->cursection = NULL;
	ini->size = strlen(mem);
	return ini;
}

int main(void)
{
	const char *value;
	struct zzini zzi, *ini;
	struct test_t *test;
	char buf[200];

	ini = zziniopen(&zzi, "no-such-file");
	assert(ini == NULL);
	for (test = tests; test->file != NULL; test++)
	{
		ini = zzinimem(&zzi, test->file);
		value = zzinivalue(ini, test->section, test->key, buf, sizeof(buf));
		assert(test->result != NULL || value == NULL);
		assert(test->result == NULL || strcmp(test->result, value) == 0);
	}
	ini = zziniopen(&zzi, "samples/test.ini");
	assert(ini != NULL);
	value = zzinivalue(ini, "section", "key", buf, sizeof(buf));
	assert(strcmp("value", value) == 0);
	ini = zziniclose(ini);
	assert(ini == NULL);
	return 0;
}
