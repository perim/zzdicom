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
	$(CC) -o $@ $< -c $(CFLAGS)

sqlinit.h: SQL
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

zzanon: zzanon.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS)

zzdump: zzdump.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS)

zzverify: zzverify.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS)

zzgroupfix: zzgroupfix.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS)

zzread: zzread.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzstudies: zzstudies.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzprune: zzprune.c $(HEADERS) $(COMMON) $(COMMONSQL)
	$(CC) -o $@ $< $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zztojpegls: zztojpegls.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS) -lCharLS

zzmkrandom: zzmkrandom.c $(HEADERS) $(COMMON)
	$(CC) -o $@ $< $(COMMON) $(CFLAGS)

clean:
	rm -f *.o sqlinit.h $(PROGRAMS) *.gcno *.gcda random.dcm

check: tests/zz1.c
	$(CC) -o tests/zz1 tests/zz1.c -I. $(COMMON) $(CFLAGS)
	tests/zz1 2> /dev/null

install:
	install -t /usr/local/bin $(PROGRAMS)
