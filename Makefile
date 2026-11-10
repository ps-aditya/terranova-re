CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
SRC := src/lg_res.c src/lg_bitmap.c src/lg_scene.c src/lgres_cli.c
HDR := src/lg_res.h src/lg_bitmap.h src/lg_scene.h

all: lgres

lgres: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	-del /q lgres 2>nul || rm -f lgres

.PHONY: all clean
