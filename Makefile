CFLAGS += -Wall -Wextra -DPOSIX -Wshadow -Wformat-security -Wno-unused -g -march=native -mtune=native -std=gnu99 -Wno-error=nonnull-compare
LDFLAGS += -luuid
COMMON = zz.o zzio.o zzini.o
COMMONSQL = zzsql.o
COMMONWRITE = zzwrite.o
COMMONTEXTURE = zztexture.o
COMMONVERIFY = zzverify.o
COMMONNET = zznet.o zznetwork.o
COMMONDINET = zzdinetwork.o zznetwork.o
PART6 = part6.o
PROGRAMS = zzanon zzcopy zzdump zzgroupfix zzread zzstudies zzprune zzechoscp zzechoscu zzmkrandom zzdiscp zzdiscu zzpixel
HEADERS = zz.h zz_priv.h zzsql.h zzwrite.h part6.h zztexture.h zznet.h zzio.h zzdinetwork.h zzditags.h zznetwork.h zzini.h

all: CFLAGS += -Os -fstack-protector -DNDEBUG -Werror
all: sqlinit.h $(PROGRAMS)

debug: clean
debug: CFLAGS += -O0 -DDEBUG -fstack-protector-all -Werror
debug: sqlinit.h $(PROGRAMS)

gcov: CFLAGS += -fprofile-arcs -ftest-coverage
gcov: clean sqlinit.h $(PROGRAMS) check
	gcov *.c
	lcov --directory ./ --capture --output-file lcov_tmp.info -b ./
	lcov --extract lcov_tmp.info "$(pwd)/*" --output-file lcov.info
	genhtml lcov.info -o coverage_report

%.o : %.c
	$(CC) -o $@ $< -c $(CFLAGS)

sqlinit.h: SQL
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

zzanon: zzanon.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(LDFLAGS)

zzdump: zzdump.c $(HEADERS) $(COMMON) $(PART6) $(COMMONVERIFY)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(PART6) $(COMMONVERIFY) $(LDFLAGS)

zzcopy: zzcopy.c $(HEADERS) $(COMMON) $(PART6) $(COMMONWRITE)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(PART6) $(COMMONWRITE) $(LDFLAGS) -lCharLS

zzpixel: zzpixel.c $(HEADERS) $(COMMON) $(PART6)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(PART6) $(LDFLAGS)

zzgroupfix: zzgroupfix.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(LDFLAGS)

zzread: zzread.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) $(LDFLAGS) -lsqlite3

zzstudies: zzstudies.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) $(LDFLAGS) -lsqlite3

zzprune: zzprune.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) $(LDFLAGS) -lsqlite3

zzmkrandom: zzmkrandom.c $(HEADERS) $(COMMON) $(COMMONWRITE)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(COMMONWRITE) $(LDFLAGS)

zzechoscp: zzechoscp.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONNET)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(COMMONWRITE) $(COMMONNET) $(LDFLAGS)

zzechoscu: zzechoscu.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONNET)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(COMMONWRITE) $(COMMONNET) $(LDFLAGS)

zzdiscp: zzdiscp.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONDINET) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(COMMONWRITE) $(COMMONDINET) $(COMMONSQL) $(LDFLAGS) -lsqlite3

zzdiscu: zzdiscu.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONDINET)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(COMMONWRITE) $(COMMONDINET) $(LDFLAGS)

clean:
	rm -f *.o $(PROGRAMS) *.gcno *.gcda random.dcm *.gcov gmon.out tests/initest
	rm -rf coverage_report

CPPCHECK=cppcheck -j 4 -q --enable=portability,missingInclude --std=posix
cppcheck:
	$(CPPCHECK) zz.c zzwrite.c zzdump.c zzverify.c zzmkrandom.c
	$(CPPCHECK) zzcopy.c zztexture.c zzsql.c zzio.c
	$(CPPCHECK) zzread.c zzanon.c zzstudies.c zznetwork.c
	$(CPPCHECK) zzdiscp.c zzdiscu.c zzdinetwork.c zznet.c
	$(CPPCHECK) zzpixel.c zzini.c zzechoscp.c zzechoscu.c
	$(CPPCHECK) tests/zziotest.c tests/zzwcopy.c tests/zz1.c tests/zzt.c
	$(CPPCHECK) tests/testnet.c tests/initest.c tests/sqltest.c

