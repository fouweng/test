CFLAGS += -Wall -g  -I./include
LDFLAGS += -L. -loctopus

PLUGINS_y =
PLUGINS_$(CONFIG_OCTOPUS_PLUGIN_CAM) += cam
PLUGINS_$(CONFIG_OCTOPUS_PLUGIN_NM) += nm
PLUGINS_$(CONFIG_OCTOPUS_PLUGIN_IO) += io
PLUGINS_$(CONFIG_OCTOPUS_PLUGIN_UVC) += uvc

SHARED_LIB=liboctopus.so
STATIC_LIB=liboctopus.a
SRC_LIB= utils.c octopus.c
HEADERS=octopus.h uvcvideo.h

all: $(SHARED_LIB) $(STATIC_LIB) octopus plugins tmpfs

$(SHARED_LIB):$(SRC_LIB)
	$(CC) -shared $(CFLAGS) -fPIC -o $@ $^

$(STATIC_LIB):$(SRC_LIB:.c=.o)
	$(AR) cr $@ $^

octopus:server.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -ldl


plugins:
	for i in $(PLUGINS_y) ; do \
		[ ! -d $$i ] || $(MAKE) -C $$i || exit $$? ; \
	done

tmpfs:
	cp -f $(STATIC_LIB) $(DIR_TMPFS)/lib/ 
	cp -f $(SHARED_LIB) $(DIR_TMPFS)/lib/
	cp -f include/octopus.h $(DIR_TMPFS)/include/
	cp -f include/uvcvideo.h $(DIR_TMPFS)/include/
	cp -f include/rts_cam.h $(DIR_TMPFS)/include/

romfs:
	$(ROMFSINST) $(SHARED_LIB) /lib
	$(ROMFSINST) octopus /bin/
	for i in $(PLUGINS_y) ; do \
		[ ! -d $$i ] || $(MAKE) -C $$i romfs || exit $$? ; \
	done

clean:
	rm -f *.o octopus example $(SHARED_LIB) $(STATIC_LIB)
