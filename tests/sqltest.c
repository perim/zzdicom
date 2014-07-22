#include <sqlite3.h>

#include "../zz_priv.h"
#include "../zzsql.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

int main(void)
{
	struct zzdb szdb, *zdb;
	const char *studyuid, *patientsname;
	const char *whichstudy = "1.2.752.24.5.838177120.2009070700740.5327542";
	int64_t len = 0;
	int count = 0;

	zdb = zzdbopen(&szdb);
	assert(zdb);
	struct zzdbiter iter = zzdbquery(zdb, "SELECT studyuid,patientsname FROM studies WHERE studyuid=\"FAIL_ME\"");
	assert(zzdbnext(zdb, &iter, NULL) == false);
	iter = zzdbquery(zdb, "SELECT studyuid,patientsname FROM studies WHERE studyuid=@s", whichstudy);
	while (zzdbnext(zdb, &iter, "@s @l @s", &studyuid, &len, &patientsname))
	{
		printf("%s | %s (%ld, %ld)\n", studyuid, patientsname, (long)len, (long)strlen(whichstudy));
		assert(len == (long)strlen(whichstudy));
		count++;
	}
	assert(count == 1);
	zdb = zzdbclose(zdb);
	assert(!zdb);

	return 0;
}
