CC =	cc
CFLAGS =  -std=gnu11 -pipe  -O -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror -g 
SRC_INCS = -I src

default:	build

clean:
	rm -rf *.o src/*.o

build:
		$(CC)  $(CFLAGS) $(SRC_INCS) -o bin/csite src/website.c
	