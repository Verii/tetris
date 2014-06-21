BIN = tetris
VERSION = 0.2
SRC = ${BIN}.c blocks.c debug.c

OBJS = ${SRC:.c=.o}

INCS = -I../include
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" ${INCS}

DEBUG = -g -Og -DDEBUG
CFLAGS = -std=c99 -Wpedantic -Wall -Wextra -Werror -Os ${DEBUG}

LIBS = -lm -lncurses -lpthread
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
