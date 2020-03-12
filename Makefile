CC = gcc
FLAGS = "-Iinclude/SLAV"
SRC_DAT = $(wildcard src/datatypes/*.c)
OBJ_DAT = $(patsubst %.c, %.o, $(SRC_DAT))
SRC_SLAV = $(wildcard src/*.c) $(wildcard src/parser/*.c) $(wildcard src/regex/*.c)
OBJ_SLAV = $(patsubst %.c, %.o, $(SRC_SLAV))
SRC_UTILS = $(wildcard src/util/*.c)
OBJ_UTILS = $(patsubst %.c, %.o, $(SRC_UTILS))
SRC_BIGI = $(wildcard src/bigint/*.c)
OBJ_BIGI = $(patsubst %.c, %.o, $(SRC_BIGI))

static_%: libs/lib%.a
	

shared_%: libs/%.so
	

libs/libslavint.a: $(OBJ_BIGI)
libs/slavint.so: $(OBJ_BIGI)

libs/libslav.a: $(OBJ_SLAV) $(OBJ_DAT) $(OBJ_UTILS)
libs/slav.so: $(OBJ_SLAV) libs/datam.so libs/slavio.so

libs/libdatam.a: $(OBJ_DAT)
libs/datam.so: $(OBJ_DAT)

libs/libslavio.a: $(OBJ_UTILS)
libs/slavio.so: $(OBJ_UTILS)

libs/lib%.a:
	ar rcs $@ $?

libs/%.so:
	$(CC) $(FLAGS) --shared -o $@ $?

test_static: static_slav
	$(CC) $(FLAGS) -o test/test_static src/test.c -Llibs -lslav

test_shared: shared_slav shared_slavio
	cp libs/slav.so test
	cp libs/datam.so test
	$(CC) $(FLAGS) -o test/test_shared src/test.c libs/slav.so libs/datam.so libs/slavio.so

%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJ_DAT) $(OBJ_SLAV) $(OBJ_UTILS) $(OBJ_BIGI)

.PHONY: destroy
destroy: clean
	rm -f libs/*