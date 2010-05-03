all:
	gcc -o zzanon zzanon.c zz.c -Wall -Wextra -Os -DPOSIX
	gcc -o zzdump zzdump.c zz.c -Wall -Wextra -Os -DPOSIX
	gcc -o zzverify zzverify.c zz.c -Wall -Wextra -Os -DPOSIX

clean:
	rm -f *.o zzanon zzdump zzverify
