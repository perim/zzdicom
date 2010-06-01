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

struct node
{
	char seriesuid[MAX_LEN_UID];
	char filename[PATH_MAX];
	struct node *next;
};
static struct node *first = NULL;
static char rbuf[4096];

static int callback(void *cbdata, int cols, char **data, char **colnames)
{
	char *modified = (char *)cbdata;

	(void)cols;
	(void)colnames;
	strcpy(modified, data[0]);
	return 0;
}


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

static int callback_instances(void *nada, int cols, char **data, char **colnames)
{
	struct node *node;

	(void)nada;	// ignore
	(void)cols;
	(void)colnames;

	// Queue up filenames in a linked list for later processing
	node = malloc(sizeof(*node));
	if (!first)
	{
		first = node;
	}
	else
	{
		first->next = node;
	}
	node->next = NULL;
	strcpy(node->filename, data[0]);
	strcpy(node->seriesuid, data[1]);
	return 0;
}

int main(int argc, char **argv)
{
	struct zzdb *zdb;
	struct node *iter;
	char modified[MAX_LEN_DATETIME];
	struct stat st;
	struct zzfile *zz;

	zzutil(argc, argv, 1, "");
	zdb = zzdbopen();
	if (!zdb)
	{
		fprintf(stderr, "Error opening local DICOM database - aborting.\n");
		exit(-1);
	}

	// Check each instance
	zzquery(zdb, "SELECT filename,seriesuid,lastmodified FROM instances", callback_instances, NULL);
	for (iter = first; iter; iter = iter->next)
	{
		if (stat(iter->filename, &st) < 0)
		{
			if (errno == ENOENT)
			{
				printf("%s no longer exists - pruning\n", iter->filename);
				sprintf(rbuf, "DELETE FROM instances WHERE filename=\"%s\"", iter->filename);
				zzquery(zdb, rbuf, NULL, NULL);
				continue;
			}
		}
		sprintf(rbuf, "SELECT lastmodified FROM instances WHERE filename=\"%s\"", iter->filename);
		zzquery(zdb, rbuf, callback, modified);
		if (st.st_mtime > zzundatetime(modified))
		{
			zz = zzopen(iter->filename, "r");
			zzdbupdate(zdb, zz);
			zz = zzclose(zz);
			printf("%s was updated\n", iter->filename);
		}
	}

	// Find series without instances - TODO delete them
	zzquery(zdb, "SELECT seriesuid FROM series WHERE seriesuid NOT IN (SELECT DISTINCT seriesuid FROM instances)", callback_series, zdb);

	// Find studies without series - TODO delete them
	zzquery(zdb, "SELECT studyuid FROM studies WHERE studyuid NOT IN (SELECT DISTINCT studyuid FROM series)", callback_studies, zdb);

	zdb = zzdbclose(zdb);

	return 0;
}
