
OS := $(shell uname -s)
ARCH := $(shell sh print_arch)

SETUPDB = ../setupdb
CFLAGS = -g -Wall
CFLAGS += -I$(SETUPDB)
CFLAGS += $(shell xdelta-config --cflags) $(shell xml-config --cflags)
LFLAGS += -L$(SETUPDB)/$(ARCH) -lsetupdb
LFLAGS += $(shell xdelta-config --libs) $(shell xml-config --libs) -static

SHARED_OBJS = load_patch.o size_patch.o loki_xdelta.o mkdirhier.o log_output.o

MAKE_PATCH_OBJS = make_patch.o tree_patch.o save_patch.o

LOKI_PATCH_OBJS = loki_patch.o apply_patch.o

ALL_OBJS = $(SHARED_OBJS) $(MAKE_PATCH_OBJS) $(LOKI_PATCH_OBJS)

#MEM_DEBUG = -lefence

all: make_patch loki_patch

make_patch: $(MAKE_PATCH_OBJS) $(SHARED_OBJS)
	$(CC) -o $@ $^ $(MEM_DEBUG) $(LFLAGS)

loki_patch: $(LOKI_PATCH_OBJS) $(SHARED_OBJS)
	$(CC) -o $@ $^ $(MEM_DEBUG) $(LFLAGS)

test: all cleanpat
	gzip -cd test.tar.gz | tar xf -
	./make_patch test/patch/patch.dat load-file test/build-patch
	./make_patch test/patch/patch.dat load-file test/build-patch
	mkdir test/out
	cp -rp test/old/* test/out/
	cp -rp test/bin-1.1a/* test/out/
	(cd test/patch; ../../loki_patch -v patch.dat ../out)

install: all
	if test ! -d image/bin/$(OS); then mkdir image/bin/$(OS); fi
	if test ! -d image/bin/$(OS)/$(ARCH); then mkdir image/bin/$(OS)/$(ARCH); fi
	cp loki_patch image/bin/$(OS)/$(ARCH)/
	strip image/bin/$(OS)/$(ARCH)/loki_patch
	-brandelf -t $(OS) image/bin/$(OS)/$(ARCH)/loki_patch
	cp image/bin/$(OS)/$(ARCH)/loki_patch /loki/patch-tools/image/bin/$(OS)/$(ARCH)/loki_patch

clean: cleanpat
	rm -f *.o core

distclean: clean
	rm -f make_patch loki_patch

cleanpat:
	rm -rf test

dep: depend

depend:
	$(CC) -MM $(CFLAGS) $(ALL_OBJS:.o=.c) > .depend

ifeq ($(wildcard .depend),.depend)
include .depend
endif
