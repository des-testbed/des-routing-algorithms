DAEMONNAME = des-hello

CC = cc
LIBS = dessert  cli
CFLAGS += -W -Wall -g3 -ggdb -O0
LDFLAGS += $(addprefix -l,$(LIBS))
SOURCE = $(addsuffix .c,$(DAEMONNAME))

all: $(DAEMONNAME)

clean:
	rm -f $(DAEMONNAME) || true

$(DAEMONNAME) : $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

.SILENT: clean

