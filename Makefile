BIN = blocks
VERSION = v0.19.1
SRC = src/main.c src/blocks.c src/screen.c src/debug.c src/db.c

OBJS = ${SRC:.c=.o}

INCS = -I./include
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" ${INCS}

#DEBUG = -g -Og -DDEBUG
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -Os ${DEBUG}

LIBS = -lm -lbsd -lncurses -lpthread -lsqlite3
LDFLAGS = -s ${LIBS}

CC = cc

## Compilation is fast enough that we don't build .o files
## Building takes <1 second on my laptop.
all:
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}

strip: all
	strip -s ${BIN}

clean:
	-rm -f ${BIN}
