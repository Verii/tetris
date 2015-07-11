BIN = tetris
VERSION = v0.73

SRC =	src/main.c \
	src/conf.c \
	src/events.c \
	src/helpers.c \
	src/input.c \
	src/logs.c \
	src/screen.c \
	src/screen_nodraw.c \
	src/screen_nc.c \
	src/db_sqlite.c \
	src/tetris.c \
	src/net/network.c \
	src/net/pack.c \
	src/net/serialize.c

DESTDIR = /usr/local/bin

CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" -DNDEBUG -I./include

DEBUG = -g -Og -DDEBUG
CFLAGS = -std=c99 -Wall -Wextra -Werror -Os

LDFLAGS = -lm -lncursesw -lrt -lsqlite3 -ljson-c

.PREFIX:
.PREFIX: .c .o

all:
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}

install: all
	install -sp -o root -g root --mode=755 -t ${DESTDIR} ${BIN}

## Build with debugging flags when told to produce .o files.
OBJS = ${SRC:.c=.o}
.c.o:
	${CC} -c $< -o $@ ${CPPFLAGS} ${CFLAGS} ${DEBUG}

debug: ${OBJS}
	${CC} ${OBJS} ${LDFLAGS} -o ${BIN}-$@

clean:
	-rm -f ${BIN} ${BIN}-debug ${OBJS}

.PHONY: clean debug install all
