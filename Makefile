CC = gcc
FLAGS = -Iinclude/SLAV -g -lm -Wno-pointer-to-int-cast -Wno-format -Wno-int-to-pointer-cast
SRC_DAT = $(wildcard src/datatypes/*.c)
OBJ_DAT = $(patsubst %.c, %.o, $(SRC_DAT))
SRC_SLAV = $(wildcard src/slav.c) $(wildcard src/parser/*.c) $(wildcard src/regex/*.c)
OBJ_SLAV = $(patsubst %.c, %.o, $(SRC_SLAV))
SRC_UTILS = $(wildcard src/util/*.c)
OBJ_UTILS = $(patsubst %.c, %.o, $(SRC_UTILS))
SRC_BIGI = $(wildcard src/bigint/*.c)
OBJ_BIGI = $(patsubst %.c, %.o, $(SRC_BIGI))
SRC_PUT = $(wildcard src/putter/*.c)
OBJ_PUT = $(patsubst %.c, %.o, $(SRC_PUT))

static_%: libs/lib%.a
	

shared_%: libs/%.so
	

libs/libslavint.a: $(OBJ_BIGI)
libs/slavint.so: $(OBJ_BIGI)

libs/libslav.a: $(OBJ_SLAV)
libs/slav.so: $(OBJ_SLAV) libs/datam.so libs/slavio.so

libs/libdatam.a: $(OBJ_DAT)
libs/datam.so: $(OBJ_DAT)

libs/libslavio.a: $(OBJ_UTILS)
libs/slavio.so: $(OBJ_UTILS)

libs/libputter.a: $(OBJ_PUT)
libs/putter.so: $(OBJ_PUT) libs/datam.so libs/slavio.so libs/bigint.so

put: $(OBJ_BIGI) $(OBJ_PUT) $(OBJ_UTILS) $(OBJ_DAT) $(OBJ_SLAV)
	$(CC) -o put $(OBJ_BIGI) $(OBJ_PUT) $(OBJ_UTILS) $(OBJ_DAT) $(OBJ_SLAV) $(FLAGS)

libs/lib%.a:
	ar rcs $@ $?

libs/%.so:
	$(CC) $(FLAGS) --shared -o $@ $?

test_static: static_slav static_datam
	$(CC) $(FLAGS) -o test/test_static src/test.c -Llibs -lslav

test_shared: shared_slav
	cp libs/slav.so test
	cp libs/datam.so test
	$(CC) $(FLAGS) -o test/test_shared src/test.c libs/slav.so libs/datam.so libs/slavio.so

langbuilder_static: static_slav static_datam static_slavio
	$(CC) $(FLAGS) -o test/langbuilder_static src/langbuilder.c -Llibs -lslav -ldatam -lslavio

langbuilder_shared: shared_slav
	cp libs/slav.so test
	cp libs/datam.so test
	cp libs/slavio.so test
	$(CC) $(FLAGS) -o test/langbuilder_shared src/langbuilder.c libs/slav.so libs/datam.so libs/slavio.so

%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJ_DAT) $(OBJ_SLAV) $(OBJ_UTILS) $(OBJ_BIGI) $(OBJ_PUT)

.PHONY: destroy
destroy: clean
	rm -f libs/*