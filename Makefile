.PHONY: slav.so
slav.so:
	gcc --shared -o slav.so slav.c parser/*.c regex/*.c datatypes/*.c

test: slav.so
	gcc -o test test.c slav.so