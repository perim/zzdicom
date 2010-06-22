CFLAGS = -Wall -Wextra -DPOSIX -Werror -Wshadow -Wformat-security
COMMON = zz.o
COMMONSQL = zzsql.o
PROGRAMS = zzanon zzdump zzverify zzgroupfix zzread zzstudies zzprune zztojpegls zzmkrandom
HEADERS = zz.h zz_priv.h zzsql.h

all: CFLAGS += -Os
all: sqlinit.h $(PROGRAMS)

debug: CFLAGS += -O0 -g -DDEBUG -fprofile-arcs -ftest-coverage
debug: sqlinit.h $(PROGRAMS) check

%.o : %.c
	$(COMPILE.c) -o $@ $<

sqlinit.h: SQL
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

zzanon: $(HEADERS) $(COMMON) zzanon.c
	$(CC) -o zzanon zzanon.c $(COMMON) $(CFLAGS)

zzdump: $(HEADERS) $(COMMON) zzdump.c
	$(CC) -o zzdump zzdump.c $(COMMON) $(CFLAGS)

zzverify: $(HEADERS) $(COMMON) zzverify.c
	$(CC) -o zzverify zzverify.c $(COMMON) $(CFLAGS)

zzgroupfix: $(HEADERS) $(COMMON) zzgroupfix.c
	$(CC) -o zzgroupfix zzgroupfix.c $(COMMON) $(CFLAGS)

zzread: $(HEADERS) $(COMMON) $(COMMONSQL) zzread.c
	$(CC) -o zzread zzread.c $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzstudies: $(HEADERS) $(COMMON) $(COMMONSQL) zzstudies.c
	$(CC) -o zzstudies zzstudies.c $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzprune: $(HEADERS) $(COMMON) $(COMMONSQL) zzprune.c
	$(CC) -o zzprune zzprune.c $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zztojpegls: $(HEADERS) $(COMMON) zztojpegls.c
	$(CC) -o zztojpegls zztojpegls.c $(COMMON) $(CFLAGS) -lCharLS

zzmkrandom: zzmkrandom.c $(HEADERS) $(COMMON)
	$(CC) -o zzmkrandom zzmkrandom.c $(COMMON) $(CFLAGS)

clean:
	rm -f *.o sqlinit.h $(PROGRAMS)

check: tests/zz1.c
	$(CC) -g -O0 -o tests/zz1 tests/zz1.c -I. $(COMMON) $(CFLAGS)
	tests/zz1

install:
	install -t /usr/local/bin $(PROGRAMS)
