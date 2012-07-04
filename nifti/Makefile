CFLAGS = -Wall -DPOSIX -Wshadow -Wformat-security -DHAVE_ZLIB -DHAVE_FDOPEN -I. -luuid
COMMON = ../zz.o ../part6.o ../zzwrite.o znzlib.o nifti1_io.o ../zzio.o
PROGRAMS = zznifti2dcm zzniftistitch zzdcm2nifti
HEADERS = ../zz.h ../zz_priv.h ../zzwrite.h ../part6.h nifti1.h znzlib.h nifti1_io.h

all: CFLAGS += -Os -g
all: zznifti2dcm zzniftistitch zzdcm2nifti

debug: CFLAGS += -O0 -g -DDEBUG -fprofile-arcs -ftest-coverage
debug: zznifti2dcm zzniftistitch zzdcm2nifti

%.o : %.c
	$(CC) -o $@ $< -c $(CFLAGS)

zznifti2dcm: zznifti2dcm.c $(HEADERS) $(COMMON)
	$(CC) -I.. -o $@ $< $(COMMON) $(CFLAGS) -lCharLS $(PART6) -lz -lm

zzniftistitch: zzniftistitch.c $(HEADERS) $(COMMON)
	$(CC) -I.. -o $@ $< $(COMMON) $(CFLAGS) -lCharLS $(PART6) -lz -lm

zzdcm2nifti: zzdcm2nifti.c $(HEADERS) $(COMMON)
	$(CC) -I.. -o $@ $< $(COMMON) $(CFLAGS) -lCharLS $(PART6) -lz -lm

clean:
	rm -f *.o sqlinit.h $(PROGRAMS) *.gcno *.gcda random.dcm *.dcm

check: zznifti2dcm zzniftistitch zzdcm2nifti
	./zznifti2dcm ../samples/minimal.nii ../samples/minimal-nii.dcm
	./zznifti2dcm ../samples/minimal.hdr ../samples/minimal.img ../samples/minimal-hdr.dcm
	./zzdcm2nifti ../samples/spine.dcm ../samples/spine.nii
	[ ! -f ~/bin/nifti_tool ] || ~/bin/nifti_tool -check_hdr -infiles ../samples/spine.nii
	[ ! -f ~/bin/nifti_tool ] || ~/bin/nifti_tool -check_nim -infiles ../samples/spine.nii
	./zznifti2dcm ../samples/spine.nii ../samples/spine-converted.dcm
	./zzniftistitch ../samples/spine.dcm ../samples/spine.nii ../samples/spine-stitched.dcm

install:
	install -t /usr/local/bin $(PROGRAMS)
