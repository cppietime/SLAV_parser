/*
Some functions for randomness/security shit.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include "bigint.h"

#define TEST_PRIMES 64
static digit_t first_primes[TEST_PRIMES] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59,
                                                  61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137,
                                                  139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227,
                                                  229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313};

void seedrand(){
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  unsigned int seed = 11 * now.tv_sec + 7 * now.tv_nsec;
  seed += (unsigned int)&seed;
  srand(seed);
}

void randbits(digit_t *arr, size_t len, int bits){
  memset(arr, 0, sizeof(digit_t)*len);
  size_t digit = 0;
  int offset = 0;
  while(bits > 0 && digit < len){
    digit_t randbit = rand() & 255;
    if(bits < 32)
      randbit &= (1<<(bits))-1;
    arr[digit] |= randbit << offset;
    offset += 8;
    if(offset >= 32){
      offset %= 32;
      digit++;
    }
    bits -= 8;
  }
}

void arrexpmod(digit_t *dst, digit_t *base, digit_t *pow, digit_t *mod, size_t len, digit_t *work){
  digit_t *factor = work + len * 8;
  digit_t *accum = work + len * 9;
  digit_t *tpow = work + len * 10;
  digit_t *sfac = work + len * 11;
  digit_t *mulwork = work;
  memcpy(factor, base, sizeof(digit_t)*len);
  memset(accum, 0, sizeof(digit_t)*len);
  accum[0] = 1;
  memcpy(tpow, pow, sizeof(digit_t)*len);
  digit_t one = 1;
  while(hibit(tpow, len)>0){
    if((tpow[0] & 1) == 0){ //Square  factor
      memcpy(sfac, factor, sizeof(digit_t)*len);
      kara_nat(factor, sfac, len, mulwork);
      if(hibit(sfac, len)){
        fprintf(stderr, "Warning! Multiplication overflow!\n");
        farrprint(stderr, sfac, len);printf("\n");
      }
      if(mod != NULL){
        arrdivmod(NULL, factor, factor, mod, len, mulwork);
      }
      arrshf(tpow, len, -1);
    }
    if(tpow[0]&1){ //Multiply accum
      memcpy(sfac, factor, sizeof(digit_t)*len);
      kara_nat(accum, sfac, len, mulwork);
      if(hibit(sfac, len)){
        fprintf(stderr, "Warning! Multiplication overflow!\n");
        farrprint(stderr, sfac, len);printf("\n");
      }
      if(mod != NULL){
        arrdivmod(NULL, accum, accum, mod, len, mulwork);
      }
      tpow[0] &= ~1L;
    }
  }
  memcpy(dst, accum, sizeof(digit_t)*len);
}

void ipowmod(binode_t *dst, binode_t *base, binode_t *pow, binode_t *mod){
  digit_t *msrc = NULL;
  if(mod != NULL)
    msrc = mod->value->digits;
  if(pow->value->sign){
    fprintf(stderr, "Warning: Ignoring sign of power in big-integer exponentiation!\n");
  }
  if(mod != NULL && mod->value->sign){
    fprintf(stderr, "Warning: Ignoring sign of modulus in big-integer exponentiation!\n");
  }
  int sign = base->value->sign && (pow->value->digits[0] & 1);
  arrexpmod(dst->value->digits, base->value->digits, pow->value->digits, msrc, bigint_size, scrap);
  dst->value->sign = sign;
}

int miller_rabin(digit_t *num, size_t len, int tries, digit_t *work){
  digit_t *ework = work;
  digit_t *attempt = work + len * 12;
  digit_t *odd = work + len *13;
  digit_t *debug = work + len * 14;
  digit_t two = 2;
  digit_t one = 1;
  // Test against first someodd primes
  memcpy(odd, num, sizeof(digit_t)*len);
  memset(debug, 0, sizeof(digit_t)*len);
  for(int i=0; i<TEST_PRIMES; i++){
    debug[0] = first_primes[i];
    arrdivmod(NULL, attempt, odd, debug, len, ework);
    if(attempt[0] == 0){
      return 0;
    }
  }
  // Now do MR
  odd[0] &= ~1;
  digit_t twopow = 0;
  while((odd[0] & 1) == 0){
    twopow++;
    arrshf(odd, len, -1);
  }
  for(int i = 0; i < tries; i++){
    randbits(attempt, len, len*32);
    arradd(attempt, attempt, len, &two, 1);
    arrdivmod(NULL, attempt, attempt, num, len, ework);
    memcpy(debug, attempt, sizeof(digit_t)*len);
    arrexpmod(attempt, attempt, odd, num, len, ework);
    if(hibit(attempt, len) == 1){
      continue;
    }
    arradd(attempt, attempt, len, &one, 1);
    if(arrcmp(attempt, num, len) == 0){
      continue;
    }
    arrsub(attempt, attempt, len, &one, 1);
    int isprime = 0;
    for(digit_t j = 0; j < twopow; j ++){
      memcpy(ework + len*8, attempt, sizeof(digit_t)*len);
      kara_nat(attempt, ework+len*8, len, ework);
      arrdivmod(NULL, attempt, attempt, num, len, ework); //Square attempt mod num 
      arradd(attempt, attempt, len, &one, 1);
      if(arrcmp(attempt, num, len) == 0){
        isprime = 1;
        break;
      }
      arrsub(attempt, attempt, len, &one, 1);
    }
    if(!isprime){
      return 0;
    }
  }
  return 1;
}

#undef TEST_PRIMES

int arrprime(digit_t *arr, size_t len, int tries, int acc, int pow){
  for(int i = 0; i < tries; i ++){
    randbits(arr, len, pow-1);
    arr[pow/32] |= 1 << (pow%32);
    arr[0] |= 1;
    if(miller_rabin(arr, len, acc, scrap)){
      return i;
    }
  }
  return -1;
}

int probprime(binode_t *num, int pow){
  return arrprime(num->value->digits, bigint_size, 1000, 40, pow);
}

/*
Extended Euclid's algorithm.
Pass (GCD, ..., a, b) for GCD(a,b) to be input to gcd.
Pass (..., INV, m, b) for multiplicative inverse of b mod m.
*/
void ext_euclid(binode_t *gcd, binode_t *inv, binode_t *a, binode_t *b){
  /* rm2 = GCD; tm1 = inv */
  static binode_t *q = NULL, *rm1 = NULL, *rm2 = NULL, *sm1 = NULL, *sm2 = NULL, *tm1 = NULL, *tm2 = NULL, *tmp = NULL;
  if(q == NULL){
    q = bigint_link();
    rm1 = bigint_link();
    rm2 = bigint_link();
    sm1 = bigint_link();
    sm2 = bigint_link();
    tm1 = bigint_link();
    tm2 = bigint_link();
    tmp = bigint_link();
  }
  bicpy(rm2, a);
  bicpy(rm1, b);
  ltobi(sm2, 1);
  ltobi(tm1, 1);
  ltobi(tm2, 0);
  
  while(1){
    /* Division + Modulus */
    bicpy(tmp, rm1);
    idivmod(q, rm1, rm2, rm1);
    bicpy(rm2, tmp);
    /* S(i) = S(i-2) - q * S(i-1) */
    ip_kara(tmp, sm1, q);
    bisub(tmp, sm2, tmp);
    bicpy(sm2, sm1);
    bicpy(sm1, tmp);
    /* T(i) = T(i-2) - q * T(i-1) */
    ip_kara(tmp, tm1, q);
    bisub(tmp, tm2, tmp);
    bicpy(tm2, tm1);
    bicpy(tm1, tmp);
    /* Check if rm1 0 */
    if(!hibit(rm1->value->digits, bigint_size)){
      while(tm2->value->sign){
        biadd(tm2, tm2, a);
      }
      break;
    }
  }
  if(gcd != NULL)
    bicpy(gcd, rm2);
  if(inv != NULL)
    bicpy(inv, tm2);
}

