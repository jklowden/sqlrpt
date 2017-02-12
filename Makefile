CC = cc -O0 -g -std=c11

CNOWARN = $(addprefix -Wno-,unused-function unused-value \
	missing-braces parentheses)

CPPFLAGS = -I/usr/local/include -D_GNU_SOURCE
CFLAGS = -Wswitch -Werror -Wall -Wpointer-arith $(CNOWARN)
LDFLAGS = -lsqlite3

sqlrpt: report.c
	$(CC) -o $@ $^  $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

TAGS: *.c
	etags $^
