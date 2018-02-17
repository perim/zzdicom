#include "zzsql.h"

#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <pwd.h>

#include "sqlinit.h"	// creates sqlinit global

static char rbuf[4096];

bool zzquery(struct zzdb *zdb, const char *statement, int (*callback)(void*,int,char**,char**), void *cbdata)
{
	int rc;
	char *zErrMsg = NULL;

	rc = sqlite3_exec(zdb->sqlite, statement, callback, cbdata, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		if (zErrMsg)
		{
			fprintf(stderr, "SQL error from %s: %s\n", statement, zErrMsg);
			sqlite3_free(zErrMsg);
		}
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

// return whether we did update local db
bool zzdbupdate(struct zzdb *zdb, struct zzfile *zz)
{
	char *modified = NULL;
	uint16_t group, element;
	char studyInstanceUid[MAX_LEN_UI];
	char seriesInstanceUid[MAX_LEN_UI];
	char patientsName[MAX_LEN_PN];
	char modality[MAX_LEN_CS];
	long len;
	bool done = false;

	memset(studyInstanceUid, 0, sizeof(studyInstanceUid));
	memset(seriesInstanceUid, 0, sizeof(seriesInstanceUid));
	memset(patientsName, 0, sizeof(patientsName));
	memset(modality, 0, sizeof(modality));

	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len) && !done)
	{
		switch (ZZ_KEY(group, element))
		{
		case DCM_StudyInstanceUID:
			zzgetstring(zz, studyInstanceUid, sizeof(studyInstanceUid));
			break;
		case DCM_SeriesInstanceUID:
			zzgetstring(zz,	seriesInstanceUid, sizeof(seriesInstanceUid));
			break;
		case DCM_PatientsName:
			zzgetstring(zz,	patientsName, sizeof(patientsName));
			break;
		case DCM_Modality:
			zzgetstring(zz,	modality, sizeof(modality));
			break;
		case DCM_PixelData:
			done = true;
			break;
		default:
			break;
		}
	}

	// Check if date on file is newer, if so, skip the update and return false
	struct zzdbiter zq = zzdbquery(zdb, "SELECT lastmodified FROM instances WHERE filename=@s", zz->fullPath);
	if (zzdbnext(zdb, &zq, "@s", &modified))
	{
		//printf("MODIFIED: %s (%d) vs %d\n", modified, (int)zz->modifiedTime, (int)zzundatetime(modified));
		if (modified[0] != '\0' && zz->modifiedTime <= zzundatetime(modified))
		{
			printf("%s is unchanged (%d <= %d)\n", zz->fullPath, (int)zz->modifiedTime, (int)zzundatetime(modified));
			zzdbdone(zq);
			return false;
		}
	}
	zzdbdone(zq);

	zzdbdone(zzdbquery(zdb, "BEGIN TRANSACTION"));
	zzdbdone(zzdbquery(zdb, "INSERT OR REPLACE INTO instances(filename, sopclassuid, instanceuid, size, "
	         "lastmodified, seriesuid) values (@s1, @s2, @s3, @d, @s4, @s5)", zz->fullPath,
	         zz->sopClassUid, zz->sopInstanceUid, zz->fileSize, zzdatetime(zz->modifiedTime), seriesInstanceUid));
	zzdbdone(zzdbquery(zdb, "INSERT OR REPLACE INTO series(seriesuid, modality, studyuid) values (@s6, @s7, @s8)",
	         seriesInstanceUid, modality, studyInstanceUid));
	zzdbdone(zzdbquery(zdb, "INSERT OR REPLACE INTO studies(studyuid, patientsname) values (@s9, @s10)",
	         studyInstanceUid, patientsName));
	zzdbdone(zzdbquery(zdb, "COMMIT"));
	return true;
}

struct zzdb *zzdbclose(struct zzdb *zdb)
{
	sqlite3_close(zdb->sqlite);
	return NULL;
}

static const char *user_home()
{
	const char *home = getenv("HOME");
	if (home)
	{
		return home;
	}
	struct passwd *pw = getpwuid(getuid());
	return pw->pw_dir;
}

struct zzdb *zzdbopen(struct zzdb *zdb)
{
	sqlite3 *db;
	char dbname[PATH_MAX];
	int rc;
	bool exists;
	char *zErrMsg = NULL;
	struct stat buf;