int rsa_keygen(rsakey_t *key, size_t bits){
  static binode_t *totient = NULL, *gcd  = NULL;
  static digit_t one = 1;
  if(totient == NULL){
    totient = bigint_link();
    gcd = bigint_link();
  }
  int pbits = bits / 2;
  if(bigint_size * 32 < pbits * 4){
    bigint_resize((pbits + 7) / 8, bigfix_point);
  }
  if(probprime(key->prime_a, pbits - 1)==-1){
    return 0;
  }
  if(probprime(key->prime_b, bits - pbits)==-1){
    return 0;
  }
  ip_kara(key->modulus, key->prime_a, key->prime_b);
  int mbit = hibit(key->modulus->value->digits, bigint_size);
  if(mbit * 2 > bigint_size * 32){
    bigint_resize((mbit + 15) / 16, bigfix_point);
  }
  bisub(totient, key->modulus, key->prime_a);
  bisub(totient, totient, key->prime_b);
  arradd(totient->value->digits, totient->value->digits, bigint_size, &one, 1);
  arrsub(key->prime_a->value->digits, key->prime_a->value->digits, bigint_size, &one, 1);
  arrsub(key->prime_b->value->digits, key->prime_b->value->digits, bigint_size, &one, 1);
  ext_euclid(gcd, NULL, key->prime_a, key->prime_b);
  idivmod(totient, NULL, totient, gcd);
  while(1){
    // int fbit = (rand() % (bits*1/4)) + 4;
    int fbit = 2;
    for(fbit = 4; fbit < 16; fbit++){
      if(rand() % 10 == 0)
        break;
    }
    if(fbit == 16){
      for(; fbit < pbits - 1; fbit++){
        if(rand() % 4 == 0)
          break;
      }
    }
    memset(key->pub_exp->value->digits, 0, sizeof(digit_t)*bigint_size);
    key->pub_exp->value->digits[fbit/32] = 1 << (fbit%32);
    key->pub_exp->value->digits[0] |= 1;
    ext_euclid(gcd, key->pri_exp, totient, key->pub_exp);
    if(hibit(gcd->value->digits, bigint_size) == 1){
      break;
    }
  }
  if(key->expmod_a != NULL){
    idivmod(NULL, key->expmod_a, key->pri_exp, key->prime_a);
  }
  if(key->expmod_b != NULL){
    idivmod(NULL, key->expmod_b, key->pri_exp, key->prime_b);
  }
  arradd(key->prime_a->value->digits, key->prime_a->value->digits, bigint_size, &one, 1);
  arradd(key->prime_b->value->digits, key->prime_b->value->digits, bigint_size, &one, 1);
  if(key->q_inv != NULL){
    ext_euclid(NULL, key->q_inv, key->prime_a, key->prime_b);
  }
}

