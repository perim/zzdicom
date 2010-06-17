#include "zzsql.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "sqlinit.h"	// creates sqlinit global

static char rbuf[4096];

bool zzquery(struct zzdb *zdb, const char *statement, int (*callback)(void*,int,char**,char**), void *cbdata)
{
	int rc;
	char *zErrMsg = NULL;

	rc = sqlite3_exec(zdb->sqlite, statement, callback, cbdata, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error from %s: %s\n", statement, zErrMsg);
		sqlite3_free(zErrMsg);
		exit(-1);	// for now...
		return false;
	}
	return true;
}

const char *zzdatetime(time_t value)
{
	static char lastmod[MAX_LEN_DATETIME];
	struct tm lasttm;

	strftime(lastmod, sizeof(lastmod), DATETIME_FORMAT, localtime_r(&value, &lasttm));
	return lastmod;
}

time_t zzundatetime(const char *datetime)
{
	struct tm tmval;

	strptime(datetime, DATETIME_FORMAT, &tmval);
	return mktime(&tmval);
}

static int callback(void *nada, int cols, char **data, char **colnames)
{
	char *modified = (char *)nada;

	(void)nada;	// ignore
	(void)cols;
	(void)colnames;
	strcpy(modified, data[0]);
	return 0;
}

// return whether we did update local db
bool zzdbupdate(struct zzdb *zdb, struct zzfile *zz)
{
	char modified[MAX_LEN_DATETIME];
	uint16_t group, element;
	uint32_t len;
	char studyInstanceUid[MAX_LEN_UID];
	char patientsName[MAX_LEN_PN];
	char modality[MAX_LEN_CS];
	uint64_t pos;

	fseek(zz->fp, zz->startPos, SEEK_SET);

	modified[0] = '\0';
	memset(studyInstanceUid, 0, sizeof(studyInstanceUid));
	memset(patientsName, 0, sizeof(patientsName));
	memset(modality, 0, sizeof(modality));

	while (zzread(zz, &group, &element, &len))
	{
		// Abort early, skip loading pixel data into memory if possible
		if ((uint32_t)ftell(zz->fp) + len == zz->fileSize)
		{
			break;
		}

		pos = ftell(zz->fp);
		switch (ZZ_KEY(group, element))
		{
		case DCM_StudyInstanceUID:
			fread(studyInstanceUid, 1, MIN(sizeof(studyInstanceUid) - 1, len), zz->fp);
			break;
		case DCM_SeriesInstanceUID:
			fread(zz->seriesInstanceUid, 1, MIN(sizeof(zz->seriesInstanceUid) - 1, len), zz->fp);
			break;
		case DCM_PatientsName:
			fread(patientsName, 1, MIN(sizeof(patientsName) - 1, len), zz->fp);
			break;
		case DCM_Modality:
			fread(modality, 1, MIN(sizeof(modality) - 1, len), zz->fp);
			break;
		default:
			break;
		}
		// Skip ahead - also if read something, because read might be truncated
		if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0)
		{
			fseek(zz->fp, pos + len, SEEK_SET);
		}
	}

	// Check if date on file is newer, if so, skip the update and return false
	sprintf(rbuf, "SELECT lastmodified FROM instances WHERE filename=\"%s\"", zz->fullPath);
	zzquery(zdb, rbuf, callback, modified);
	if (modified[0] != '\0' && zz->modifiedTime <= zzundatetime(modified))
	{
		printf("%s is unchanged (%d <= %d)\n", zz->fullPath, (int)zz->modifiedTime, (int)zzundatetime(modified));
		return false;
	}

	sprintf(rbuf, "INSERT OR REPLACE INTO instances(filename, sopclassuid, instanceuid, size, lastmodified, seriesuid) values (\"%s\", \"%s\", \"%s\", \"%lu\", \"%s\", \"%s\")",
		zz->fullPath, zz->sopClassUid, zz->sopInstanceUid, zz->fileSize, zzdatetime(zz->modifiedTime), zz->seriesInstanceUid);
	zzquery(zdb, rbuf, NULL, NULL);
	sprintf(rbuf, "INSERT OR REPLACE INTO series(seriesuid, modality, studyuid) values (\"%s\", \"%s\", \"%s\")", zz->seriesInstanceUid, modality, studyInstanceUid);
	zzquery(zdb, rbuf, NULL, NULL);
	sprintf(rbuf, "INSERT OR REPLACE INTO studies(studyuid, patientsname) values (\"%s\", \"%s\")", studyInstanceUid, patientsName);
	zzquery(zdb, rbuf, NULL, NULL);
	return true;
}

struct zzdb *zzdbclose(struct zzdb *zdb)
{
	sqlite3_close(zdb->sqlite);
	free(zdb);
	return NULL;
}

struct zzdb *zzdbopen()
{
	struct zzdb *zdb = malloc(sizeof(*zdb));
	sqlite3 *db;
	const char *dbname = "/home/per/.zzdb";
	int rc;
	bool exists;
	char *zErrMsg = NULL;
	struct stat buf;

	// Check if exists
	exists = (stat(dbname, &buf) == 0);

	rc = sqlite3_open(dbname, &db);
	if (!db || rc != SQLITE_OK)
	{
		fprintf(stderr, "Failed to open db %s: %s\n", dbname, sqlite3_errmsg(db));
		exit(-2);
	}
	if (!exists)	// create local dicom database
	{
		rc = sqlite3_exec(db, sqlinit, NULL, NULL, &zErrMsg);
		if (rc != SQLITE_OK)
		{
			fprintf(stderr, "SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			exit(-3);
		}
	}
	zdb->sqlite = db;
	return zdb;
}
