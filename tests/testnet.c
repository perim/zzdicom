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
static const char *tracename = "samples/dump2.dcm";

static void *threadfunc1(void *arg)
{
	struct zznetwork *zzn = NULL;
	struct zzio *io = NULL;
	unsigned char buf[100];
	long result, i;

	(void)arg;

	memset(buf, 0xfd, sizeof(buf));
	while (!zzn)
	{
		usleep(100);
		zzn = zznetconnect(NULL, "localhost", 5106, 0);
	}
	result = ziwrite(zzn->out->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	ziflush(zzn->out->zi);
	result = ziread(zzn->out->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	for (i = 0; i < result; i++)
	{
		assert(buf[i] == 0x10);
	}

	// Now copy over an entire file
	io = ziopenfile(filename, "r");
	assert(io);
	assert(filesize > 0);
	result = zicopy(zzn->out->zi, io, filesize);
	assert(result == filesize);
	io = ziclose(io);
	ziflush(zzn->out->zi);

	// Now do it again, for zzreadbuf test
	io = ziopenfile(filename, "r");
	assert(io);
	assert(filesize > 0);
	result = zicopy(zzn->out->zi, io, filesize);
	assert(result == filesize);
	io = ziclose(io);
	ziflush(zzn->out->zi);

	zzn = zznetclose(zzn);
	return NULL;
}

int main(void)
{
	struct stat st;
	struct zznetwork *zzn = NULL;
	pthread_t thread;
	unsigned char buf[100], buf2[100];
	long result, i;
	struct zzio *io = NULL;
	FILE *fp1, *fp2;
	char *ptr;

	stat(filename, &st);
	filesize = st.st_size;

	// test permission denied
	zzn = zznetlisten(NULL, 1, ZZNET_NO_FORK);
	assert(zzn == NULL);

	// just connect and exchange some data
	result = pthread_create(&thread, NULL, threadfunc1, NULL);
	assert(result == 0);
	zzn = zznetlisten(NULL, 5106, ZZNET_NO_FORK);
	assert(zzn);
	zzn->trace = calloc(1, sizeof(*zzn->trace));
	zzn->trace->zi = ziopenfile(tracename, "w");
	zitee(zzn->in->zi, zzn->trace->zi, ZZIO_TEE_READ);
	result = ziread(zzn->in->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	assert(zibytesread(zzn->in->zi) == sizeof(buf));
	for (i = 0; i < result; i++)
	{
		assert(buf[i] == 0xfd);
	}
	memset(buf, 0x10, sizeof(buf));
	result = ziwrite(zzn->in->zi, buf, sizeof(buf));
	assert(result == sizeof(buf));
	assert(zibyteswritten(zzn->in->zi) == sizeof(buf));
	ziclose(zzn->trace->zi);

	// Receive big file, dump to big file
	io = ziopenfile(resultname, "w");
	zzn->trace->zi = ziopenfile(tracename, "w");
	assert(zzn->trace->zi);
	zitee(zzn->in->zi, zzn->trace->zi, ZZIO_TEE_READ);
	assert(io);	
	result = zicopy(io, zzn->in->zi, filesize);
	assert(result == filesize);
	assert(zibyteswritten(io) == filesize);
	assert(zibyteswritten(zzn->trace->zi) == filesize);
	io = ziclose(io);
	zzn->trace = zzclose(zzn->trace);
	zitee(zzn->in->zi, NULL, 0);

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

	// Compare results for tracefile
	fp1 = fopen(filename, "r");
	fp2 = fopen(tracename, "r");
	while (!feof(fp1))
	{
		result = fread(buf, 1, sizeof(buf), fp1);
		i = fread(buf2, 1, sizeof(buf2), fp2);
		assert(result == i);
		assert(memcmp(buf, buf2, result) == 0);
	}
	fclose(fp1);
	fclose(fp2);

	// ---------------------------------------
	// Now receive big file, dump it to buffer
	ptr = zireadbuf(zzn->in->zi, filesize);
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
	zifreebuf(zzn->in->zi, ptr, filesize);

	// All done!
	zzn = zznetclose(zzn);
	pthread_join(thread, NULL);

	return 0;
}
