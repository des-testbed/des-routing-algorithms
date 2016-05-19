DAEMONNAME = des-ara
VERSION_MAJOR = 2
VERSION_MINOR = 1
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)
DESTDIR ?= /usr/sbin
PREFIX ?=

DIR_BIN = $(PREFIX)$(DESTDIR)
DIR_ETC = $(PREFIX)/etc
DIR_DEFAULT = $(DIR_ETC)/default
DIR_INIT = $(DIR_ETC)/init.d
MODULES=ara_main ara_newrt ara_packethandlers ara_packettrap ara_loopprotect ara_rmnt ara_cli ara_ack ara_classification
UNAME = $(shell uname | tr 'a-z' 'A-Z')
TARFILES = *.c *.h Makefile *.conf *.init *.default ChangeLog TODO *.lua

FILE_DEFAULT = ./$(DAEMONNAME).default
FILE_ETC = ./$(DAEMONNAME).conf
FILE_INIT = ./$(DAEMONNAME).init

LIBS = dessert  m cli pthread
CFLAGS += -ggdb -W -Wall -Os -mtune=native -DTARGET_$(UNAME) -D_GNU_SOURCE -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR)
LDFLAGS += $(addprefix -l,$(LIBS))

all: ara

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

ara:	$(addsuffix .o,$(MODULES)) ara.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(DAEMONNAME) $(addsuffix .o,$(MODULES))


android: CC=android-gcc
android: CFLAGS=-I$(DESSERT_LIB)/include -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR)
android: LDFLAGS=-L$(DESSERT_LIB)/lib -Wl,-rpath-link=$(DESSERT_LIB)/lib -ldessert
android: ara package

package:
	mv $(DAEMONNAME) android.files/daemon
	zip -j android.files/$(DAEMONNAME).zip android.files/*

tarball: clean
	mkdir $(DAEMONNAME)-$(VERSION)
	cp -R $(TARFILES) $(DAEMONNAME)-$(VERSION)
	tar -czf $(DAEMONNAME)-$(VERSION).tar.gz $(DAEMONNAME)-$(VERSION)
	rm -rf $(DAEMONNAME)-$(VERSION)

debian: tarball
	cp $(DAEMONNAME)-$(VERSION).tar.gz ../debian/tarballs/$(DAEMONNAME)_$(VERSION).orig.tar.gz
