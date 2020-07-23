CC = gcc
FLAGS = -Iinclude/SLAV -lm -Wno-pointer-to-int-cast -Wno-format -Wno-int-to-pointer-cast -ggdb -static-libgcc
SRC_DAT = $(wildcard src/datatypes/*.c)
OBJ_DAT = $(patsubst %.c,%.o,$(SRC_DAT))
SRC_SLAV = $(wildcard src/slav.c) $(wildcard src/parser/*.c) $(wildcard src/regex/*.c)
OBJ_SLAV = $(patsubst %.c,%.o,$(SRC_SLAV))
SRC_UTILS = $(wildcard src/util/*.c)
OBJ_UTILS = $(patsubst %.c,%.o,$(SRC_UTILS))
SRC_BIGI = $(wildcard src/bigint/*.c)
OBJ_BIGI := $(patsubst %.c,%.o,$(SRC_BIGI))
SRC_PUT := $(wildcard src/putter/*.c)
OBJ_PUT := $(patsubst %.c,%.o,$(SRC_PUT))
PREFIX := /usr/local
ifeq ($(OS),Windows_NT)
SHARED :=.dll
SHARE_INC := .dll.a
else
SHARED :=.so
SHARE_INC := .so
endif

static_%:libs/lib%.a
	
shared_%:libs/lib%$(SHARED)
	
both_%:shared_% static_%
	

libs/libslavint.a:$(OBJ_BIGI)
libs/libslavint$(SHARED):$(OBJ_BIGI)

libs/libslav.a:$(OBJ_SLAV)
libs/libslav$(SHARED):$(OBJ_SLAV) libs/libdatam$(SHARED) libs/libslavio$(SHARED)

libs/libdatam.a:$(OBJ_DAT)
libs/libdatam$(SHARED):$(OBJ_DAT)

libs/libslavio.a:$(OBJ_UTILS)
libs/libslavio$(SHARED):$(OBJ_UTILS)

libs/libputter.a:$(OBJ_PUT)
libs/libputter$(SHARED):$(OBJ_PUT) libs/datam$(SHARED) libs/slavio$(SHARED) libs/bigint$(SHARED)

put:$(OBJ_BIGI) $(OBJ_PUT) $(OBJ_UTILS) $(OBJ_DAT) $(OBJ_SLAV)
	$(CC) -o put $(OBJ_BIGI) $(OBJ_PUT) $(OBJ_UTILS) $(OBJ_DAT) $(OBJ_SLAV) $(FLAGS)

libs/lib%.a:
	ar rcs $@ $?

libs/%.dll:
	$(CC) $(FLAGS) -shared -o $@ -Llibs $(patsubst libs/%$(SHARED),-l%,$?) -Wl,--out-implib,$@.a

libs/%.so:
	$(CC) $(FLAGS) --shared -o $@ $?

test_static:static_slav static_datam static_slavio
	$(CC) $(FLAGS) -o test/test_static src/test.c -Llibs -lslav -ldatam -lslavio

test_shared:shared_slav shared_datam
	cp libs/libslav$(SHARED) test
	cp libs/libslavio$(SHARED) test
	cp libs/libdatam$(SHARED) test
	$(CC) $(FLAGS) -o test/test_shared src/test.c -Llibs -lslav

langbuilder_static:static_slav static_datam static_slavio
	$(CC) $(FLAGS) -o test/langbuilder_static src/langbuilder.c -Llibs -lslav -ldatam -lslavio

langbuilder_shared:shared_slav
	cp libs/libslav$(SHARED) test
	cp libs/libdatam$(SHARED) test
	cp libs/libslavio$(SHARED) test
	$(CC) $(FLAGS) -o test/langbuilder_shared src/langbuilder.c -Llibs -lslav

install:
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include/SLAV
	cp libs/*$(SHARED)* $(DESTDIR)$(PREFIX)/lib
	cp include/SLAV/*.h $(DESTDIR)$(PREFIX)/include/SLAV

%.o:%.c
	$(CC) $(FLAGS) -c -o $@ $<

.PHONY:clean
clean:
	rm -f $(OBJ_DAT) $(OBJ_SLAV) $(OBJ_UTILS) $(OBJ_BIGI) $(OBJ_PUT)

.PHONY:destroy
destroy:clean
	rm -f libs/*