#include <sqlite3.h>

#include "zz_priv.h"
#include "zzsql.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

int main(int argc, char **argv)
{
	struct zzdb szdb, *zdb;
	char lines[100];
	const char *studyuid, *patientsname;
	int counter = 0;
	char name[40];

	zzutil(argc, argv, 0, "", "List all studies in local DICOM database", NULL);
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

	struct zzdbiter iter = zzdbquery(zdb, "SELECT studyuid,patientsname FROM studies");
	while (zzdbnext(zdb, &iter, "@s @s", &studyuid, &patientsname))
	{
		memset(name, 0, sizeof(name));
		strncpy(name, patientsname, sizeof(name) - 3);
		name[sizeof(name) - 1] = '\0';	// add trailing dots if cut off
		name[sizeof(name) - 2] = '.';
		name[sizeof(name) - 3] = '.';
		name[sizeof(name) - 4] = '.';
		printf("%03d | %-42s | %s\n", counter, name, studyuid);
		counter++;
	}

	zdb = zzdbclose(zdb);

	return 0;
}
