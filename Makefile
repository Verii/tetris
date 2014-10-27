BIN = blocks
VERSION = v0.21
SRC = src/main.c src/bag.c src/blocks.c src/db.c src/debug.c src/screen.c
OBJS = ${SRC:.c=.o}

DESTDIR = /usr/local/bin

CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" -DNDEBUG -I./include

DEBUG = -g -Og -DDEBUG
CFLAGS = -std=c99 -Wall -Wextra -Werror -Os

LDFLAGS = -lm -lbsd -lncurses -lpthread -lsqlite3

.PREFIX:
.PREFIX: .c .o

## Build with debugging flags when told to produce .o files.
.c.o:
	${CC} -c $< -o $@ ${CPPFLAGS} ${CFLAGS} ${DEBUG}

all:
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}

debug: ${OBJS}
	${CC} $^ ${LDFLAGS} -o ${BIN}-$@

install: all
	install -sp -o root -g root --mode=755 -t ${DESTDIR} ${BIN}

clean:
	-rm -f ${BIN} ${BIN}-debug ${OBJS}
