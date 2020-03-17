/*
Endian-agnostic I/O functions
*/

#ifndef _H_SAFEIO
#define _H_SAEFIO

#include <stdint.h>
#include <stdio.h>

#define SAFE_BIG_ENDIAN 1
#define SAFE_LITTLE_ENDIAN 0

#define TYPE_UNKNOWN 0
#define TYPE_UTF8 1
#define TYPE_UTF16LE 2
#define TYPE_UTF16BE 3
#define TYPE_UTF32LE 4
#define TYPE_UTF32BE 5

int fget_utf_type(FILE *file);
uint32_t sget_unicode(char *src, char **eptr, int type);
uint32_t fget_unicode(FILE *src, int type);
void fput_unicode(FILE *src, int type, uint32_t val);
void convert_utf(FILE *dst, int dtyp, FILE *src, int styp);
void wstrstr(char *str, uint32_t *wstr); /* Copy wide to normal */
void wstrcpy(uint32_t *dst, uint32_t *src); /* Copy unicode string */
void wstrncpy(uint32_t *dst, uint32_t *src, size_t n); /* Copy unicode string */
int wstrncmp(uint32_t *dst, uint32_t *src, size_t n); /* Copy unicode string */
long wstrtol(uint32_t *wstr, uint32_t **end, int radix); /* Unicode string to long int */
void wstrpad(uint32_t *dst, char *src); /* Char to uni and back */
void wstrpluck(char *dst, uint32_t *src);
void fprintw(FILE *file, uint32_t *wstr); /* Prints a unicode string as normal string to file */
void fprintwn(FILE *file, uint32_t *wstr, size_t n); /* Prints a unicode string as normal string to file with a limit */
size_t wstrlen(uint32_t *wstr); /* Length of wide string */
void safe_write(uint64_t, int, int, FILE*);
uint64_t safe_read(int, int, FILE*);
void double_write(double, int, FILE*);
double double_read(int, FILE*);
size_t bin_fgets(char *buffer, size_t len, FILE *src);

#endif