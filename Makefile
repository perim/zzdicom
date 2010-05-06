CFLAGS = -Wall -Wextra -Os -DPOSIX -Werror

all:
	gcc -o zzanon zzanon.c zz.c $(CFLAGS)
	gcc -o zzdump zzdump.c zz.c $(CFLAGS)
	gcc -o zzverify zzverify.c zz.c $(CFLAGS)

clean:
	rm -f *.o zzanon zzdump zzverify