check: tests/zz1 tests/zzw tests/zzt tests/zziotest tests/zzwcopy tests/testnet tests/initest tests/sqltest zzmkrandom
	tests/initest
	tests/zz1 2> /dev/null
	tests/zzw
	tests/zzt samples/spine.dcm
	tests/zzt samples/spine-ls.dcm
	tests/zziotest
	./zzmkrandom --seed 5466 samples/random.dcm
	tests/zzwcopy
	./zzmkrandom --seed 54632 samples/random.dcm
	tests/zzwcopy
	tests/testnet
	./zzread samples/spine.dcm
	tests/sqltest
	./zzdump --version > /dev/null
	./zzdump --help > /dev/null
	./zzdump --usage > /dev/null
	./zzdump -v samples/confuse.dcm > /dev/null
	./zzdump -- samples/tw1.dcm > /dev/null
	cat samples/tw2.dcm | ./zzdump --stdin > /dev/null
	./zzdump samples/brokensq.dcm > /dev/null
	./zzdump samples/spine.dcm > /dev/null
	./zzdump samples/SIEMENS_GBS_III-16-ACR_NEMA_1-ULis2Bytes.dcm > /dev/null
	./zzdump samples/SIEMENS_CSA2.dcm > /dev/null
	./zzanon TEST samples/tw1.dcm
	./zzanon TEST samples/tw2.dcm
	- ./zzanon TEST samples/nonexistent.dcm 2> /dev/null
	./zzcopy samples/spine.dcm samples/copy.dcm
	./zzpixel --zero 200 200 300 300 samples/copy.dcm
	./zzcopy --rgb samples/spine.dcm samples/copy.dcm
	./zzcopy --jpegls samples/spine.dcm samples/copy.dcm
	@#./zzmkrandom --stdout --seek 1495665173 | ./zzdump --stdin > /dev/null

memcheck:
	valgrind --leak-check=yes -q tests/zzw
	valgrind --leak-check=yes -q ./tests/zziotest
	valgrind --leak-check=yes -q ./tests/sqltest
	valgrind --leak-check=yes -q ./zzanon ANON samples/tw1.dcm
	valgrind --leak-check=yes -q ./zzcopy samples/spine.dcm samples/copy.dcm
	valgrind --leak-check=yes -q ./zzpixel --zero 200 200 300 300 samples/copy.dcm
	valgrind --leak-check=yes -q ./zzdump -- samples/tw1.dcm > /dev/null
	valgrind --leak-check=yes -q ./zzdump -v samples/tw2.dcm > /dev/null

tests/gdcmdatatest: tests/gdcmdatatest.c $(HEADERS) $(COMMON) $(COMMONVERIFY)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(COMMONVERIFY)

checkall: debug cppcheck check memcheck tests/gdcmdatatest
	[ -d gdcmData ] || git clone -q git://gdcm.git.sourceforge.net/gitroot/gdcm/gdcmData
	tests/gdcmdatatest

tests/zz1: tests/zz1.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(LDFLAGS)

tests/zziotest: tests/zziotest.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(LDFLAGS)

tests/sqltest: tests/sqltest.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) -I. $(CFLAGS) $(LDFLAGS) -lsqlite3

tests/testnet: tests/testnet.c $(HEADERS) $(COMMON) $(COMMONWRITE) zznetwork.o
	$(CC) -o $@ $< $(COMMON) $(COMMONWRITE) -I. $(CFLAGS) $(LDFLAGS) zznetwork.o -lpthread

tests/zzw: tests/zzw.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONVERIFY)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(COMMONWRITE) $(COMMONVERIFY) $(LDFLAGS)

tests/zzt: tests/zzt.c $(HEADERS) $(COMMON) $(COMMONTEXTURE)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(COMMONTEXTURE) $(LDFLAGS) -lGL -lglut -lCharLS

tests/zzwcopy: tests/zzwcopy.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONVERIFY) $(PART6)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(COMMONWRITE) $(COMMONVERIFY) $(PART6) $(LDFLAGS)

tests/initest: tests/initest.c zzini.c zzini.h
	$(CC) -o $@ $< -I. zzini.c $(CFLAGS) $(LDFLAGS)

install:
	install -t /usr/local/bin $(PROGRAMS)

.PHONY: all clean install
