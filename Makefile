CFLAGS = -Wall -Wextra -DPOSIX -Wshadow -Wformat-security -Wno-unused -mtune=native -march=native
COMMON = zz.o zzio.o
COMMONSQL = zzsql.o
COMMONWRITE = zzwrite.o
COMMONTEXTURE = zztexture.o
COMMONVERIFY = zzverify.o
COMMONNET = zznet.o
PART6 = part6.o
# zztojpegls zzmkrandom
PROGRAMS = zzanon zzdump zzgroupfix zzread zzstudies zzprune zzechoscp
HEADERS = zz.h zz_priv.h zzsql.h zzwrite.h part6.h zztexture.h zznet.h zzio.h

all: CFLAGS += -Os -fstack-protector
all: sqlinit.h $(PROGRAMS)

debug: clean
debug: CFLAGS += -O0 -g -DDEBUG -fprofile-arcs -ftest-coverage -fstack-protector-all
debug: sqlinit.h $(PROGRAMS) check

%.o : %.c
	$(CC) -o $@ $< -c $(CFLAGS)

sqlinit.h: SQL
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

zzanon: zzanon.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS)

zzdump: zzdump.c $(HEADERS) $(COMMON) $(PART6) $(COMMONVERIFY)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(PART6) $(COMMONVERIFY)

zzgroupfix: zzgroupfix.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS)

zzread: zzread.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzstudies: zzstudies.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzprune: zzprune.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zztojpegls: zztojpegls.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(PART6)
	$(CC) -o $@ $< $(COMMON) $(COMMONWRITE) $(CFLAGS) -lCharLS $(PART6)

zzmkrandom: zzmkrandom.c $(HEADERS) $(COMMON) $(COMMONWRITE)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(COMMONWRITE)

zzechoscp: zzechoscp.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONNET)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) $(COMMONWRITE) $(COMMONNET)

clean:
	rm -f *.o sqlinit.h $(PROGRAMS) *.gcno *.gcda random.dcm *.gcov

check: tests/zz1 tests/zzw tests/zzt tests/zziotest
	tests/zz1 2> /dev/null
	tests/zzw
	tests/zzt samples/spine.dcm
	tests/zzt samples/spine-ls.dcm
	tests/zziotest
	./zzdump --version > /dev/null
	./zzdump --help > /dev/null
	./zzdump --usage > /dev/null
	./zzdump -v samples/confuse.dcm > /dev/null
	./zzdump -- samples/tw1.dcm > /dev/null
	./zzdump samples/tw2.dcm > /dev/null
	./zzdump samples/brokensq.dcm > /dev/null
	./zzdump samples/spine.dcm > /dev/null
	./zzanon TEST samples/tw1.dcm
	./zzanon TEST samples/tw2.dcm

tests/zz1: tests/zz1.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS)

tests/zziotest: tests/zziotest.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS)

tests/zzw: tests/zzw.c $(HEADERS) $(COMMON) $(COMMONWRITE) $(COMMONVERIFY)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(COMMONWRITE) $(COMMONVERIFY)

tests/zzt: tests/zzt.c $(HEADERS) $(COMMON) $(COMMONTEXTURE)
	$(CC) -o $@ $< $(COMMON) -I. $(CFLAGS) $(COMMONTEXTURE) -lglut -lCharLS

install:
	install -t /usr/local/bin $(PROGRAMS)

.PHONY: all clean install
