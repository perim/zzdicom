#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "zz_priv.h"

extern void uuid_unparse_dicom(uuid_t uuid, char *str);

static void testuuid(void)
{
	uuid_t uuid;
	char str[64 + 1];

	memset(str, 0, sizeof(str));
	uuid[0] = 0xba; // using test sequence from DCMTK
	uuid[1] = 0xa0;
	uuid[2] = 0x05;
	uuid[3] = 0x5e;
	uuid[4] = 0x20;
	uuid[5] = 0xd4;
	uuid[6] = 0x01;
	uuid[7] = 0xe1;
	uuid[8] = 0x80;
	uuid[9] = 0x4a;
	uuid[10] = 0x67;
	uuid[11] = 0xc6;
	uuid[12] = 0x69;
	uuid[13] = 0x73;
	uuid[14] = 0x51;
	uuid[15] = 0xff;
	uuid_unparse_dicom(uuid, str);
	assert(strcmp(str, "2.25.248067283583015040850042404479733813759") == 0);
}

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

	testuuid();

	return 0;
}
