DAEMONNAME = des-aodv
VERSION_MAJOR = 1
VERSION_MINOR = 5
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR)
DESTDIR ?=

DIR_BIN = $(DESTDIR)/usr/sbin
DIR_ETC = $(DESTDIR)/etc
DIR_DEFAULT = $(DIR_ETC)/default
DIR_INIT = $(DIR_ETC)/init.d

MODULES = src/aodv src/helper src/cli/aodv_cli src/database/aodv_database src/database/timeslot src/database/neighbor_table/nt src/database/data_seq/ds \
	src/database/packet_buffer/packet_buffer src/database/rerr_log/rerr_log src/database/routing_table/aodv_rt src/database/rreq_log/rreq_log \
	src/database/schedule_table/aodv_st src/pipeline/aodv_periodic src/pipeline/aodv_pipeline src/pipeline/aodv_metric src/pipeline/aodv_forward \
	src/pipeline/aodv_gossip src/database/pdr_tracker/pdr 

UNAME = $(shell uname | tr 'a-z' 'A-Z')
TARFILES = src etc Makefile ChangeLog android.files icon.*

FILE_DEFAULT = etc/$(DAEMONNAME).default
FILE_ETC = etc/$(DAEMONNAME).conf
FILE_INIT = etc/$(DAEMONNAME).init

LIBS = -ldessert -lpthread -lcli
CFLAGS += -std=gnu99 -D_GNU_SOURCE

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

build: $(DAEMONNAME)

$(DAEMONNAME): $(addsuffix .o,$(MODULES))
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

android: CC=android-gcc
android: CFLAGS=-I$(DESSERT_LIB)/include
android: LDFLAGS=-L$(DESSERT_LIB)/lib -Wl,-rpath-link=$(DESSERT_LIB)/lib -ldessert
android: build package

package:
	mv $(DAEMONNAME) android.files/daemon
	zip -j android.files/$(DAEMONNAME).zip android.files/*

tarball: clean
	mkdir des-aodv
	cp -r $(TARFILES) des-aodv
	tar -czf des-aodv.tar.gz des-aodv
	rm -rf des-aodv

debian: tarball
	cp $(DAEMONNAME).tar.gz ../debian/tarballs/$(DAEMONNAME).orig.tar.gz

.SILENT: clean