	memset(dbname, 0, sizeof(dbname));
	strncpy(dbname, user_home(), sizeof(dbname) - 1);
	strncat(dbname, "/.zzdb", sizeof(dbname) - 1);

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

static void mydbrow(struct zzdbiter *zq, char const *fmt, va_list arg)
{
	int index = 0;
	int64_t *int_temp = 0;
	double *float_temp;
	char *string_temp;
	const unsigned char **strref_temp;
	char ch;
	int result;
	const void **ptr_temp;

	while ((ch = *fmt++))
	{
		if ('@' == ch)
		{
			switch (ch = *fmt++)
			{
			case 'f':
				float_temp = va_arg(arg, double *);
				*float_temp = sqlite3_column_double(zq->stmt, index++);
				//printf("GET PARAM(%d)%%f=%f\n", index - 1, *float_temp);
				break;
			case 's':
				strref_temp = va_arg(arg, const unsigned char **);
				*strref_temp = sqlite3_column_text(zq->stmt, index++);
				//printf("GET PARAM(%d)%%s=%s\n", index - 1, *strref_temp);
				break;
			case 'd':
				int_temp = va_arg(arg, int64_t *);
				*int_temp = sqlite3_column_int(zq->stmt, index++);
				//printf("GET PARAM(%d)%%d=%ld\n", index - 1, (long)*int_temp);
				break;
			case 'l':
				assert(index > 0);
				if (index <= 0) break; // oops
				int_temp = va_arg(arg, int64_t *);
				*int_temp = sqlite3_column_bytes(zq->stmt, index - 1);
				break;
			case 'p':
				ptr_temp = va_arg(arg, const void **);
				*ptr_temp = sqlite3_column_blob(zq->stmt, index++);
				//printf("GET PARAM(%d)%%p=%p\n", index - 1, *ptr_temp);
				break;
			}
		}
	}
}

static void mydbquery(struct zzdb *zdb, struct zzdbiter *zq, char const *fmt, va_list arg)
{
	int index = 1;
	int64_t int_temp = 0;
	double float_temp;
	char *string_temp;
	char ch;
	int result;
	void *ptr_temp;

	while ((ch = *fmt++))
	{
		if ('@' == ch)
		{
			result = SQLITE_OK;
			switch (ch = *fmt++)
			{
			case 'f':
				float_temp = va_arg(arg, double);
				//printf("PARAM(%d)%%f=%f\n", index, float_temp);
				result = sqlite3_bind_double(zq->stmt, index, float_temp);
				index++;
				break;
			case 's':
				string_temp = va_arg(arg, char *);
				//printf("PARAM(%d)%%s=%s\n", index, string_temp);
				result = sqlite3_bind_text(zq->stmt, index, string_temp, strlen(string_temp), SQLITE_TRANSIENT);
				int_temp = 0;
				index++;
				break;
			case 'd':
				int_temp = va_arg(arg, int64_t);
				//printf("PARAM(%d)%%d=%ld\n", index, (long)int_temp);
				result = sqlite3_bind_int(zq->stmt, index, int_temp);
				index++;
				break;
			case 'l':
				int_temp = va_arg(arg, int64_t);
				//printf("PARAM(%d)%%l=%ld\n", index + 1, (long)int_temp);
				break;
			case 'p':
				ptr_temp = va_arg(arg, void *);
				//printf("PARAM(%d)%%p=%p\n", index, ptr_temp);
				result = sqlite3_bind_blob(zq->stmt, index, ptr_temp, int_temp, SQLITE_STATIC);
				index++;
				int_temp = 0;
				break;
			case 'm':
				ptr_temp = va_arg(arg, void *);
				//printf("PARAM(%d)%%m=%p\n", index, ptr_temp);
				result = sqlite3_bind_blob(zq->stmt, index, ptr_temp, int_temp, free);
				index++;
				int_temp = 0;
				break;
			}
			if (result != SQLITE_OK)
			{
				fprintf(stderr, "Bind failed: %s\n", sqlite3_errmsg(zdb->sqlite));
			}
		}
	}
}

// @d - 64bit int
// @f - 64bit double
// @s - utf8 string
// @p - binary blob, blob must not be freed before db connection is closed
// @m - malloced binary blob, ownership of memory transferred to database
// @l - length of next blob ???
// if multiple instances of each type is used in a format string, add an incremented number
// behind the letter, eg first integer is @d, second is @d2, third is @d3, and so on.
struct zzdbiter zzdbquery(struct zzdb *zdb, char const *fmt, ...)
{
	struct zzdbiter zq;
	va_list arg;
	memset(&zq, 0, sizeof(zq));
	int result = sqlite3_prepare_v2(zdb->sqlite, fmt, strlen(fmt), &zq.stmt, NULL);
	if (result != SQLITE_OK)
	{
		fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg(zdb->sqlite));
		return zq;
	}
	va_start(arg, fmt);
	mydbquery(zdb, &zq, fmt, arg);
	va_end(arg);
	zq.retval = sqlite3_step(zq.stmt);
	return zq;
}

void zzdbdone(struct zzdbiter zq)
{
	if (zq.stmt)
	{
		sqlite3_finalize(zq.stmt);
	}
}

// @l must be AFTER pointer, in this case, unlike above
// no @m
bool zzdbnext(struct zzdb *zdb, struct zzdbiter *zq, const char *fmt, ...)
{
	va_list arg;
	if (zq->index > 0)
	{
		zq->retval = sqlite3_step(zq->stmt);
	}
	if (zq->retval == SQLITE_ROW && fmt)
	{
		va_start(arg, fmt);
		mydbrow(zq, fmt, arg);
		va_end(arg);
		zq->index++;
		return true;
	}
	else
	{
		if (zq->retval != SQLITE_DONE)
		{
			fprintf(stderr, "Step failed: %s\n", sqlite3_errmsg(zdb->sqlite));
		}
		sqlite3_finalize(zq->stmt);
		zq->stmt = NULL;
		return false;
	}
}
