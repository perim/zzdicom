#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "zz_priv.h"

int main(void)
{
	bool result;
	struct zzfile szz, *zz;
	char filename[12];
	int fd, i;
	char value;
	long len;

	// Check proper handling of NULL values
	zz = zzopen(NULL, NULL, NULL);
	assert(zz == NULL);
	zzclose(NULL);
        zziterinit(NULL);
        result = zziternext(NULL, NULL, NULL, &len);
        assert(result == false);

	// Check proper handling of bad file input
	zz = zzopen("/nonexistent", "r", &szz); // canot open non-existent file
	assert(zz == NULL);
	zz = zzopen("/tmp", "r", &szz); // cannot open directory
	assert(zz == NULL);
	strcpy(filename, "/tmp/XXXXXX");
	fd = mkstemp(filename);
	fchmod(fd, 0);	// make file inaccessible
	zz = zzopen(filename, "r", &szz);
	assert(zz == NULL);
	fchmod(fd, S_IWUSR | S_IRUSR);	// make accessible
	value = 0; write(fd, &value, 1);
	value = 1; write(fd, &value, 1);
	zz = zzopen(filename, "r", &szz);
	assert(zz == NULL);	// too small
	for (i = 0; i < 128; i++) write(fd, &value, 1);	// filler
	zz = zzopen(filename, "r", &szz);
	assert(zz == NULL);	// above created file pretends to be big-endian
	close(fd);

	return 0;
}
