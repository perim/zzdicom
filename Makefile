CFLAGS = -Wall -Wextra -DPOSIX -Werror -Wshadow -Wformat-security
COMMON = zz.o
COMMONSQL = zzsql.o
PROGRAMS = zzanon zzdump zzverify zzgroupfix zzread zzstudies zzprune
HEADERS = zz.h zz_priv.h zzsql.h

all: CFLAGS += -Os
all: sqlinit.h $(PROGRAMS)

debug: CFLAGS += -O0 -g -DDEBUG
debug: sqlinit.h $(PROGRAMS)

%.o : %.c
	$(COMPILE.c) -o $@ $<

sqlinit.h: SQL
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

zzanon: $(HEADERS) $(COMMON) zzanon.c
	gcc -o zzanon zzanon.c $(COMMON) $(CFLAGS)

zzdump: $(HEADERS) $(COMMON) zzdump.c
	gcc -o zzdump zzdump.c $(COMMON) $(CFLAGS)

zzverify: $(HEADERS) $(COMMON) zzverify.c
	gcc -o zzverify zzverify.c $(COMMON) $(CFLAGS)

zzgroupfix: $(HEADERS) $(COMMON) zzgroupfix.c
	gcc -o zzgroupfix zzgroupfix.c $(COMMON) $(CFLAGS)

zzread: $(HEADERS) $(COMMON) $(COMMONSQL) zzread.c
	gcc -o zzread zzread.c $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzstudies: $(HEADERS) $(COMMON) $(COMMONSQL) zzstudies.c
	gcc -o zzstudies zzstudies.c $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

zzprune: $(HEADERS) $(COMMON) $(COMMONSQL) zzprune.c
	gcc -o zzprune zzprune.c $(COMMON) $(COMMONSQL) $(CFLAGS) -lsqlite3

clean:
	rm -f *.o sqlinit.h $(PROGRAMS)

check:

install:
	install -t /usr/local/bin $(PROGRAMS)
