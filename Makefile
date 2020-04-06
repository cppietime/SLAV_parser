CC = gcc
FLAGS = -Iinclude/SLAV -Wno-pointer-to-int-cast -Wno-format -Wno-int-to-pointer-cast -fPIC
LIBS = -lm
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

libs/libslavint.a: $(OBJ_BIGI)
	ar -rcs $@ $?
libs/libslavint.so: $(OBJ_BIGI)
	$(CC) $(FLAGS) --shared -o $@ $^ $(LIBS)

libs/libslav.a: $(OBJ_SLAV)
	ar -rcs $@ $?
libs/libslav.so: $(OBJ_SLAV) libs/libdatam.so libs/libslavio.so
	$(CC) $(FLAGS) --shared -o $@ $(OBJ_SLAV) $(LIBS)

libs/libdatam.a: $(OBJ_DAT)
	ar -rcs $@ $?
libs/libdatam.so: $(OBJ_DAT)
	$(CC) $(FLAGS) --shared -o $@ $^ $(LIBS)

libs/libslavio.a: $(OBJ_UTILS)
	ar -rcs $@ $?
libs/libslavio.so: $(OBJ_UTILS)
	$(CC) $(FLAGS) --shared -o $@ $^ $(LIBS)

libs/libputter.a: $(OBJ_PUT)
	ar -rcs $@ $?
libs/libputter.so: $(OBJ_PUT) libs/datam.so libs/slavio.so libs/bigint.so
	$(CC) $(FLAGS) --shared -o $@ $^ $(LIBS)

put: $(OBJ_BIGI) $(OBJ_PUT) $(OBJ_UTILS) $(OBJ_DAT) $(OBJ_SLAV)
	$(CC) -o put $(OBJ_BIGI) $(OBJ_PUT) $(OBJ_UTILS) $(OBJ_DAT) $(OBJ_SLAV) $(FLAGS) $(LIBS)

test_static: libs/libslav.a libs/libdatam.a libs/libslavio.a
	$(CC) $(FLAGS) -o test/test_static src/test.c $(LIBS) -Llibs -lslav -ldatam -lslavio

test_shared: libs/libslav.so libs/libslavio.so libs/libdatam.so
	$(CC) $(FLAGS) -o test/test_shared src/test.c $(LIBS) -Llibs -lslav -lslavio -ldatam

langbuilder_static: static_slav static_datam static_slavio
	$(CC) $(FLAGS) -o test/langbuilder_static src/langbuilder.c $(LIBS) -Llibs -lslav -ldatam -lslavio


%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(OBJ_DAT) $(OBJ_SLAV) $(OBJ_UTILS) $(OBJ_BIGI) $(OBJ_PUT)

.PHONY: destroy
destroy: clean
	rm -f libs/*