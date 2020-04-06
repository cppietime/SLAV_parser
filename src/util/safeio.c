#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "safeio.h"

static int endian = -1;

int fget_utf_type(FILE *file){
		/* Read BOM */
		static unsigned char bom_buf[4];
		static unsigned char bom_8[3] = {0xef, 0xbb, 0xbf},
												bom_16_le[2] = {0xff, 0xfe},
												bom_16_be[2] = {0xfe, 0xff},
												bom_32_le[4] = {0xff, 0xfe, 0, 0},
												bom_32_be[4] = {0, 0, 0xfe, 0xff};
		memset(bom_buf, 0, 4);
		int utf_type = TYPE_UTF8;
		long start = ftell(file);
		fread(bom_buf, 1, 4, file);
		if(!memcmp(bom_buf, bom_8, 3)){
			utf_type = TYPE_UTF8;
			start += 3;
		}
		else if(!memcmp(bom_buf, bom_16_le, 2)){
			utf_type = TYPE_UTF16LE;
			start += 2;
			if(!memcmp(bom_buf, bom_32_le, 4)){
				utf_type = TYPE_UTF32LE;
				start += 2;
			}
		}
		else if(!memcmp(bom_buf, bom_16_be, 2)){
			utf_type = TYPE_UTF16BE;
			start += 2;
		}
		else if(!memcmp(bom_buf, bom_32_be, 4)){
			utf_type = TYPE_UTF32BE;
			start += 4;
		}
		fseek(file, start, SEEK_SET);
		return utf_type;
}

void write_bom(FILE *dst, int type){
	switch(type){
		case TYPE_UTF8:
			fputc(0xef, dst);
			fputc(0xbb, dst);
			fputc(0xbf, dst);
			break;
		case TYPE_UTF32BE:
			fputc(0, dst);
			fputc(0, dst);
		case TYPE_UTF16BE:
			fputc(0xfe, dst);
			fputc(0xff, dst);
			break;
		case TYPE_UTF16LE:
			fputc(0xff, dst);
			fputc(0xfe, dst);
			break;
		case TYPE_UTF32LE:
			fputc(0xff, dst);
			fputc(0xfe, dst);
			fputc(0, dst);
			fputc(0, dst);
			break;
	}
}

uint32_t sget_unicode(char *src, char **eptr, int type){
	uint32_t uni = 0;
	switch(type){
		case TYPE_BINARY:
			uni = *(src++);
			break;
		case TYPE_UTF8:
		case TYPE_UNKNOWN:{
			int first = *src;
			src++;
			uni = first;
			if((first & 0xe0) == 0xc0){
				int sec = *src++;
				uni = first & 0x1f;
				uni <<= 6;
				uni += sec & 0x3f;
			}
			else if((first & 0xf0) == 0xe0){
				int sec = *src++;
				int third = *src++;
				uni = first & 0x0f;
				uni <<= 6;
				uni += sec & 0x3f;
				uni <<= 6;
				uni += third & 0x3f;
			}
			else if((first & 0xf8) == 0xf0){
				int sec = *src++;
				int third = *src++;
				int fourth = *src++;
				uni = first & 0x07;
				uni <<= 6;
				uni += sec & 0x3f;
				uni <<= 6;
				uni += third & 0x3f;
				uni <<= 6;
				uni += fourth & 0x3f;
			}
			break;
		}
		case TYPE_UTF32LE:{
			uni = (src[0]) | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
			src += 4;
			break;
		}
		case TYPE_UTF32BE:{
			uni = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[0];
			src += 4;
			break;
		}
		case TYPE_UTF16LE:{
			int hi = (src[1] << 8) | src[0];
			src += 2;
			if(hi >= 0xd800 && hi < 0xdc00){ /* Surrogate */
				int lo = (src[1] << 8) | src[0];
				src += 2;
				lo -= 0xdc00;
				hi -= 0xd800;
				uni = (hi << 10) | lo;
			}
			else
				uni = hi;
			break;
		}
		case TYPE_UTF16BE:{
			int hi = (src[0] << 8) | src[1];
			src += 2;
			if(hi >= 0xd800 && hi < 0xdc00){ /* Surrogate */
				int lo = (src[0] << 8) | src[1];
				src += 2;
				lo -= 0xdc00;
				hi -= 0xd800;
				uni = (hi << 10) | lo;
			}
			else
				uni = hi;
			break;
		}
	}
	if(eptr != NULL)
		*eptr = src;
  return uni;
}