void rsa_printkey(FILE *file, rsakey_t *key){
  if(key->modulus != NULL){
    fprintf(file, "modulus = ");farrprint(file, key->modulus->value->digits, bigint_size);printf(";\n");
  }
  if(key->prime_a != NULL){
    fprintf(file, "prime_a = ");farrprint(file, key->prime_a->value->digits, bigint_size);printf(";\n");
  }
  if(key->prime_b != NULL){
    fprintf(file, "prime_b = ");farrprint(file, key->prime_b->value->digits, bigint_size);printf(";\n");
  }
  if(key->pub_exp != NULL){
    fprintf(file, "pub_exp = ");farrprint(file, key->pub_exp->value->digits, bigint_size);printf(";\n");
  }
  if(key->pri_exp != NULL){
    fprintf(file, "pri_exp = ");farrprint(file, key->pri_exp->value->digits, bigint_size);printf(";\n");
  }
  if (key->expmod_a != NULL){
    fprintf(file, "expmod_a = ");farrprint(file, key->expmod_a->value->digits, bigint_size);printf(";\n");
  }
  if (key->expmod_b != NULL){
    fprintf(file, "expmod_b = ");farrprint(file, key->expmod_b->value->digits, bigint_size);printf(";\n");
  }
  if (key->q_inv != NULL){
    fprintf(file, "q_inv = ");farrprint(file, key->q_inv->value->digits, bigint_size);printf(";\n");
  }
}

void rsa_encrypt(rsakey_t *key, binode_t *ciphertext, binode_t *plaintext){
  ipowmod(ciphertext, plaintext, key->pub_exp, key->modulus);
}

