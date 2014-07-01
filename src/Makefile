BIN = blocks
VERSION = v0.9
SRC = main.c blocks.c screen.c debug.c #net.c

OBJS = ${SRC:.c=.o}

INCS = -I../include
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" ${INCS}

DEBUG = -g -Og -DDEBUG
CFLAGS = -std=c99 -Wpedantic -Wall -Wextra -Werror -Os ${DEBUG}

LIBS = -lm -lncurses -lpthread #-lsqlite3
LDFLAGS = ${LIBS}

CC = cc

all: ${BIN}

${BIN}: ${OBJS}
	${CC} -o ${BIN} ${OBJS} ${LDFLAGS}

.c.o:
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

clean:
	-rm -f ${BIN} ${OBJS}

.PHONY: all clean
