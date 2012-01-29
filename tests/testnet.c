#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "zznetwork.h"

static void *threadfunc1(void *arg)
{
	struct zzfile szz, *zz = NULL;
	unsigned char buf[100];
	long result, i;

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
	zz = zzclose(zz);
	return NULL;
}

int main(void)
{
	struct zzfile szz, *zz;
	pthread_t thread;
	unsigned char buf[100];
	long result, i;

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
	zz = zzclose(zz);
	pthread_join(thread, NULL);

	return 0;
}
