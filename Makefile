SRC =	src/main.c \
	src/conf.c \
	src/events.c \
	src/helpers.c \
	src/input.c \
	src/logs.c \
	src/db.c \
	src/tetris.c \
	src/screen.c

VERSION = v1.0

CPPFLAGS = -DVERSION=\"${VERSION}\" -DNDEBUG -UDEBUG
CFLAGS   = -std=gnu11 -Wall -Wextra -Wpedantic -Wextra -Os
CFLAGS  += -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wformat=2
CFLAGS  += -Wmissing-prototypes -Wmissing-prototypes -Wredundant-decls
LDFLAGS  =
LDLIBS   = -lm -lrt -lncurses -lsqlite3

## Debugging flags
#CPPFLAGS += -UNDEBUG -DDEBUG
#CFLAGS += -Og -ggdb3

## Enable LLVM/Clang
#CC = clang
#CFLAGS += -Weverything

tetris: $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $^ -o $@

clean:
	rm -f tetris

.PHONY: clean
