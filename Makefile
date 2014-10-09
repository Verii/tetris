BIN = blocks
VERSION = v0.19.2
SRC = src/main.c src/blocks.c src/screen.c src/debug.c src/db.c
OBJS = ${SRC:.c=.o}

SRCDIR = /usr/local/bin

CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" -DNDEBUG -I./include

DEBUG = -g -Og -DDEBUG
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -Os

LDFLAGS = -lm -lbsd -lncurses -lpthread -lsqlite3

.PREFIX:
.PREFIX: .c .o

## Build with debugging flags when told to produce .o files.
.c.o:
	${CC} -c $< -o $@ ${CPPFLAGS} ${CFLAGS} ${DEBUG}

all:
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}

debug: ${OBJS}
	${CC} $^ ${LDFLAGS} -o blocks-$@

install:
	install -sp -o root -g root --mode=755 -t ${SRCDIR} ${BIN}

clean:
	-rm -f ${BIN} ${OBJS}