void rsa_decrypt(rsakey_t *key, binode_t *plaintext, binode_t *ciphertext){
  static binode_t *m1 = NULL, *m2 = NULL, *h = NULL;
  if(key->expmod_a == NULL || key->expmod_b == NULL || key->q_inv == NULL){
    ipowmod(plaintext, ciphertext, key->pri_exp, key->modulus);
    return;
  }
  if(m1 == NULL){
    m1 = bigint_link();
    m2 = bigint_link();
    h = bigint_link();
  }
  ipowmod(m1, ciphertext, key->expmod_a, key->prime_a);
  ipowmod(m2, ciphertext, key->expmod_b, key->prime_b);
  bisub(m1, m1, m2);
  ip_kara(h, key->q_inv, m1);
  idivmod(NULL, h, h, key->prime_a);
  ip_kara(h, h, key->prime_b);
  biadd(plaintext, m2, h);
}

void rsa_writekey(unsigned char *private, unsigned char *public, unsigned char **pri_end, unsigned char **pub_end, rsakey_t *key){
  /* Write private key */
  if (private != NULL){
    *private = 0x30;
    unsigned char *ptr = private + 2;
    *(ptr++) = 2;
    *(ptr++) = 1;
    *(ptr++) = 0; /* Write the version */
    ptr = der_bigint(ptr, key->modulus);
    ptr = der_bigint(ptr, key->pub_exp);
    ptr = der_bigint(ptr, key->pri_exp);
    ptr = der_bigint(ptr, key->prime_a);
    ptr = der_bigint(ptr, key->prime_b);
    ptr = der_bigint(ptr, key->expmod_a);
    ptr = der_bigint(ptr, key->expmod_b);
    ptr = der_bigint(ptr, key->q_inv);
    size_t total_len = ptr - (private + 2);
    if(pri_end != NULL)
      *pri_end = ptr;
    if (total_len >= 128) {
      size_t elen = enc_bytes(total_len);
      memmove(private + elen + 1, private + 2, total_len);
      if(pri_end != NULL)
        *pri_end += elen - 1;
    }
    der_writelen(private + 1, total_len);
  }
  /* Write public key */
  if (public != NULL){
    *public = 0x30;
    unsigned char *ptr = public + 2;
    ptr = der_bigint(ptr, key->modulus);
    ptr = der_bigint(ptr, key->pub_exp);
    size_t total_len = ptr - (public + 2);
    if(pub_end != NULL)
      *pub_end = ptr;
    if (total_len >= 128){
      size_t elen = enc_bytes(total_len);
      memmove(public + elen + 1, public + 2, total_len);
      if(pub_end != NULL)
        *pub_end += elen - 1;
    }
    der_writelen(public + 1, total_len);
  }
}

unsigned char* rsa_readkey_private(rsakey_t *key, unsigned char *src){
  src++; /* Skip ID */
  int bytes = *(src++);
  if(bytes >= 128){
    bytes &= 127;
    src += bytes;
  }
  /* Length ID skipped */
  src = der_read(NULL, src); /* skip version */
  src = der_read(key->modulus, src);
  src = der_read(key->pub_exp, src);
  src = der_read(key->pri_exp, src);
  src = der_read(key->prime_a, src);
  src = der_read(key->prime_b, src);
  src = der_read(key->expmod_a, src);
  src = der_read(key->expmod_b, src);
  src = der_read(key->q_inv, src);
  return src;
}

unsigned char* rsa_readkey_public(rsakey_t *key, unsigned char *src){
  src++; /* Skip ID */
  int bytes = *(src++);
  if(bytes >= 128){
    bytes &= 127;
    src += bytes;
  }
  /* Length ID skipped */
  src = der_read(key->modulus, src);
  src = der_read(key->pub_exp, src);
  return src;
}

