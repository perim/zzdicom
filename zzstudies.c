#include <sqlite3.h>

#include "zz_priv.h"
#include "zzsql.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

static int counter = 0;

static int callback(void *nada, int cols, char **data, char **colnames)
{
	char name[40];

	(void)nada;	// ignore
	(void)cols;
	(void)colnames;

	memset(name, 0, sizeof(name));
	strncpy(name, data[1], sizeof(name) - 3);
	name[sizeof(name) - 1] = '\0';	// add trailing dots if cut off
	name[sizeof(name) - 2] = '.';
	name[sizeof(name) - 3] = '.';
	name[sizeof(name) - 4] = '.';
	printf("%03d | %-42s | %s\n", counter, name, data[0]);
	counter++;
	return 0;
}

int main(int argc, char **argv)
{
	struct zzdb szdb, *zdb;
	char lines[100];

	zzutil(argc, argv, 1, "", "List all studies in local DICOM database");
	zdb = zzdbopen(&szdb);
	if (!zdb)
	{
		fprintf(stderr, "Error opening local DICOM database - aborting.\n");
		exit(-1);
	}

	// List studies
	memset(lines, '-', sizeof(lines));
	lines[sizeof(lines)-1] = '\0';
	printf("idx | %-42s | %s\n", "PATIENT NAME", "STUDY UID");
	printf("%s\n", lines);
	zzquery(zdb, "SELECT studyuid,patientsname FROM studies", callback, NULL);
	zdb = zzdbclose(zdb);

	return 0;
}
