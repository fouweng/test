CFLAGS += -Wall -g  -I../include
LDFLAGS += -L../ -loctopus

.PHONY: all tmpfs romfs

SHARED_LIB_RTSIO = librtsio.so
SHARED_LIB_OPTIO = opt_io.so
all: $(SHARED_LIB_RTSIO) $(SHARED_LIB_OPTIO) testptz tmpfs

$(SHARED_LIB_RTSIO):rtsio.c
	$(CC) -shared $(CFLAGS) -fPIC -o $@ $^

$(SHARED_LIB_OPTIO):opt_io.c
	$(CC) -shared $(CFLAGS) -fPIC -o $@ $^

testptz:testptz.c librtsio.so
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

tmpfs:
	cp -f $(SHARED_LIB_RTSIO) $(DIR_TMPFS)/lib
	cp -f $(SHARED_LIB_OPTIO) $(DIR_TMPFS)/lib
	cp -f rtsio.h $(DIR_TMPFS)/include

romfs:
	$(ROMFSINST) $(SHARED_LIB_RTSIO) /lib
	$(ROMFSINST) testptz /bin
	mkdir -p $(DIR_ROMFS)/usr/lib/octopus
	$(ROMFSINST) $(SHARED_LIB_OPTIO) /usr/lib/octopus

clean:
	rm -rf *.o *.so testptz
