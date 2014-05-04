all: alloctest

PKGS = gobject-2.0
WARNINGS = -Wall
DEBUG = -ggdb
OPTIMIZE = -O0
CFLAGS = $(shell pkg-config --cflags $(PKGS))
LIBS = $(shell pkg-config --libs $(PKGS)) -ldl -lpthread

alloctest: alloctest.c
	$(CC) -o $@ $(WARNINGS) $(DEBUG) $(OPTIMIZE) $(CFLAGS) $(LIBS) $^

clean:
	rm -f alloctest
