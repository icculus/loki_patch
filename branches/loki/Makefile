
SETUPDB = ../setupdb
CFLAGS = -g -Wall
CFLAGS += -I$(SETUPDB)
CFLAGS += $(shell xdelta-config --cflags) $(shell xml-config --cflags)
LFLAGS += -L$(SETUPDB) -lsetupdb
LFLAGS += $(shell xdelta-config --libs) $(shell xml-config --libs) -static

OS := $(shell uname -s)
ARCH := $(shell ./print_arch)

SHARED_OBJS = load_patch.o size_patch.o loki_xdelta.o mkdirhier.o log_output.o

MAKE_PATCH_OBJS = make_patch.o tree_patch.o save_patch.o $(SHARED_OBJS)

LOKI_PATCH_OBJS = loki_patch.o apply_patch.o $(SHARED_OBJS)

#MEM_DEBUG = -lefence

all: make_patch loki_patch

make_patch: $(MAKE_PATCH_OBJS)
	$(CC) -o $@ $^ $(MEM_DEBUG) $(LFLAGS)

loki_patch: $(LOKI_PATCH_OBJS)
	$(CC) -o $@ $^ $(MEM_DEBUG) $(LFLAGS)
	if test ! -d image/bin/$(OS); then mkdir image/bin/$(OS); fi
	if test ! -d image/bin/$(OS)/$(ARCH); then mkdir image/bin/$(OS)/$(ARCH); fi
	cp $@ image/bin/$(OS)/$(ARCH)/
	strip image/bin/$(OS)/$(ARCH)/$@

test: all cleanpat
	./make_patch test/patch/test.pat load-file test/build-patch
	mkdir test/out
	cp -rp test/old/* test/out/
	cp test/bin-1.1a/* test/out/
	./loki_patch -v test/patch/test.pat test/out

distclean: clean
	rm -f make_patch loki_patch

clean: cleanpat
	rm -f *.o core

cleanpat:
	rm -rf test/patch/data/
	cp test/patch/test.pat.virgin test/patch/test.pat
	rm -rf test/out/
