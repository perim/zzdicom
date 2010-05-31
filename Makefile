CFLAGS = -Wall -Wextra -DPOSIX -Werror -Wshadow -Wformat-security
PROGRAMS = zzanon zzdump zzverify zzgroupfix zzread zzstudies
HEADERS = zz.h zz_priv.h zzsql.h

all: CFLAGS += -Os
all: generate $(PROGRAMS)

debug: CFLAGS += -O0 -g -DDEBUG
debug: generate $(PROGRAMS)

generate:
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

zzanon: $(HEADERS)
	gcc -o zzanon zzanon.c zz.c $(CFLAGS)

zzdump: $(HEADERS)
	gcc -o zzdump zzdump.c zz.c $(CFLAGS)

zzverify: $(HEADERS)
	gcc -o zzverify zzverify.c zz.c $(CFLAGS)

zzgroupfix: $(HEADERS)
	gcc -o zzgroupfix zzgroupfix.c zz.c $(CFLAGS)

zzread: $(HEADERS)
	gcc -o zzread zzread.c zz.c zzsql.c $(CFLAGS) -lsqlite3

zzstudies: $(HEADERS)
	gcc -o zzstudies zzstudies.c zz.c zzsql.c $(CFLAGS) -lsqlite3

#zzprune: $(HEADERS)
#	gcc -o zzprune zzprune.c zz.c zzsql.c $(CFLAGS) -lsqlite3

clean:
	rm -f *.o sqlinit.h $(PROGRAMS)

check:
