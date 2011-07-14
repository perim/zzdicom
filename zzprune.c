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
#include <limits.h>
#include <errno.h>

static char rbuf[4096];

struct updatenode
{
	char path[PATH_MAX];
	struct updatenode *next;
};
static struct updatenode *first = NULL;

static int callback_studies(void *cbdata, int cols, char **data, char **colnames)
{
	struct zzdb *zdb = (struct zzdb *)cbdata;

	(void)cols;
	(void)colnames;

	printf("DELETING STUDY: %s\n", data[0]);
	sprintf(rbuf, "DELETE FROM studies WHERE studyuid=\"%s\"", data[0]);
	zzquery(zdb, rbuf, NULL, NULL);
	return 0;
}

static int callback_series(void *cbdata, int cols, char **data, char **colnames)
{
	struct zzdb *zdb = (struct zzdb *)cbdata;

	(void)cols;
	(void)colnames;

	printf("DELETING SERIES: %s\n", data[0]);
	sprintf(rbuf, "DELETE FROM series WHERE seriesuid=\"%s\"", data[0]);
	zzquery(zdb, rbuf, NULL, NULL);
	return 0;
}

static int callback_instances(void *cbdata, int cols, char **data, char **colnames)
{
	struct zzdb *zdb = (struct zzdb *)cbdata;
	struct stat st;

	(void)cols;
	(void)colnames;

	if (stat(data[0], &st) < 0)
	{
		if (errno == ENOENT)
		{
			printf("%s no longer exists - pruning\n", data[0]);
			sprintf(rbuf, "DELETE FROM instances WHERE filename=\"%s\"", data[0]);
			zzquery(zdb, rbuf, NULL, NULL);
			return 0;
		}
	}
	if (st.st_mtime > zzundatetime(data[1]))
	{
		struct updatenode *node = malloc(sizeof(*node));
		node->next = first;
		first = node;
		strcpy(node->path, data[0]);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct zzdb szdb, *zdb;
	struct updatenode *iter, *next;
	struct zzfile szz, *zz;

	zzutil(argc, argv, 1, "", "Program to update the local DICOM database for changed and deleted files", NULL);
	zdb = zzdbopen(&szdb);
	if (zdb)
	{
		// Find and delete instances without matching physical files, updated changed files
		zzquery(zdb, "SELECT filename,lastmodified FROM instances", callback_instances, zdb);

		for (iter = first; iter; iter = next)	// need it outside select loop for transactions to work
		{
			next = iter->next;
			zz = zzopen(iter->path, "r", &szz);
			if (zz)
			{
				zzdbupdate(zdb, zz);
				zz = zzclose(zz);
				printf("%s was updated\n", iter->path);
				free(iter);
			}
		}

		// Find and delete series without instances
		zzquery(zdb, "SELECT seriesuid FROM series WHERE seriesuid NOT IN (SELECT DISTINCT seriesuid FROM instances)", callback_series, zdb);

		// Find studies without series
		zzquery(zdb, "SELECT studyuid FROM studies WHERE studyuid NOT IN (SELECT DISTINCT studyuid FROM series)", callback_studies, zdb);

		zdb = zzdbclose(zdb);
	}

	return 0;
}
