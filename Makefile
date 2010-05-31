CFLAGS = -Wall -Wextra -DPOSIX -Werror -Wshadow -Wformat-security
PROGRAMS = zzanon zzdump zzverify zzgroupfix zzread zzstudies
HEADERS = zz.h zz_priv.h

all: CFLAGS += -Os
all: generate $(PROGRAMS)

debug: CFLAGS += -O0 -g -DDEBUG
debug: generate $(PROGRAMS)

generate:
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

zzanon:
	gcc -o zzanon zzanon.c zz.c $(CFLAGS) $(HEADERS)

zzdump:
	gcc -o zzdump zzdump.c zz.c $(CFLAGS) $(HEADERS)

zzverify:
	gcc -o zzverify zzverify.c zz.c $(CFLAGS) $(HEADERS)

zzgroupfix:
	gcc -o zzgroupfix zzgroupfix.c zz.c $(CFLAGS) $(HEADERS)

zzread:
	gcc -o zzread zzread.c zz.c zzsql.c $(CFLAGS) $(HEADERS) -lsqlite3

zzstudies:
	gcc -o zzstudies zzstudies.c zz.c zzsql.c $(CFLAGS) $(HEADERS) -lsqlite3

#zzprune:
#	gcc -o zzprune zzprune.c zz.c zzsql.c $(CFLAGS) $(HEADERS) -lsqlite3

clean:
	rm -f *.o sqlinit.h $(PROGRAMS)

check:
