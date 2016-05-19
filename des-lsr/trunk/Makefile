DAEMONNAME = des-lsr
VERSION_MAJOR = 0
VERSION_MINOR = 3
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)
DESTDIR ?=

DIR_BIN = $(DESTDIR)/usr/sbin
DIR_ETC = $(DESTDIR)/etc
DIR_DEFAULT = $(DIR_ETC)/default
DIR_INIT = $(DIR_ETC)/init.d

MODULES = src/des-lsr src/des-lsr_routinglogic src/des-lsr_packethandler src/des-lsr_dijkstra src/des-lsr_cli

UNAME = $(shell uname | tr 'a-z' 'A-Z')
TARFILES = src etc Makefile ChangeLog android.files icon.*

FILE_DEFAULT = etc/$(DAEMONNAME).default
FILE_ETC = etc/$(DAEMONNAME).conf
FILE_INIT = etc/$(DAEMONNAME).init

LIBS = dessert pthread cli
CFLAGS += -ggdb -Wall -DTARGET_$(UNAME) -D_GNU_SOURCE -I/usr/include
LDFLAGS += $(addprefix -l,$(LIBS))

all: build

clean:
	rm -f *.o *.tar.gz ||  true
	find . -name *.o -delete
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

build: $(addsuffix .o,$(MODULES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(DAEMONNAME) $(addsuffix .o,$(MODULES))

android: CC=android-gcc
android: CFLAGS=-I$(DESSERT_LIB)/include
android: LDFLAGS=-L$(DESSERT_LIB)/lib -Wl,-rpath-link=$(DESSERT_LIB)/lib -ldessert
android: build package

package:
	mv $(DAEMONNAME) android.files/daemon
	zip -j android.files/$(DAEMONNAME).zip android.files/*

tarball: clean
	mkdir des-lsr
	cp -r $(TARFILES) des-lsr
	tar -czf des-lsr.tar.gz des-lsr
	rm -rf des-lsr

debian: tarball
	cp $(DAEMONNAME).tar.gz ../debian/tarballs/$(DAEMONNAME).orig.tar.gz

.SILENT: clean
