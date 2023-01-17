CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic

test: josh.h test.c
	$(CC) $(CFLAGS) test.c -o test
	./test
