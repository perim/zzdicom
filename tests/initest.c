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

int main(void)
{
	const char *value;
	struct zzini zzi, *ini;
	struct test_t *test;
	char buf[200];

	ini = zziniopen(&zzi, "no-such-file");
	assert(ini == NULL);

	ini = zzinibuffer(&zzi, tests[7].file, strlen(tests[7].file));
	const char *key = zzinisection(ini, NULL);
	assert(strncmp(key, "section", 6) == 0);
	ini = zziniclose(ini);

	ini = zzinibuffer(&zzi, tests[3].file, strlen(tests[3].file));
	key = zzinisection(ini, NULL);
	assert(key == NULL);
	ini = zziniclose(ini);

	ini = zzinibuffer(&zzi, tests[6].file, strlen(tests[6].file));
	key = zzinisection(ini, NULL);
	assert(key == NULL);
	ini = zziniclose(ini);

	for (test = tests; test->file != NULL; test++)
	{
		ini = zzinibuffer(&zzi, test->file, strlen(test->file));
		value = zzinivalue(ini, test->section, test->key, buf, sizeof(buf));
		assert(test->result != NULL || value == NULL);
		assert(test->result == NULL || strcmp(test->result, value) == 0);
		zziniclose(ini);
	}
	ini = zziniopen(&zzi, "samples/test.ini");
	assert(ini != NULL);
	value = zzinivalue(ini, "section", "key", buf, sizeof(buf));
	assert(strcmp("value", value) == 0);
	ini = zziniclose(ini);
	assert(ini == NULL);
	return 0;
}
