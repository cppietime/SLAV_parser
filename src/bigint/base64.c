#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bigint.h"

size_t enc_bytes(size_t bytes){
  if (bytes < 128)
    return 1;
  int bit = 1, bits = 0;
  while(bit < bytes){
    bit <<= 1;
    bits++;
  }
  return 1 + ((bits + 7) >> 3);
}

unsigned char* der_writelen(unsigned char *dst, size_t bytes){
  if(bytes < 128){
    *dst = (unsigned char)bytes;
    return dst + 1;
  }
  int e_bytes = enc_bytes(bytes) - 1;
  *(dst++) = (unsigned char)(0x80 + e_bytes);
  for(int i = 0; i < e_bytes; i ++){
    dst[e_bytes - 1 - i] = (unsigned char)(bytes & 0xff);
    bytes >>= 8;
  }
  return dst + e_bytes;
}

unsigned char* der_bigint(unsigned char *dst, binode_t *bigint){
  if (bigint == NULL){
    *(dst++) = 5; /* NULL */
    *(dst++) = 0; /* EMPTY */
    return dst;
  }
  int bytes = (hibit(bigint->value->digits, bigint_size) + 7) >> 3;
  if (bytes == 0)
    bytes = 1; /* At least one byte to represent 0 */
  *(dst++) = 2; /* INTEGER */
  dst = der_writelen(dst, bytes); /* Write length */
  for(int i = bytes - 1; i >= 0; i--){ /* Write integer in big-endian */
    int digit = i>>2, offset = i&3; /* 4 bytes per bigint dword */
    unsigned char byte = (unsigned char)((bigint->value->digits[digit] >> (offset << 3)) & 0xff);
    *(dst++) = byte;
  }
  return dst;
}

unsigned char* der_read(binode_t *dst, unsigned char *src){
  if(dst != NULL){
    memset(dst->value->digits, 0, sizeof(unsigned long)*bigint_size);
    dst->value->sign = 0;
  }
  int id = *(src++); /* Skip ID, we'll assume it's good */
  if(id == 5){ /* NULL, it's empty */
    return ++src;
  }
  int bytes = *(src++);
  if(bytes >= 128){ /* Long form */
    int ebyt = bytes & 127;
    bytes = 0;
    for(int i=0; i < ebyt; i ++){
      bytes <<= 8;
      bytes |= *(src++);
    }
  }
  if(bigint_size < (bytes + 3) / 4){
    bigint_resize((bytes + 3) / 4, bigfix_point);
  }
  for(int i = bytes - 1; i >= 0; i --){
    unsigned long byte = *(src++);
    size_t digit = i >> 2, offset = i & 3;
    if(dst != NULL){
      dst->value->digits[digit] |= (byte << (offset << 3));
    }
  }
  return src;
}

unsigned char b64_rep(unsigned char byte, unsigned char c62, unsigned char c63){
  byte &= 63;
  if(byte < 26)
    return 'A' + byte;
  if(byte - 26 < 26)
    return 'a' + byte - 26;
  if(byte - 26 - 26 < 10)
    return '0' + byte - 26 - 26;
  if(byte == 62)
    return c62;
  return c63;
}

unsigned char bin_rep(unsigned char byte, unsigned char c62, unsigned char c63){
  if(byte >= 'A' && byte <= 'Z')
    return byte - 'A';
  if(byte >= 'a' && byte <= 'z')
    return (byte - 'a') + 26;
  if(byte >= '0' && byte <= '9')
    return (byte - '0') + 26 + 26;
  if(byte == c62)
    return 62;
  return 63;
}

unsigned char* to_b64(unsigned char *dst, unsigned char *src, size_t len, unsigned char c62, unsigned char c63, unsigned char pad){
  size_t ptr = 0;
  size_t i;
  for(i = 0; i + 2 < len; i += 3){
    unsigned char b0, b1, b2, b3;
    b0 = src[i] >> 2;
    b1 = ((src[i] & 3) << 4) | (src[i+1] >> 4);
    b2 = ((src[i+1] & 15) << 2) | (src[i+2] >> 6);
    b3 = src[i+2] & 63;
    dst[ptr++] = b64_rep(b0, c62, c63);
    dst[ptr++] = b64_rep(b1, c62, c63);
    dst[ptr++] = b64_rep(b2, c62, c63);
    dst[ptr++] = b64_rep(b3, c62, c63);
  }
  unsigned char b0 = 0, b1 = 0, b2 = pad, b3 = pad;
  switch(len - i){
    case 3:
      b0 = src[i] >> 2;
      b1 = ((src[i] & 3) << 4) | (src[i+1] >> 4);
      b2 = ((src[i+1] & 15) << 2) | (src[i+2] >> 6);
      b3 = src[i+2] & 63;
      b0 = b64_rep(b0, c62, c63);
      b1 = b64_rep(b1, c62, c63);
      b2 = b64_rep(b2, c62, c63);
      b3 = b64_rep(b3, c62, c63);
      break;
    case 2:
      b0 = src[i] >> 2;
      b1 = ((src[i] & 3) << 4) | (src[i+1] >> 4);
      b2 = (src[i+1] & 15) << 2;
      b0 = b64_rep(b0, c62, c63);
      b1 = b64_rep(b1, c62, c63);
      b2 = b64_rep(b2, c62, c63);
      break;
    case 1:
      b0 = src[i] >> 2;
      b1 = (src[i] & 3) << 4;
      b0 = b64_rep(b0, c62, c63);
      b1 = b64_rep(b1, c62, c63);
      break;
  }
  if(len > i){
    dst[ptr++] = b0;
    dst[ptr++] = b1;
    dst[ptr++] = b2;
    dst[ptr++] = b3;
  }
  dst[ptr] = 0;
  return dst;
}

unsigned char* from_b64(unsigned char *dst, unsigned char *src, unsigned char c62, unsigned char c63, unsigned char pad){
  size_t len = strlen(src);
  memset(dst, 0, (len + 3)/4*3);
  size_t ptr = 0;
  for(size_t i = 0; i + 3 < len; i += 4){
    unsigned char b0 = bin_rep(src[i], c62, c63)&63, b1 = bin_rep(src[i+1], c62, c63)&63, b2 = 0, b3 = 0;
    dst[ptr++] = (b0 << 2) | (b1 >> 4);
    if(src[i + 2] != pad){
      dst[ptr++] = (b1 << 4);
      b2 = bin_rep(src[i+2], c62, c63)&63;
      dst[ptr-1] |= b2 >> 2;
      if(src[i + 3] != pad){
        dst[ptr++] = b2 << 6;
        b3 = bin_rep(src[i+3], c62, c63)&63;
        dst[ptr-1] |= b3;
      }
    }
  }
  return dst + ptr;
}

int load_b64(unsigned char *dst, FILE *file, unsigned char end){
  int temp;
  do{
    temp = fgetc(file);
  }while(isspace(temp));
  do{
    if( (temp >= 'A' && temp <= 'Z') || (temp >= 'a' && temp <= 'z') || (temp >= '0' && temp <= '9') || temp == '+' || temp == '/' || temp == '=' )
      *(dst++) = temp;
    temp = fgetc(file);
    if(feof(file) || temp == EOF){
      *dst = 0;
      return 1;
    }
  }while(temp != end);
  fseek(file, -1, SEEK_CUR);
  return 1;
}