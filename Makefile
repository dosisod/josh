.PRECIOUS: test

CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic -g

test: josh.h test.c
	$(CC) $(CFLAGS) test.c -o test
	./test
