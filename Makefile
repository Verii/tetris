BIN = tetris
SRC = src/main.c src/blocks.c src/menu.c src/debug.c src/db.c

CPPFLAGS = -DDEBUG -D_POSIX_C_SOURCE=200112L -DVERSION=\"v0.20\" -I./include

#RELEASE = -UDEBUG -O3
CFLAGS = -std=c99 -Wall -Wextra -Werror -O0 -g ${RELEASE}

#LD_RELEASE = -s
LDFLAGS = ${LD_RELEASE} -lm -lncurses -lpthread -lsqlite3

CC = cc

all:
	${CC} -o ${BIN} ${CPPFLAGS} ${CFLAGS} ${SRC} ${LDFLAGS}
