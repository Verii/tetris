BIN = blocks
VERSION = v0.15
SRC = src/main.c src/blocks.c src/screen.c src/debug.c src/db.c src/net.c
SRC+= src/client.c

OBJS = ${SRC:.c=.o}

INCS = -I./include
CPPFLAGS = -DVERSION=\"${VERSION}\" ${INCS}

DEBUG = -g -O0 -DDEBUG
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -Os ${DEBUG}

LIBS = -lm -lncurses -lpthread -lsqlite3
LDFLAGS = ${LIBS}

CC = cc

all:
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}
