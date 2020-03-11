#include <stdio.h>
#include <stdint.h>
#include "safeio.h"

static int endian = -1;

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