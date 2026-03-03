CC      = gcc
CFLAGS  = -Wall -Wextra
LDFLAGS = -lpthread

EXAMPLES = example
MANDIR   = /usr/local/man/man3

.PHONY: all clean install-man

all: $(EXAMPLES)

example: example.c verbose.h
	$(CC) $(CFLAGS) example.c $(LDFLAGS) -o example

clean:
	rm -f $(EXAMPLES)

install-man:
	install -d $(MANDIR)
	install -m 644 verbose.3 $(MANDIR)/verbose.3
