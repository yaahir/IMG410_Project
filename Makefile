CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -O2

all: blur

blur: blur.c
	$(CC) $(CFLAGS) -o blur blur.c

clean:
	rm -f blur
.PHONY: all clean
