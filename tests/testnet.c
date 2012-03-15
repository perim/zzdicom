#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "zznetwork.h"
#include <sys/stat.h>

static int filesize = -1;
static const char *filename = "samples/spine.dcm";
static const char *resultname = "samples/dump.dcm";

static void *threadfunc1(void *arg)
{
	struct zzfile szz, *zz = NULL;
	struct zzio *io = NULL;
	unsigned char buf[100];
	long result, i;

	(void)arg;

	memset(buf, 0xfd, sizeof(buf));
	while (!zz)
	{
		usleep(100);
		zz = zznetconnect(NULL, "localhost", 5106, &szz, 0);
	}
	result = ziwrite(zz->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	ziflush(zz->zi);
	result = ziread(zz->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	for (i = 0; i < result; i++)
	{
		assert(buf[i] == 0x10);
	}

	// Now copy over an entire file
	io = ziopenfile(filename, "r");
	assert(io);
	assert(filesize > 0);
	zicopy(zz->zi, io, filesize);
	io = ziclose(io);
	ziflush(zz->zi);

	// Now do it again, for zzreadbuf test
	io = ziopenfile(filename, "r");
	assert(io);
	assert(filesize > 0);
	zicopy(zz->zi, io, filesize);
	io = ziclose(io);
	ziflush(zz->zi);

	zz = zzclose(zz);
	return NULL;
}

int main(void)
{
	struct stat st;
	struct zzfile szz, *zz;
	pthread_t thread;
	unsigned char buf[100], buf2[100];
	long result, i;
	struct zzio *io = NULL;
	FILE *fp1, *fp2;
	char *ptr;

	stat(filename, &st);
	filesize = st.st_size;

	// test permission denied
	zz = zznetlisten(NULL, 1, &szz, ZZNET_NO_FORK);
	assert(zz == NULL);

	// just connect and exchange some data
	result = pthread_create(&thread, NULL, threadfunc1, NULL);
	assert(result == 0);
	zz = zznetlisten(NULL, 5106, &szz, ZZNET_NO_FORK);
	assert(zz);
	result = ziread(zz->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	for (i = 0; i < result; i++)
	{
		assert(buf[i] == 0xfd);
	}
	memset(buf, 0x10, sizeof(buf));
	result = ziwrite(zz->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	ziflush(zz->zi);

	// Receive big file, dump to big file
	io = ziopenfile(resultname, "w");
	assert(io);	
	zicopy(io, zz->zi, filesize);
	io = ziclose(io);

	// Compare results
	fp1 = fopen(filename, "r");
	fp2 = fopen(resultname, "r");
	while (!feof(fp1))
	{
		result = fread(buf, 1, sizeof(buf), fp1);
		i = fread(buf2, 1, sizeof(buf2), fp2);
		assert(result == i);
		assert(memcmp(buf, buf2, result) == 0);
	}
	fclose(fp1);
	fclose(fp2);

	// Now receive big file, dump it to buffer
	ptr = zireadbuf(zz->zi, filesize);
	// Compare results
	fp1 = fopen(filename, "r");
	i = 0;
	while (!feof(fp1))
	{
		result = fread(buf, 1, sizeof(buf), fp1);
		assert(memcmp(buf, ptr + i, result) == 0);
		i += result;
	}
	fclose(fp1);
	zifreebuf(zz->zi, ptr, filesize);

	// All done!
	zz = zzclose(zz);
	pthread_join(thread, NULL);

	return 0;
}
