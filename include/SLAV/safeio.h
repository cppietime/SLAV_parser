/*
Endian-agnostic I/O functions
*/

#ifndef _H_SAFEIO
#define _H_SAEFIO

#include <stdint.h>
#include <stdio.h>

#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 0

#define TYPE_UNKNOWN 0
#define TYPE_UTF8 1
#define TYPE_UTF16LE 2
#define TYPE_UTF16BE 3
#define TYPE_UTF32LE 4
#define TYPE_UTF32BE 5

int fget_utf_type(FILE *file);
uint32_t sget_unicode(char *src, char **eptr, int type);
uint32_t fget_unicode(FILE *src, int type);
void safe_write(uint64_t, int, int, FILE*);
uint64_t safe_read(int, int, FILE*);
void double_write(double, int, FILE*);
double double_read(int, FILE*);

#endif