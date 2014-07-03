BIN = blocks
VERSION = v0.13
SRC = main.c blocks.c screen.c debug.c db.c

OBJS = ${SRC:.c=.o}

INCS = -I../include
CPPFLAGS = -DVERSION=\"${VERSION}\" ${INCS}

DEBUG = -g -DDEBUG
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -Os ${DEBUG}

LIBS = -lm -lncurses -lpthread -lsqlite3
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
