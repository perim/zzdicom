#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "zzio.h"

#define HEADER_SIZE 8
#define PACKET_STEP 71
#define STEPS 20
#define FILENAME "/tmp/test.bin"

static void header_write_func(long size, char *buffer, const void *userdata)
{
	int64_t *header = (int64_t *)buffer;
	*header = size;
}

static long header_read_func(char *buffer, void *userdata)
{
	int64_t *header = (int64_t *)buffer;
	return *header;
}

static void test1(int packetsize, bool splitter)
{
	struct zzio *zi;
	int i, c, value;

	// Write
	zi = ziopenwrite(FILENAME, packetsize, 0);
	assert(zi);
	if (splitter) zisplitter(zi, HEADER_SIZE, header_write_func, header_read_func, NULL);
	value = 1;
	for (c = 0; c < STEPS; c++)
	{
		for (i = 0; i < c * PACKET_STEP; i++)
		{
			ziputc(zi, value);
			value = (value + 1) % 255;
		}
		ziflush(zi);
	}
	zi = ziclose(zi);
	assert(zi == NULL);

	// Verify packets
	value = 1;
	if (splitter)
	{
		FILE *fp = fopen(FILENAME, "r");
		assert(fp);
		while (!feof(fp))
		{
			int64_t size = 0;
			i = fread(&size, 8, 1, fp);
			assert(i == 1 || feof(fp));
			for (i = 0; i < size && !feof(fp); i++)
			{
				int v = fgetc(fp);
				assert(v == value);
				value = (value + 1) % 255;
			}
		}
		fclose(fp);
	}
	else
	{
		FILE *fp = fopen(FILENAME, "r");
		assert(fp);
		for (i = 0; i < STEPS * PACKET_STEP; i++)
		{
			int v = fgetc(fp);
			assert(v != EOF);
			assert(v == value);
			value = (value + 1) % 255;
		}
		fclose(fp);
	}

	// Read back in
	zi = ziopenread(FILENAME, packetsize, 0);
	assert(zi);
	if (splitter) zisplitter(zi, HEADER_SIZE, header_write_func, header_read_func, NULL);
	ziwillneed(zi, 0, 1024);
	value = 1;
	for (c = 0; c < STEPS; c++)
	{
		for (i = 0; i < c * PACKET_STEP; i++)
		{
			int v = zigetc(zi);
			assert(v == value);
			value = (value + 1) % 255;
		}
	}
	zi = ziclose(zi);
	assert(zi == NULL);
}

static void test2(int value, int packetsize, bool splitter)
{
	struct zzio *zi;
	int i, c;

	// Write
	zi = ziopenwrite(FILENAME, packetsize, 0);
	assert(zi);
	if (splitter) zisplitter(zi, HEADER_SIZE, header_write_func, header_read_func, NULL);
	for (c = 0; c < STEPS; c++)
	{
		for (i = 0; i < c * PACKET_STEP; i++)
		{
			ziwrite(zi, &value, 4);
		}
		ziflush(zi);
	}
	zi = ziclose(zi);
	assert(zi == NULL);

	// Verify packets
	if (splitter)
	{
		FILE *fp = fopen(FILENAME, "r");
		assert(fp);
		while (!feof(fp))
		{
			int64_t size;
			int r = 1;
			int32_t v = value;
			i = fread(&size, 8, 1, fp);
			assert(i == 1 || feof(fp));
			if (!feof(fp)) r = fread(&v, 4, 1, fp);	// check first word
			assert(r == 1);
			assert(v == value);
			for (i = 4; i < size && !feof(fp); i++) fgetc(fp); // skip rest, too hard to test
		}
		fclose(fp);
	}
	else
	{
		FILE *fp = fopen(FILENAME, "r");
		int32_t v = 0;
		assert(fp);
		for (i = 0; i < STEPS * PACKET_STEP; i++)
		{
			int r = fread(&v, 4, 1, fp);
			assert(r == 1);
			assert(v == value);
		}
		fclose(fp);
	}

	// Read back in
	zi = ziopenread(FILENAME, packetsize, 0);
	assert(zi);
	if (splitter) zisplitter(zi, HEADER_SIZE, header_write_func, header_read_func, NULL);
	ziwillneed(zi, 0, 1024);
	for (c = 0; c < STEPS; c++)
	{
		for (i = 0; i < c * PACKET_STEP; i++)
		{
			int32_t v = 0;
			int r = ziread(zi, &v, 4);
			assert(r == 4);
			assert(v == value);
		}
	}
	zi = ziclose(zi);
	assert(zi == NULL);
}

static void test3(int pos, const char *stamp, int packetsize)
{
	int i;
	struct zzio *zi;
	FILE *fp = fopen(FILENAME, "w");

	for (i = 0; i < 1024; i++) fputc('.', fp);
	fclose(fp);

	zi = ziopenmodify(FILENAME, packetsize, 0);
	zisetreadpos(zi, 128);
	assert(zireadpos(zi) == 128);
	i = zigetc(zi);
	assert(i == '.');

	zisetwritepos(zi, 128);
	i = ziwrite(zi, stamp, strlen(stamp));
	assert(i = strlen(stamp));
	ziflush(zi);	// commit changes

	zisetreadpos(zi, 128);
	for (i = 0; i < (int)strlen(stamp); i++)
	{
		int value = zigetc(zi);
		assert(value == stamp[i]);	// read it from file
	}
	zi = ziclose(zi);
}

static void test4()
{
	const char *srcfile = "samples/spine.dcm";
	const char *dstfile = "samples/copy.dcm";
	struct zzio *src = ziopenfile(srcfile, "r");
	struct zzio *dst = ziopenfile(dstfile, "w");
	FILE *cmpsrc = fopen(srcfile, "r");
	FILE *cmpdst;
	struct stat st;
	long size, result;
	char *mem, *mem2;
	int i;

	stat("samples/spine.dcm", &st);
	size = st.st_size;
	mem = malloc(size);
	mem2 = malloc(size);
	result = ziread(src, mem, size);
	assert(result == size);
	result = fread(mem2, 1, size, cmpsrc);
	assert(result == size);
	for (i = 0; i < size; i++)
	{
		if (mem[i] != mem2[i]) fprintf(stderr, "Memory read from file differs at byte %d\n", i);
		assert(mem[i] == mem2[i]);
	}
	fclose(cmpsrc);
	result = ziwrite(dst, mem, size);
	assert(result == size);
	src = ziclose(src);
	dst = ziclose(dst);
	cmpdst = fopen(dstfile, "r");
	result = fread(mem2, 1, size, cmpdst);
	assert(result == size);
	for (i = 0; i < size; i++)
	{
		if (mem[i] != mem2[i]) fprintf(stderr, "Memory writtent o file differs at byte %d\n", i);
		assert(mem[i] == mem2[i]);
	}
	fclose(cmpdst);
	free(mem);
}

int main(void)
{
	// Round # 1 - getc, putc
	test1(128, true);
	test1(1024, true);
	test1(11, true);
	test1(99, true);

	test1(128, false);
	test1(1024, false);
	test1(11, false);
	test1(99, false);

	// Round # 2 - read, write 32 bit values
	test2(4, 128, true);
	test2(0, 60, true);

	test2(4, 128, false);
	test2(0, 60, false);

	// Round # 3 - modify
	test3(128, "DICM", 32);

	// Large copy
	test4();

	return 0;
}