uint32_t fget_unicode(FILE *src, int type){
	if(feof(src))
		return EOF;
	if(type == TYPE_UNKNOWN){
		long curpos = ftell(src);
		fseek(src, 0, SEEK_SET);
		type = fget_utf_type(src);
		fseek(src, curpos, SEEK_SET);
	}
	uint32_t unicode = 0;
	static unsigned char uni_buf[4];
	memset(uni_buf, 0, 4);
	switch(type){
		case TYPE_BINARY:
			unicode = fgetc(src);
			break;
		case TYPE_UNKNOWN:
		case TYPE_UTF8:{
			int first = fgetc(src);
			unicode = first; /* ASCII */
			if((first & 0xe0) == 0xc0){ /* 2-byte */
				int second = fgetc(src);
				unicode = (first & 0x1f) << 6;
				unicode |= second & 0x3f;
			}
			else if((first & 0xf0) == 0xe0){ /* 3-byte */
				int second = fgetc(src);
				int third = fgetc(src);
				unicode = (first & 0x0f) << 6;
				unicode |= second & 0x3f;
				unicode <<= 6;
				unicode |= third & 0x3f;
			}
			else if((first & 0xf8) == 0xf0){ /* 4-byte */
				int second = fgetc(src);
				int third = fgetc(src);
				int fourth = fgetc(src);
				unicode = (first & 0x07) << 6;
				unicode |= second & 0x3f;
				unicode <<= 6;
				unicode |= third & 0x3f;
				unicode <<= 6;
				unicode |= fourth & 0x3f;
			}
			break;
		}
		case TYPE_UTF32LE:{
			unicode = safe_read(4, SAFE_LITTLE_ENDIAN, src);
			break;
		}
		case TYPE_UTF32BE:{
			unicode = safe_read(4, SAFE_BIG_ENDIAN, src);
			break;
		}
		case TYPE_UTF16LE:{
			unicode = safe_read(2, SAFE_LITTLE_ENDIAN, src);
			if(unicode >= 0xd800 && unicode < 0xdc00){ /* Surrogate */
				int surr = safe_read(2, SAFE_LITTLE_ENDIAN, src);
				surr -= 0xdc00;
				unicode -= 0xd800;
				unicode = (unicode << 10) | surr;
			}
			break;
		}
		case TYPE_UTF16BE:{
			unicode = safe_read(2, SAFE_BIG_ENDIAN, src);
			if(unicode >= 0xd800 && unicode < 0xdc00){ /* Surrogate */
				int surr = safe_read(2, SAFE_BIG_ENDIAN, src);
				surr -= 0xdc00;
				unicode -= 0xd800;
				unicode = (unicode << 10) | surr;
			}
			break;
		}
	}
	return unicode;
}

