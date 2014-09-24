
CFLAGS = -O3 -Wall
IFLAGS =
LFLAGS = -Wl,-framework -Wl,IOKit -Wl,-framework -Wl,CoreFoundation -Wl,-prebind

all:
	gcc $(CFLAGS) -o mcp2200-forwarder $(IFLAGS) mcp2200-forwarder.c $(LFLAGS)
	strip mcp2200-forwarder

clean:
	rm -f mcp2200-forwarder
	rm -rf *.dSYM
