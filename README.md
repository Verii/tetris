# Falling blocks game

## Building
This program links with sqlite3 (3.8+) (3.7?) and libpthreads.
Any POSIX compliant pthreads implementation should work fine.
I use GCC 4.8.x to build. Different version may report misc. errors during
the build. Patches are welcome :)

## Contributions
To help with the understanding of this program(it's quite simple), you should
first read the overviews in docs/files/\* to get an idea of what does what.
src/main.c is also a good place to start.

## Note
These are currently out of date. v0.19 -> v0.20 introduces large architectural
changes. I don't have time to update these, but they will be correct before my
github branch sets v0.20 as the default branch.

v0.20 is still unfinished; I'm not going to start fixing the docs anyway.

## License
Read the LICENSE file.

![screenshot](assets/screenshot.png "falling blocks game")
