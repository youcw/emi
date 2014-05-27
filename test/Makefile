
CC=$(CROSS)gcc

CFLAGS=-I../include/ -g

LDFLAGS= -L../lib/ -lemi

ifneq ($(DEBUG),)
CFLAGS+=$(DEBUG)
endif

all:test sar

test:test.c
	$(CC) $(CFLAGS) -static -o $@ $< $(LDFLAGS)
#	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
sar:sar.c
	$(CC) $(CFLAGS) -static -o $@ $< $(LDFLAGS)
#	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm test simpletest sertest sar -f
