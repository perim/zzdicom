#include "zzsql.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "sqlinit.h"	// creates sqlinit global

static char rbuf[4096];

#define DATETIME_FORMAT "%Y-%m-%d %H:%M:%S"

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

// return whether we did update local db
bool zzdbupdate(struct zzdb *zdb, struct zzfile *zz)
{
	uint16_t group, element;
	uint32_t len, size;
	struct stat st;
	int rc;
	char studyInstanceUid[MAX_LEN_UID];
	char patientsName[MAX_LEN_PN];
	char modality[MAX_LEN_CS];
	char *zErrMsg = NULL;
	sqlite3 *db = zdb->sqlite;
	size_t pos;

	fseek(zz->fp, zz->startPos, SEEK_SET);
	fstat(fileno(zz->fp), &st);
	size = st.st_size;

	memset(studyInstanceUid, 0, sizeof(studyInstanceUid));
	memset(patientsName, 0, sizeof(patientsName));
	memset(modality, 0, sizeof(modality));

	while (zzread(zz, &group, &element, &len))
	{
		// Abort early, skip loading pixel data into memory if possible
		if (ftell(zz->fp) + len == size)
		{
			break;
		}

		pos = ftell(zz->fp);
		switch (ZZ_KEY(group, element))
		{
		case DCM_StudyInstanceUID:
			fread(studyInstanceUid, MIN(sizeof(studyInstanceUid) - 1, len), 1, zz->fp);
			break;
		case DCM_SeriesInstanceUID:
			fread(zz->seriesInstanceUid, MIN(sizeof(zz->seriesInstanceUid) - 1, len), 1, zz->fp);
			break;
		case DCM_PatientsName:
			fread(patientsName, MIN(sizeof(patientsName) - 1, len), 1, zz->fp);
			break;
		case DCM_Modality:
			fread(modality, MIN(sizeof(modality) - 1, len), 1, zz->fp);
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

	// TODO - check if date on file is newer, if so, skip the below and return false

	sprintf(rbuf, "INSERT OR REPLACE INTO instances(filename, sopclassuid, instanceuid, size, lastmodified, seriesuid) values (\"%s\", \"%s\", \"%s\", \"%d\", \"%s\", \"%s\")",
		zz->fullPath, zz->sopClassUid, zz->sopInstanceUid, size, zzdatetime(st.st_mtime), zz->seriesInstanceUid);
	rc = sqlite3_exec(db, rbuf, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	sprintf(rbuf, "INSERT OR REPLACE INTO series(seriesuid, modality, studyuid) values (\"%s\", \"%s\", \"%s\")", zz->seriesInstanceUid, modality, studyInstanceUid);
	rc = sqlite3_exec(db, rbuf, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	sprintf(rbuf, "INSERT OR REPLACE INTO studies(studyuid, patientsname) values (\"%s\", \"%s\")", studyInstanceUid, patientsName);
	rc = sqlite3_exec(db, rbuf, NULL, NULL, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
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
	struct stat buf;
	char *zErrMsg = NULL;

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
