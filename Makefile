CFLAGS = -Wall -Wextra -DPOSIX -Werror -Wshadow -Wformat-security

all: CFLAGS += -Os
all: apps

debug: CFLAGS += -O0 -g -DDEBUG
debug: apps

apps:
	echo "const char *sqlinit =" > sqlinit.h
	cat SQL | sed s/^/\"/ | sed s/$$/\"/ >> sqlinit.h
	echo ";" >> sqlinit.h

	gcc -o zzanon zzanon.c zz.c $(CFLAGS)
	gcc -o zzdump zzdump.c zz.c $(CFLAGS)
	gcc -o zzverify zzverify.c zz.c $(CFLAGS)
	gcc -o zzgroupfix zzgroupfix.c zz.c $(CFLAGS)
	gcc -o zzread zzread.c zz.c zzsql.c $(CFLAGS) -lsqlite3

clean:
	rm -f *.o zzanon zzdump zzverify zzgroupfix sqlinit.h