void fput_unicode(FILE *dst, int type, uint32_t pt){
	switch(type){
		case TYPE_BINARY:
			fputc(pt, dst);
			break;
		case TYPE_UTF32LE:
			safe_write(pt, 4, SAFE_LITTLE_ENDIAN, dst);
			break;
		case TYPE_UTF32BE:
			safe_write(pt, 4, SAFE_BIG_ENDIAN, dst);
			break;
		case TYPE_UTF16LE:
			if(pt >= 0x10000){
				int hi = (pt >> 10);
				int lo = pt & 0x1ff;
				safe_write(hi + 0xd800, 2, SAFE_LITTLE_ENDIAN, dst);
				safe_write(lo + 0xdc00, 2, SAFE_LITTLE_ENDIAN, dst);
			}else{
				safe_write(pt, 2, SAFE_LITTLE_ENDIAN, dst);
			}
			break;
		case TYPE_UTF16BE:
			if(pt >= 0x10000){
				int hi = (pt >> 10);
				int lo = pt & 0x1ff;
				safe_write(hi + 0xd800, 2, SAFE_BIG_ENDIAN, dst);
				safe_write(lo + 0xdc00, 2, SAFE_BIG_ENDIAN, dst);
			}else{
				safe_write(pt, 2, SAFE_BIG_ENDIAN, dst);
			}
			break;
		case TYPE_UTF8:{
			if(pt < 0x80){
				fputc(pt, dst);
			}
			else if(pt < 0x7ff){
				fputc(0xc0 | (pt >> 6), dst);
				fputc(0x80 | (pt & 0x3f), dst);
			}
			else if(pt < 0xffff){
				fputc(0xe0 | (pt >> 12), dst);
				fputc(0x80 | ((pt >> 6) & 0x3f), dst);
				fputc(0x80 | (pt & 0x3f), dst);
			}else{
				fputc(0xf0 | (pt >> 18), dst);
				fputc(0x80 | ((pt >> 12) & 0x3f), dst);
				fputc(0x80 | ((pt >> 6) & 0x3f), dst);
				fputc(0x80 | (pt & 0x3f), dst);
			}
			break;
		}
	}
}

char* sput_unicode(char *dst, int type, uint32_t pt){
	switch(type){
		case TYPE_BINARY:
			*(dst++) = pt;
			break;
		case TYPE_UTF32LE:
			*(dst++) = pt & 0xff;
			*(dst++) = (pt >> 8) & 0xff;
			*(dst++) = (pt >> 16) & 0xff;
			*(dst++) = (pt >> 24) & 0xff;
			break;
		case TYPE_UTF32BE:
			*(dst++) = (pt >> 24) & 0xff;
			*(dst++) = (pt >> 16) & 0xff;
			*(dst++) = (pt >> 8) & 0xff;
			*(dst++) = pt & 0xff;
			break;
		case TYPE_UTF16LE:
			if(pt >= 0x10000){
				int hi = (pt >> 10);
				int lo = pt & 0x1ff;
				hi += 0xd800;
				lo += 0xdc00;
				*(dst++) = hi & 0xff;
				*(dst++) = (hi >> 8) & 0xff;
				*(dst++) = lo & 0xff;
				*(dst++) = (lo >> 8) & 0xff;
			}else{
				*(dst++) = pt & 0xff;
				*(dst++) = (pt >> 8) & 0xff;
			}
			break;
		case TYPE_UTF16BE:
			if(pt >= 0x10000){
				int hi = (pt >> 10);
				int lo = pt & 0x1ff;
				hi += 0xd800;
				lo += 0xdc00;
				*(dst++) = (hi >> 8) & 0xff;
				*(dst++) = hi & 0xff;
				*(dst++) = (lo >> 8) & 0xff;
				*(dst++) = lo & 0xff;
			}else{
				*(dst++) = (pt >> 8) & 0xff;
				*(dst++) = pt & 0xff;
			}
			break;
		case TYPE_UTF8:{
			if(pt < 0x80){
				*(dst++) = pt;
			}
			else if(pt < 0x7ff){
				*(dst++) = 0xc0 | (pt >> 6);
				*(dst++) = 0x80 | (pt & 0x3f);
			}
			else if(pt < 0xffff){
				*(dst++) = 0xe0 | (pt >> 12);
				*(dst++) = 0x80 | ((pt >> 6) & 0x3f);
				*(dst++) = 0x80 | (pt & 0x3f);
			}else{
				*(dst++) = 0xf0 | (pt >> 18);
				*(dst++) = 0x80 | ((pt >> 12) & 0x3f);
				*(dst++) = 0x80 | ((pt >> 6) & 0x3f);
				*(dst++) = 0x80 | (pt & 0x3f);
			}
			break;
		}
	}
	return dst;
}

void wstrstr(char *str, uint32_t *wstr){
	int i = 0;
	while(wstr[i]){
		str[i] = wstr[i];
		i++;
	}
	str[i] = 0;
}

