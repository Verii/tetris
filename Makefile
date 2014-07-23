BIN = blocks
VERSION = v0.20
SRC = src/main.c src/blocks.c src/menu.c src/debug.c src/db.c

INCS = -I./include
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" ${INCS}

#DEBUG = -g -O0 -DDEBUG
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -Os ${DEBUG}

LIBS = -lm -lncurses -lpthread -lsqlite3
LDFLAGS = -s ${LIBS}

CC = cc

all:
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}
