CC = gcc
FLAGS = "-Iinclude/SLAV"
SRC_DAT = $(wildcard src/datatypes/*.c)
OBJ_DAT = $(patsubst %.c, %.o, $(SRC_DAT))
SRC_SLAV = $(wildcard src/*.c) $(wildcard src/parser/*.c) $(wildcard src/regex/*.c)
OBJ_SLAV = $(patsubst %.c, %.o, $(SRC_SLAV))
SRC_UTILS = $(wildcard src/util/*.c)
OBJ_UTILS = $(patsubst %.c, %.o, $(SRC_UTILS))

.PHONY: slav_static
slav_static: libs/libslav.a
libs/libslav.a: $(OBJ_SLAV) $(OBJ_DAT)
	ar rcs libs/libslav.a $(OBJ_SLAV) $(OBJ_DAT)

.PHONY: slav_shared
slav_shared: libs/slav.so
libs/slav.so: $(OBJ_SLAV) datatypes_shared
	$(CC) $(FLAGS) --shared -o libs/slav.so libs/datam.so $(OBJ_SLAV)

.PHONY: datatypes_static
datatypes_static: libs/libdatam.a
libs/libdatam.a: $(OBJ_DAT)
	ar rcs libs/libdatam.a $(OBJ_DAT)

.PHONY: datatypes_shared
datatypes_shared: libs/datam.so
libs/datam.so: $(OBJ_DAT)
	$(CC) $(FLAGS) --shared -o libs/datam.so $(OBJ_DAT)

.PHONY: io_static
io_static: libs/libslavio.a
libs/libslavio.a: $(OBJ_UTILS)
	ar rcs libs/libslavio.a $(OBJ_UTILS)

.PHONY: io_shared
io_shared: libs/slavio.so
libs/slavio.so: $(OBJ_UTILS)
	$(CC) $(FLAGS) --shared -o libs/slavio.so $(OBJ_UTILS)

test_static: slav_static
	$(CC) $(FLAGS) -o test/test_static src/test.c -Llibs -lslav

test_shared: slav_shared datatypes_shared
	cp libs/slav.so test
	cp libs/datam.so test
	$(CC) $(FLAGS) -o test/test_shared src/test.c libs/slav.so libs/datam.so

%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJ_DAT) $(OBJ_SLAV)

.PHONY: destroy
destroy: clean
	rm -f libs/*