void wstrcpy(uint32_t *dst, uint32_t *src){
  for(uint32_t *ptr = src; *ptr; ptr++){
    *(dst++) = *ptr;
  }
  *dst = 0;
}

void wstrncpy(uint32_t *dst, uint32_t *src, size_t n){
  int i;
  for(i=0; src[i] && i < n; i++){
    dst[i] = src[i];
  }
  dst[i] = 0;
}

int wstrncmp(uint32_t *a, uint32_t *b, size_t n){
  for(int i = 0; i < n; i++){
    int dif = (int)(a[i]) - (int)(b[i]);
    if(dif)
      return dif;
  }
  return 0;
}

void wstrpad(uint32_t *dst, char *src){
	size_t len = strlen(src);
	for(size_t i = 0; i < len + 1; i++){
		dst[i] = src[i];
	}
}

void wstrpluck(char *dst, uint32_t *src){
	size_t len = wstrlen(src);
	for(size_t i = 0; i < len + 1; i++){
		dst[i] = src[i];
	}
}

void pututf8(FILE *file, uint32_t pt){
	if(pt < 128)
		fputc(pt, file);
	else if(pt < 2048){
		fputc(0xc0 | (pt >> 6), file);
		fputc(0x80 | (pt & 0x3f), file);
	}
	else if(pt < 65536){
		fputc(0xe0 | (pt >> 12), file);
		fputc(0x80 | ((pt >> 6) & 0x3f), file);
		fputc(0x80 | (pt & 0x3f), file);
	}
	else{
		fputc(0xf0 | (pt >> 18), file);
		fputc(0x80 | ((pt >> 12) & 0x3f), file);
		fputc(0x80 | ((pt >> 6) & 0x3f), file);
		fputc(0x80 | (pt & 0x3f), file);
	}
}

void fprintw(FILE *file, uint32_t *wstr){
  for(uint32_t *ptr = wstr; *ptr; ptr++){
		if(*ptr <= 127)
			fputc(*ptr, file);
		else if(file == stdout || file == stderr)
			fprintf(file, "[\\x%x]", *ptr);
		else{
			pututf8(file, *ptr);
		}
  }
}

void fprintwn(FILE *file, uint32_t *wstr, size_t n){
  for(uint32_t *ptr = wstr; *ptr && ptr < wstr + n; ptr++){
		if(*ptr <= 127 && *ptr != '\n' && *ptr != '\r')
			fputc(*ptr, file);
		else if(file == stdout || file == stderr)
			fprintf(file, "[\\x%x]", *ptr);
		else{
			pututf8(file, *ptr);
		}
  }
}

long wstrtol(uint32_t *wstr, uint32_t **end, int radix){
  static char onechar[2] = {0, 0};
  long ret = 0;
  uint32_t *ptr;
  for(ptr = wstr; *ptr; ptr++){
		onechar[0] = *ptr;
		if( (*ptr >= '0' && *ptr <= '9') || (*ptr >= 'A' && *ptr <= 'F') || (*ptr >= 'a' && *ptr <= 'f') ){
			ret *= radix;
			ret += strtol(onechar, NULL, radix);
		}
		else break;
  }
  if(end != NULL)
    *end = ptr;
	return ret;
}

size_t wstrlen(uint32_t *wstr){
	size_t i;
	for(i = 0; wstr[i]; i++);
	return i;
}

void convert_utf(FILE *dst, int dtyp, FILE *src, int styp){
	write_bom(dst, dtyp);
	for(uint32_t point = fget_unicode(src, styp); point != (uint32_t)EOF && !feof(src); point = fget_unicode(src, styp)){
		fput_unicode(dst, dtyp, point);
	}
}

void check_endian(){
    union {
        uint32_t big;
        uint8_t small[4];
    } val;
    val.big = 1;
    if(val.small[0] == 1){
        endian = SAFE_LITTLE_ENDIAN;
    }else{
        endian = SAFE_BIG_ENDIAN;
    }
}

