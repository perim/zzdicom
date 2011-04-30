#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "zzio.h"

#define HEADER_SIZE 8
#define PACKET_STEP 71
#define STEPS 20
#define FILENAME "/tmp/test.bin"

static void header_write_func(long size, char *buffer, const void *userdata)
{
	long *header = (long *)buffer;
	*header = size;
}

static long header_read_func(char *buffer, void *userdata)
{
	long *header = (long *)buffer;
	return *header;
}

int main(void)
{
	struct zzio *zi;
	uint32_t value = 0;
	int i, c;
	FILE *fp;
	int packetsize = 128;
	long size = 0;

	// Write
	zi = ziopenwrite(FILENAME, packetsize, 0);
	assert(zi);
	zisplitter(zi, HEADER_SIZE, header_write_func, header_read_func, NULL);
	for (c = 0; c < STEPS; c++)
	{
		for (i = 0; i < c * PACKET_STEP; i++)
		{
			ziwrite(zi, &value, 1);
		}
		ziflush(zi);
	}
	zi = ziclose(zi);
	assert(zi == NULL);

	// Verify packets
	fp = fopen(FILENAME, "r");
	assert(fp);
	while (!feof(fp))
	{
		i = fread(&size, 8, 1, fp);
		assert(i == 1 || feof(fp));
		for (i = 0; i < size && !feof(fp); i++)
		{
			char v = fgetc(fp);
			assert(v == 0);
		}
	}
	fclose(fp);

	// Read back in
	zi = ziopenread(FILENAME, packetsize, 0);
	assert(zi);
	zisplitter(zi, HEADER_SIZE, header_write_func, header_read_func, NULL);
	ziwillneed(zi, 0, 1024);
	for (c = 0; c < STEPS; c++)
	{
		for (i = 0; i < c * PACKET_STEP; i++)
		{
			char v = zigetc(zi);
			assert(v == 0);
		}
	}
	zi = ziclose(zi);
	assert(zi == NULL);

	return 0;
}
