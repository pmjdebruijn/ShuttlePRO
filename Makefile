
# Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

#CFLAGS=-g -W -Wall
CFLAGS=-O3 -W -Wall

DESTDIR=

OBJ=\
	readconfig.o \
	shuttlepro.o

all: shuttleevent

install: all
	install -d ${DESTDIR}/lib/udev/rules.d
	install -m 0644 99-contour-shuttle.rules ${DESTDIR}/lib/udev/rules.d
	install -d ${DESTDIR}/usr/bin
	install -m 0755 shuttleevent ${DESTDIR}/usr/bin
	install -d ${DESTDIR}/usr/lib/systemd/user
	install -m 0644 shuttleevent.service ${DESTDIR}/usr/lib/systemd/user
	install -d ${DESTDIR}/etc/xdg/autostart
	install -m 0644 shuttleevent.desktop ${DESTDIR}/etc/xdg/autostart
	install -d ${DESTDIR}/usr/share/doc/shuttleevent/examples
	install -m 0644 example.shuttlerc ${DESTDIR}/usr/share/doc/shuttleevent/examples

shuttleevent: ${OBJ}
	gcc ${CFLAGS} ${OBJ} -o shuttleevent -L /usr/X11R6/lib -lX11 -lXtst

clean:
	rm -f shuttleevent keys.h $(OBJ)

keys.h: keys.sed /usr/include/X11/keysymdef.h
	sed -f keys.sed < /usr/include/X11/keysymdef.h > keys.h

readconfig.o: shuttle.h keys.h
shuttlepro.o: shuttle.h