int rsakey_dump_private(FILE *file, rsakey_t *key){
  fputs("-----BEGIN RSA PRIVATE KEY-----\n", file);
  unsigned char *buffer = malloc(bigint_size * sizeof(digit_t) * 8), *bptr;
  rsa_writekey(buffer, NULL, &bptr, NULL, key);
  size_t hex_len = (bptr - buffer);
  size_t b64_len = (hex_len + 2)/3*4;
  unsigned char *b64_buf = malloc(b64_len + 1);
  to_b64(b64_buf, buffer, hex_len, '+', '/', '=');
  fprintf(file, "%s\n", b64_buf);
  free(b64_buf);
  free(buffer);
  fputs("-----END RSA PRIVATE KEY-----\n", file);
  return 1;
}

int rsakey_dump_public(FILE *file, rsakey_t *key){
  fputs("-----BEGIN RSA PUBLIC KEY-----\n", file);
  unsigned char *buffer = malloc(bigint_size * sizeof(digit_t) * 3), *bptr;
  rsa_writekey(NULL, buffer, NULL, &bptr, key);
  size_t hex_len = (bptr - buffer);
  size_t b64_len = (hex_len + 2)/3*4;
  unsigned char *b64_buf = malloc(b64_len + 1);
  to_b64(b64_buf, buffer, hex_len, '+', '/', '=');
  fprintf(file, "%s\n", b64_buf);
  free(b64_buf);
  free(buffer);
  fputs("-----END RSA PUBLIC KEY-----\n", file);
  return 1;
}

int rsakey_load_private(rsakey_t *key, FILE *file){
  long fail_pos = ftell(file);
  char delim[33];
  fgets(delim, 33, file);
  if(strncmp(delim, "-----BEGIN RSA PRIVATE KEY-----", 31)){
    fseek(file, fail_pos, SEEK_SET);
    return 0;
  }
  unsigned char *buffer = malloc((bigint_size * sizeof(digit_t) * 8 + 2) / 3 * 4 + 1);
  if(!load_b64(buffer, file, '-')){
    fseek(file, fail_pos, SEEK_SET);
    free(buffer);
    return 0;
  }
  fgets(delim, 33, file);
  if(strncmp(delim, "-----END RSA PRIVATE KEY-----", 29)){
    fseek(file, fail_pos, SEEK_SET);
    free(buffer);
    return 0;
  }
  size_t b64_len = strlen(buffer);
  unsigned char *hex_buf = malloc((b64_len + 3)/4 * 3);
  from_b64(hex_buf, buffer, '+', '/', '=');
  free(buffer);
  rsa_readkey_private(key, hex_buf);
  free(hex_buf);
  return 1;
}

int rsakey_load_public(rsakey_t *key, FILE *file){
  long fail_pos = ftell(file);
  char delim[33];
  fgets(delim, 33, file);
  if(strncmp(delim, "-----BEGIN RSA PUBLIC KEY-----", 30)){
    fseek(file, fail_pos, SEEK_SET);
    return 0;
  }
  unsigned char *buffer = malloc((bigint_size * sizeof(digit_t) * 3 + 2) / 3 * 4 + 1);
  if(!load_b64(buffer, file, '-')){
    fseek(file, fail_pos, SEEK_SET);
    free(buffer);
    return 0;
  }
  fgets(delim, 33, file);
  if(strncmp(delim, "-----END RSA PUBLIC KEY-----", 28)){
    fseek(file, fail_pos, SEEK_SET);
    free(buffer);
    return 0;
  }
  size_t b64_len = strlen(buffer);
  unsigned char *hex_buf = malloc((b64_len + 3)/4 * 3);
  from_b64(hex_buf, buffer, '+', '/', '=');
  free(buffer);
  rsa_readkey_public(key, hex_buf);
  free(hex_buf);
  return 1;
}

int rsakey_load(rsakey_t *key, FILE *file){
  if(rsakey_load_private(key, file)){
    return 1;
  }
  if(rsakey_load_public(key, file)){
    return 1;
  }
  return 0;
}

void rsakey_init(rsakey_t *key, int type){
  key->modulus = bigint_link();
  key->pub_exp = bigint_link();
  if(type == RSA_KEY_PRIVATE){
    key->pri_exp = bigint_link();
    key->prime_a = bigint_link();
    key->prime_b = bigint_link();
    key->expmod_a = bigint_link();
    key->expmod_b = bigint_link();
    key->q_inv = bigint_link();
  }
}
