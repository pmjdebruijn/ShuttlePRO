
# Copyright 2013 Eric Messick (FixedImagePhoto.com/Contact)

#CFLAGS=-g -W -Wall
CFLAGS=-O3 -W -Wall

INSTALL_DIR=/usr/local/bin

OBJ=\
	readconfig.o \
	shuttlepro.o

all: shuttleevent

install: all
	install shuttleevent ${INSTALL_DIR}

shuttleevent: ${OBJ}
	gcc ${CFLAGS} ${OBJ} -o shuttleevent -L /usr/X11R6/lib -lX11 -lXtst

clean:
	rm -f shuttleevent keys.h $(OBJ)

keys.h: keys.sed /usr/include/X11/keysymdef.h
	sed -f keys.sed < /usr/include/X11/keysymdef.h > keys.h

readconfig.o: shuttle.h keys.h
shuttlepro.o: shuttle.h
