CFLAGS = -Wall -DPOSIX -Wshadow -Wformat-security -DHAVE_ZLIB -DHAVE_FDOPEN -I. -luuid
COMMON = ../zz.o ../part6.o ../zzwrite.o znzlib.o nifti1_io.o ../zzio.o
PROGRAMS = zznifti2dcm zzniftistitch
HEADERS = ../zz.h ../zz_priv.h ../zzwrite.h ../part6.h nifti1.h znzlib.h nifti1_io.h

all: CFLAGS += -Os
all: zznifti2dcm zzniftistitch

debug: CFLAGS += -O0 -g -DDEBUG -fprofile-arcs -ftest-coverage

%.o : %.c
	$(CC) -o $@ $< -c $(CFLAGS)

zznifti2dcm: zznifti2dcm.c $(HEADERS) $(COMMON)
	$(CC) -I.. -o $@ $< $(COMMON) $(CFLAGS) -lCharLS $(PART6) -lz -lm

zzniftistitch: zzniftistitch.c $(HEADERS) $(COMMON)
	$(CC) -I.. -o $@ $< $(COMMON) $(CFLAGS) -lCharLS $(PART6) -lz -lm

clean:
	rm -f *.o sqlinit.h $(PROGRAMS) *.gcno *.gcda random.dcm *.dcm

check: zznifti2dcm zzniftistitch
	./zznifti2dcm ../samples/minimal.nii
	./zznifti2dcm ../samples/minimal.hdr ../samples/minimal.img

install:
	install -t /usr/local/bin $(PROGRAMS)