void safe_write(uint64_t val, int size, int t_end, FILE* out){
    union {
        uint64_t bigval;
        uint8_t bytes[8];
    } data;
    data.bigval = val;
    if(endian==-1){
        check_endian();
    }
    if(t_end==SAFE_LITTLE_ENDIAN){
        if(endian==SAFE_LITTLE_ENDIAN){
            fwrite(&val,1,size,out);
        }else{
            for(int i=sizeof(uint64_t)-1; i>=sizeof(uint64_t)-size; i--){
                fputc(data.bytes[i],out);
            }
        }
    }else{
        if(endian==SAFE_BIG_ENDIAN){
            fwrite(&(data.bytes[sizeof(uint64_t)-size]),1,size,out);
        }else{
            for(int i=size-1; i>=0; i--){
                fputc(data.bytes[i],out);
            }
        }
    }
}

uint64_t safe_read(int size, int t_end, FILE* in){
    union {
        uint64_t bigval;
        uint8_t bytes[8];
    } data;
    data.bigval = 0;
    if(endian==-1){
        check_endian();
    }
    if(t_end==SAFE_LITTLE_ENDIAN){
        if(endian==SAFE_LITTLE_ENDIAN){
            for(int i=0; i<size; i++){
                data.bytes[i] = fgetc(in);
            }
        }else{
            for(int i=sizeof(uint64_t)-1; i>=sizeof(uint64_t)-size; i--){
                data.bytes[i] = fgetc(in);
            }
        }
    }else{
        if(endian==SAFE_BIG_ENDIAN){
            for(int i=sizeof(uint64_t)-size; i<sizeof(uint64_t); i++){
                data.bytes[i] = fgetc(in);
            }
        }else{
            for(int i=size-1; i>=0; i--){
                data.bytes[i] = fgetc(in);
            }
        }
    }
    return data.bigval;
}

void double_write(double val, int t_end, FILE* out){
    uint64_t lon = *(uint64_t*)&val;
    safe_write(lon, 8, t_end, out);
}

double double_read(int t_end, FILE* in){
    uint64_t lon = safe_read(8, t_end, in);
    double val = *(double*)&lon;
    return val;
}

size_t bin_fgets(char *buffer, size_t len, FILE *src){
	size_t read = 0;
	while(!feof(src) && read < len){
		char ch = fgetc(src);
		if(ch == EOF)
			break;
		buffer[read++] = ch;
		if(ch == '\n' || ch == '\n')
			break;
	}
	buffer[read] = 0;
	return read;
}

size_t wcommon_len(uint32_t *a, uint32_t *b){
	size_t i;
	for(i = 0; a[i] != 0 && b[i] != 0; i++){
		if(a[i] != b[i])
			break;
	}
	return i;
}

size_t* wstrpos_table(uint32_t *needle){
	size_t len = wstrlen(needle);
	size_t *jumps = malloc(sizeof(size_t) * len);
	jumps[0] = 0;
	for(size_t i = 1; i < len; i++){
		size_t c = i;
		while(c){
			if(needle[i] == needle[jumps[c - 1]]){
				jumps[i] = jumps[c - 1] + 1;
				break;
			}
			else{
				c = jumps[c - 1];
			}
		}
		if(c == 0){
			if(needle[i] == needle[0])
				jumps[i] == 1;
			else
				jumps[i] == 0;
		}
	}
	return jumps;
}

uint32_t *wstrpos(uint32_t *haystack, uint32_t *needle){
	if(haystack == NULL || needle == NULL)
		return NULL;
	size_t hlen = wstrlen(haystack);
	size_t nlen = wstrlen(needle);
	if(hlen == 0 || nlen == 0)
		return NULL;
	size_t *jumps = wstrpos_table(needle);
	size_t n_i = 0;
	size_t h_i = 0;
	uint32_t *match = NULL;
	while(n_i + nlen <= hlen){
		size_t common = h_i + wcommon_len(haystack + n_i + h_i, needle + h_i);
		if(common == nlen){
			match = haystack + n_i;
			break;
		}
		else if(common == 0){
			n_i += 1;
			h_i = 0;
		}
		else{
			n_i += common - jumps[common - 1];
			h_i = jumps[common - 1];
		}
	}
	free(jumps);
	return match;
}