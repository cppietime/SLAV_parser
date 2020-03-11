/*
Endian-agnostic I/O functions
*/

#ifndef _H_SAFEIO
#define _H_SAEFIO

#include <stdint.h>
#include <stdio.h>

#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 0

void safe_write(uint64_t, int, int, FILE*);
uint64_t safe_read(int, int, FILE*);
void double_write(double, int, FILE*);
double double_read(int, FILE*);

#endif