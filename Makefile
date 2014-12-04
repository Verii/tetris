BIN = tetris
VERSION = v0.30
SRC = src/main.c src/bag.c src/blocks.c src/db.c src/screen.c \
      src/logs.c src/conf.c src/helpers.c
OBJS = ${SRC:.c=.o}

DESTDIR = /usr/local/bin

CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" -DNDEBUG -I./include

DEBUG = -g -Og -DDEBUG
CFLAGS = -std=c99 -Wall -Wextra -Werror -Os

LDFLAGS = -lm -lbsd -lncurses -lpthread -lsqlite3 -L./sexp/ -lsexp

## Build with debugging flags when told to produce .o files.
.c.o:
	${CC} -c $< -o $@ ${CPPFLAGS} ${CFLAGS} ${DEBUG}

all: sexp
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}

debug: sexp ${OBJS}
	${CC} $^ ${LDFLAGS} -o ${BIN}-$@

sexp:
	${MAKE} -C sexp/

install: all
	install -sp -o root -g root --mode=755 -t ${DESTDIR} ${BIN}

clean:
	${MAKE} -C sexp/ clean
	-rm -f ${BIN} ${BIN}-debug ${OBJS}

.PHONY: sexp
