CC =	cc
CFLAGS =  -std=gnu11 -pipe  -O2 -g\
		-Wall -Wextra  -Werror \
		-Wpointer-arith \
		-Wconditional-uninitialized \
		-Wno-unused-parameter \
		-Wno-deprecated-declarations
SRC_INCS = -I src

default:	build

build:
		$(CC)  $(CFLAGS) $(SRC_INCS) -o bin/csite src/website.c
	