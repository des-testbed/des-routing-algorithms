DAEMONNAME = des-ara
DESTDIR ?= /usr/sbin
PREFIX ?=

DIR_BIN = $(PREFIX)$(DESTDIR)
DIR_ETC = $(PREFIX)/etc
DIR_DEFAULT = $(DIR_ETC)/default
DIR_INIT = $(DIR_ETC)/init.d
MODULES=ara_main ara_rt ara_packethandlers ara_packettrap ara_loopprotect ara_rmnt ara_trace ara_dump
UNAME = $(shell uname | tr 'a-z' 'A-Z')

FILE_DEFAULT = ./$(DAEMONNAME).default
FILE_ETC = ./$(DAEMONNAME).conf
FILE_INIT = ./$(DAEMONNAME).init

LIBS = dessert
CFLAGS += -ggdb -Wall -DTARGET_$(UNAME) -D_GNU_SOURCE 
LDFLAGS += $(addprefix -l,$(LIBS))

all: ara

clean:
	rm -f *.o ||  true
	rm -f $(DAEMONNAME) || true
	rm -rf $(DAEMONNAME).dSYM || true

install:
	mkdir -p $(DIR_BIN)
	install -m 744 $(DAEMONNAME) $(DIR_BIN)
	mkdir -p $(DIR_ETC)
	install -m 644 $(FILE_ETC) $(DIR_ETC)
#	mkdir -p $(DIR_DEFAULT)
#	install -m 644 $(FILE_DEFAULT) $(DIR_DEFAULT)
#	mkdir -p $(DIR_INIT)
#	install -m 755 $(FILE_INIT) $(DIR_INIT)

ara: $(addsuffix .o,$(MODULES)) ara.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(DAEMONNAME) $(addsuffix .o,$(MODULES))

