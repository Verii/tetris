BIN = tetris
VERSION = 0.1
SRC = ${BIN}.c blocks.c debug.c

OBJS = ${SRC:.c=.o}

INCS = -I../include
CPPFLAGS = -D_GNU_SOURCE -DVERSION=\"${VERSION}\" ${INCS}

DEBUG = -g -Og -DDEBUG
CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 ${DEBUG}

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
