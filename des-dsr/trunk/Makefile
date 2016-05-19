DAEMONNAME = des-dsr-devel
MAJOR = $(shell cat major_version)
BUILD = $(shell cat build)
VERSION = $(MAJOR)$(BUILD)
NAME = "Hot Rod"

MODULES = dsr blacklist linkcache rreqtable routecache maintenance_buffer alloc_cache helper etx conf sendbuffer statistics
TESTMODULES = blacklist linkcache rreqtable routecache maintenance_buffer alloc_cache helper etx conf sendbuffer statistics

UNAME = $(shell uname | tr 'a-z' 'A-Z')
TARFILES = *.c *.h Makefile.* *.conf *.init *.default *.sh build version major_version ChangeLog

LIBS = dessert  pthread cli
CFLAGS =  -ggdb -rdynamic -g3 -O0 -Wall -DTARGET_$(UNAME) -D_GNU_SOURCE  -DHASH_DEBUG=1
LDFLAGS += $(addprefix -l,$(LIBS))

all: dsr

clean:
	rm -f *.o  *.tar.gz ||  true
	rm -f $(DAEMONNAME) || true
	rm -rf $(DAEMONNAME).dSYM || true
	rm -f linkcache-test || true
	rm -f linkcache2-test || true
	rm -f source-test || true
	rm -f blacklist-test || true
	rm -f test/*.o || true

tarball: clean
	mkdir des-dsr-$(VERSION)
	cp -r $(TARFILES) des-dsr-$(VERSION)
	tar -czf des-dsr-$(VERSION).tar.gz des-dsr-$(VERSION)
	rm -rf des-dsr-$(VERSION)

dsr: $(addsuffix .o,$(MODULES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(DAEMONNAME) $(addsuffix .o,$(MODULES))

android: CC=android-gcc
android: CFLAGS = -I$(DESSERT_LIB)/include
android: LDFLAGS = -L$(DESSERT_LIB)/lib -Wl,-rpath-link=$(DESSERT_LIB)/lib -ldessert 
android: dsr package

package:
	mv $(DAEMONNAME) android.files/daemon
	zip -j android.files/$(DAEMONNAME).zip android.files/*

ver:
	$(shell ./version.sh $(NAME))

release: ver dsr

linkcache-test:  test/linkcache-test.o $(addsuffix .o,$(TESTMODULES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o linkcache-test test/linkcache-test.o $(addsuffix .o,$(TESTMODULES))

linkcache2-test:  test/linkcache2-test.o $(addsuffix .o,$(TESTMODULES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o linkcache2-test test/linkcache2-test.o $(addsuffix .o,$(TESTMODULES))

source-test:  test/source-test.o $(addsuffix .o,$(TESTMODULES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o source-test test/source-test.o $(addsuffix .o,$(TESTMODULES))

blacklist-test:  test/blacklist-test.o $(addsuffix .o,$(TESTMODULES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o blacklist-test test/blacklist-test.o $(addsuffix .o,$(TESTMODULES))
