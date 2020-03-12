#include <stdio.h>
#include <stdint.h>
#include <string.h>
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
			unicode = safe_read(4, LITTLE_ENDIAN, src);
			break;
		}
		case TYPE_UTF32BE:{
			unicode = safe_read(4, BIG_ENDIAN, src);
			break;
		}
		case TYPE_UTF16LE:{
			unicode = safe_read(2, LITTLE_ENDIAN, src);
			if(unicode >= 0xd800 && unicode < 0xdc00){ /* Surrogate */
				int surr = safe_read(2, LITTLE_ENDIAN, src);
				surr -= 0xdc00;
				unicode -= 0xd800;
				unicode = (unicode << 10) | surr;
			}
			break;
		}
		case TYPE_UTF16BE:{
			unicode = safe_read(2, BIG_ENDIAN, src);
			if(unicode >= 0xd800 && unicode < 0xdc00){ /* Surrogate */
				int surr = safe_read(2, BIG_ENDIAN, src);
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
		case TYPE_UTF32LE:
			safe_write(pt, 4, LITTLE_ENDIAN, dst);
			break;
		case TYPE_UTF32BE:
			safe_write(pt, 4, BIG_ENDIAN, dst);
			break;
		case TYPE_UTF16LE:
			if(pt >= 0x10000){
				int hi = (pt >> 10);
				int lo = pt & 0x1ff;
				safe_write(hi + 0xd800, 2, LITTLE_ENDIAN, dst);
				safe_write(lo + 0xdc00, 2, LITTLE_ENDIAN, dst);
			}else{
				safe_write(pt, 2, LITTLE_ENDIAN, dst);
			}
			break;
		case TYPE_UTF16BE:
			if(pt >= 0x10000){
				int hi = (pt >> 10);
				int lo = pt & 0x1ff;
				safe_write(hi + 0xd800, 2, BIG_ENDIAN, dst);
				safe_write(lo + 0xdc00, 2, BIG_ENDIAN, dst);
			}else{
				safe_write(pt, 2, BIG_ENDIAN, dst);
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

void convert_utf(FILE *dst, int dtyp, FILE *src, int styp){
	write_bom(dst, dtyp);
	for(uint32_t point = fget_unicode(src, styp); point != EOF; point = fget_unicode(src, styp)){
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
        endian = LITTLE_ENDIAN;
    }else{
        endian = BIG_ENDIAN;
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
    if(t_end==LITTLE_ENDIAN){
        if(endian==LITTLE_ENDIAN){
            fwrite(&val,1,size,out);
        }else{
            for(int i=sizeof(uint64_t)-1; i>=sizeof(uint64_t)-size; i--){
                fputc(data.bytes[i],out);
            }
        }
    }else{
        if(endian==BIG_ENDIAN){
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
    if(t_end==LITTLE_ENDIAN){
        if(endian==LITTLE_ENDIAN){
            for(int i=0; i<size; i++){
                data.bytes[i] = fgetc(in);
            }
        }else{
            for(int i=sizeof(uint64_t)-1; i>=sizeof(uint64_t)-size; i--){
                data.bytes[i] = fgetc(in);
            }
        }
    }else{
        if(endian==BIG_ENDIAN){
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

