.PRECIOUS: test

CFLAGS = -std=c99 \
	-Wall \
	-Wextra \
	-Werror \
	-pedantic \
	-Wno-cpp \
	-Werror=vla \
	-Wbad-function-cast \
	-Wbuiltin-macro-redefined \
	-Wcast-align \
	-Wdate-time \
	-Wdisabled-optimization \
	-Wformat=2 \
	-Winvalid-pch \
	-Wmissing-include-dirs \
	-Wnested-externs \
	-Wpacked \
	-Wredundant-decls \
	-Wshadow \
	-Wswitch-default \
	-Wundef \
	-Wunused \
	-Wunused-macros \
	-Wconversion \
	-Wfloat-equal \
	-Wwrite-strings \
	-Wdouble-promotion \
	-fno-common \
	-Wnull-dereference \
	-Wstrict-overflow=5 \
	-Wstrict-prototypes \
	-Wcast-qual \
	-Wstack-protector \
	-Wunused-function \
	-g

test: josh.h test.c
	$(CC) $(CFLAGS) test.c -o test
	./test

clean:
	rm -rf test
