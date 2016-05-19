DAEMONNAME = des-gossip-adv
MAJOR = $(shell cat major_version)
BUILD = $(shell cat build)
VERSION = $(MAJOR)$(BUILD)
DESTDIR ?= /usr/sbin
PREFIX ?=
NAME = "gossip routing"

DIR_BIN = $(PREFIX)$(DESTDIR)
DIR_ETC = $(PREFIX)/etc
DIR_DEFAULT = $(DIR_ETC)/default
DIR_INIT = $(DIR_ETC)/init.d
MODULES = gossiping gossiping_core gossiping_cli gossiping_neighbors gossiping_packettrap gossiping_log gossiping_count gossiping_nhdp gossiping_13

UNAME = $(shell uname | tr 'a-z' 'A-Z')
TARFILES = *.c *.h Makefile *.conf *.init *.default *.sh build version major_version ChangeLog *.lua

FILE_DEFAULT = ./$(DAEMONNAME).default
FILE_ETC = ./$(DAEMONNAME).conf
FILE_INIT = ./$(DAEMONNAME).init
FILES_WPLUGIN = des-gossip-adv.lua

LIBS = dessert m cli pthread
UNAME = $(shell uname | tr 'a-z' 'A-Z')
CFLAGS += -ggdb -Wall -DTARGET_$(UNAME) -D_GNU_SOURCE
LDFLAGS += $(addprefix -l,$(LIBS))

all: gossiping

clean:
	rm -f *.o *.tar.gz ||  true
	rm -f $(DAEMONNAME) || true
	rm -rf $(DAEMONNAME).dSYM || true

install:
	mkdir -p $(DIR_BIN)
	install -m 744 $(DAEMONNAME) $(DIR_BIN)
	mkdir -p $(DIR_ETC)
	install -m 644 $(FILE_ETC) $(DIR_ETC)
	mkdir -p $(DIR_DEFAULT)
	install -m 644 $(FILE_DEFAULT) $(DIR_DEFAULT)/$(DAEMONNAME)
	mkdir -p $(DIR_INIT)
	install -m 755 $(FILE_INIT) $(DIR_INIT)/$(DAEMONNAME)

gossiping: $(addsuffix .o,$(MODULES)) gossiping.h
	$(CC) $(CFLAGS) -o $(DAEMONNAME) $(addsuffix .o,$(MODULES)) $(LDFLAGS)

android: CC=android-gcc
android: CFLAGS=-I$(DESSERT_LIB)/include -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR)
android: LDFLAGS=-L$(DESSERT_LIB)/lib -Wl,-rpath-link=$(DESSERT_LIB)/lib -ldessert
android: gossiping package

package:
	mv $(DAEMONNAME) android.files/daemon
	zip -j android.files/$(DAEMONNAME).zip android.files/*

tarball: clean
	mkdir des-gossip-adv-$(VERSION)
	cp -r $(TARFILES) des-gossip-adv-$(VERSION)
	tar -czf des-gossip-adv-$(VERSION).tar.gz des-gossip-adv-$(VERSION)
	rm -rf des-gossip-adv-$(VERSION)

ver:
	$(shell ./version.sh $(NAME))

release: ver gossiping

debian: tarball
	cp $(DAEMONNAME)-$(VERSION).tar.gz ../debian/tarballs/$(DAEMONNAME)_$(VERSION).orig.tar.gz

