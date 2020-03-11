#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include "bigint.h"

#define SIZE_MUL 15

size_t bigint_size = 1;
size_t bigfix_point = 0;
binode_t* bigint_head = NULL;
unsigned long *scrap = NULL,
    *nttspace = NULL,
    *ssmspace = NULL,
    *naispace = NULL;

unsigned long n2n5(unsigned long n){
    unsigned long n2 = 1L<<n;
    while(n%2==0){
        n>>=1;
    }
    n2 *= n;
    if(n2%5!=0) n2 *= 5;
    return n2;
}

void farrprint(FILE *file, unsigned long* digs, size_t len){
    fprintf(file, "0x");
    for(int i=len-1; i>=0; i--){
        fprintf(file, "%.8x", digs[i]);
    }
}

void sarrprint(char *dst, unsigned long* digs, size_t len){
    sprintf(dst, "0x");
    for(int i=len-1; i>=0; i--){
        sprintf(dst, "%.8x", digs[i]);
    }
}

void needed_space(size_t size){
    if(scrap != NULL){
        free(scrap);
    }
    if(nttspace != NULL){
        free(nttspace);
    }
    if(ssmspace != NULL){
        free(ssmspace);
    }
    if(naispace != NULL){
        free(naispace);
    }
    naispace = malloc(sizeof(unsigned long)*size*2);
    size_t kara_space = size*SIZE_MUL;
    scrap = malloc(sizeof(unsigned long)*kara_space);
    size_t ntt_space = 0;
    size_t mul_space = 0;
    int ct=0;
    unsigned long lbits = size<<5;
    while(1){
        unsigned long bits = 1;
        unsigned long x = 0;
        while(bits<lbits){
            x++;
            bits<<=1;
        }
        unsigned long n = x>>1, k = x-n, n2 = n2n5(n), y = n + (2<<k);
        y += (n2 - (y%n2))%n2;
        unsigned long Y = 1;
        while(Y<y)Y<<=1;
        if(ct==0){
            ntt_space = Y>>2;
            ct=1;
        }
        if(n<7)n=7;
        mul_space += sizeof(unsigned long)*4*Y<<(n-5);
        if(Y<=32)break;
        if(Y>=lbits)break;
        lbits = Y;
    }
}

void bigint_init(size_t size){
    size_t digs = 1;
    while(digs<size)digs<<=1;
    if(digs==bigint_size)return;
    needed_space(digs);
    binode_t* node = bigint_head;
    while(node!=NULL){
        bigint_t* old = node->value;
        bigint_t* new = calloc(sizeof(bigint_t) + sizeof(unsigned long)*digs, 1);
        node->value = new;
        new->sign = old->sign;
        memcpy(new->digits, old->digits, sizeof(unsigned long)*bigint_size);
        free(old);
        node = node->next;
    }
    bigint_size = digs;
    setup_consts();
}

void bigint_resize(size_t n, size_t f){
    size_t digs = 1;
    while(digs<n)digs<<=1;
    if(digs != bigint_size || f != bigfix_point){
        if(digs != bigint_size){
            needed_space(digs);
        }
        binode_t* node = bigint_head;
        while(node!=NULL){
            int offset = f - bigfix_point;
            bigint_t* old = node->value;
            bigint_t* new = calloc(sizeof(bigint_t) + sizeof(unsigned long)*digs, 1);
            unsigned long* interim = calloc(sizeof(unsigned long)*digs, 1);
            memcpy(interim, old->digits, sizeof(unsigned long)*bigint_size);
            node->value = new;
            new->sign = old->sign;
            if(offset>=0){
                memcpy(new->digits + offset, interim, sizeof(unsigned long)*(bigint_size-offset));
            }else{
                offset = -offset;
                memcpy(new->digits, interim + offset, sizeof(unsigned long)*(bigint_size-offset));
            }
            free(interim);
            free(old);
            node = node->next;
        }
        bigint_size = digs;
        bigfix_point = f;
    }
    setup_consts();
}

void bigint_destroy(){
    binode_t* node = bigint_head;
    while(node != NULL){
        binode_t* next = node->next;
        free(node->value);
        free(node);
        node = next;
    }
    if(scrap != NULL){
        free(scrap);
    }
    if(nttspace != NULL){
        free(nttspace);
    }
    if(ssmspace != NULL){
        free(ssmspace);
    }
    if(naispace != NULL){
        free(naispace);
    }
    deconst_consts();
}

binode_t* bigint_link(){
    bigint_t* ret = calloc(sizeof(bigint_t) + sizeof(unsigned long)*bigint_size, 1);
    binode_t* node = malloc(sizeof(binode_t));
    node->value = ret;
    node->next = bigint_head;
    bigint_head = node;
    return node;
}

void bigint_unlink(binode_t* big){
    binode_t *prev = NULL, *next = bigint_head;
    while(next!=NULL){
        if(next==big){
            if(prev!=NULL){
                prev->next = next->next;
            }else{
                bigint_head = next->next;
            }
            free(next->value);
            free(next);
            break;
        }
        prev = next;
        next = next->next;
    }
}

int arradd(unsigned long* dst, unsigned long* left, size_t llen, unsigned long* right, size_t rlen){
    long long int carry = 0, tmp = 0;
    for(int i=0; i<llen; i++){
        tmp = left[i] + carry;
        if(i<rlen)tmp += right[i];
        carry = tmp>>32;
        dst[i] = tmp&0xffffffff;
    }
    return carry;
}

int arrsub(unsigned long* dst, unsigned long* left, size_t llen, unsigned  long* right, size_t rlen){
    long long int carry = 0, tmp = 0;
    for(int i=0; i<llen; i++){
        tmp = left[i] + carry;
        if(i<rlen)tmp -= right[i];
        carry = tmp>>32;
        dst[i] = tmp&0xffffffff;
    }
    return carry;
}

void arrmul(unsigned long* dst, unsigned long* src, unsigned long fac, size_t len){
    unsigned long long carry = 0, tmp = 0;
    for(int i=0; i<len; i++){
        tmp = (unsigned long long)src[i]*(unsigned long long)fac + carry;
        dst[i] = tmp&0xffffffff;
        carry = tmp>>32;
    }
}

void naive_mul(unsigned long* dst, unsigned long* left, unsigned long* right, size_t len){
    memset(dst, 0, sizeof(unsigned long)*2*len);
    static unsigned long tmp[2];
    for(int i=0; i<len*2; i++){
        for(int a=0; a<=i && a<len; a++){
            // printf("Naive\n");
            int b = i-a;
            if(b>=len)continue;
            unsigned long long prod = (unsigned long long)left[a]*(unsigned long long)right[b];
            tmp[0] = prod&0xffffffff;
            tmp[1] = prod>>32;
            arradd(dst+i, dst+i, len*2-i, tmp, 2);
        }
    }
}

void kara_nat(unsigned long* left, unsigned long* right, size_t len, unsigned long* work){
  static int depth = 0;
    if(len==1){
        unsigned long long prod = (unsigned long long)*left * (unsigned long long)*right;
        *left = prod&0xffffffff;
        *right = prod>>32;
        return;
    }
    if(len<4){
        {
            naive_mul(naispace, left, right, len);
            memcpy(left, naispace, sizeof(unsigned long)*len);
            memcpy(right, naispace+len, sizeof(unsigned long)*len);
        }
        return;
    }
    depth++;
    int acarry = arradd(work, left, len/2, left+len/2, len/2);
    memcpy(work+len, work, len/2*sizeof(unsigned long));
    int bcarry = arradd(work+len/2, right, len/2, right+len/2, len/2);
    memcpy(work+len+len/2, work+len/2, len/2*sizeof(unsigned long));
    kara_nat(work, work+len/2, len/2, work+len*2);
    int carry = 0;
    if(acarry){
        carry += arradd(work+len/2, work+len/2, len-len/2, work+len+len/2, len/2);
    }
    if(bcarry){
        carry += arradd(work+len/2, work+len/2, len-len/2, work+len, len/2);
    }
    if(acarry && bcarry){
        unsigned long one = 1;
        carry++;
    }
    kara_nat(left, right, len/2, work+len);
    kara_nat(left+len/2, right+len/2, len/2, work+len);
    for(size_t i=0; i<len/2; i++){
        unsigned long tmp = left[i+len/2];
        left[i+len/2] = right[i];
        right[i] = tmp;
    }
    if(arrsub(work, work, len, left, len)!=0){
        carry--;
    }
    if(arrsub(work, work, len, right, len)!=0){
        carry--;
    }
    int tcar=0;
    for(size_t i=0; i<len/2; i++){ //*m
        unsigned long long tmp = left[i+len/2];
        tmp += work[i];
        tmp += tcar;
        tcar = tmp>>32;
        left[i+len/2] = tmp&0xffffffff;
    }
    for(size_t i=0; i<len/2; i++){ //*m^2
        unsigned long long tmp = right[i];
        tmp += work[i+len/2];
        tmp += tcar;
        tcar = tmp>>32;
        right[i] = tmp&0xffffffff;
    }
    tcar += carry;
    for(size_t i=0; i<len/2; i++){ //*m^3
        unsigned long long tmp = right[i+len/2];
        tmp += tcar;
        tcar = tmp>>32;
        right[i+len/2] = tmp&0xffffffff;
    }
    depth--;
}

int arrcmp(unsigned long* left, unsigned long* right, size_t len){
    for(int i=len-1; i>=0; i--){
        if(left[i] > right[i])
          return 1;
        if(right[i] > left[i])
          return -1;
    }
    return 0;
}

void arrshf(unsigned long* dst, size_t len, int bits){
    if(bits>0){ //left
        while(bits>=32){
            for(int i=len-1; i>0; i--){
                dst[i] = dst[i-1];
            }
            dst[0]=0;
            bits-=32;
        }
        if(bits){
          unsigned long carry = 0;
          for(size_t i=0; i<len; i++){
              unsigned long tmp = dst[i]>>(32-bits);
              dst[i] <<= bits;
              dst[i] += carry;
              carry = tmp;
          }
        }
    }else{ //right
        bits = -bits;
        while(bits>=32){
            for(size_t i=0; i<len-1; i++){
                dst[i] = dst[i+1];
            }
            dst[len-1]=0;
            bits-=32;
        }
        if(bits){
          unsigned long carry = 0;
          for(int i=len-1; i>=0; i--){
              unsigned long tmp = dst[i]<<(32-bits);
              tmp &= 0xffffffff;
              dst[i] >>= bits;
              dst[i] += carry;
              carry = tmp;
          }
        }
    }
}

void arrrev(unsigned long *arr, size_t len, int start, int end){
  for(int tbit = start; tbit < end; tbit++){
    int obit = end - 1 - (tbit - start);
    if(obit <= tbit)
      break;
    int tdigit = tbit >> 5, toffset = tbit & 31, odigit = obit >> 5, ooffset = obit & 31;
    int lbit = (arr[tdigit] >> toffset) & 1;
    int rbit = (arr[odigit] >> ooffset) & 1;
    arr[tdigit] &= ~(1 << toffset);
    arr[odigit] &= ~(1 << ooffset);
    arr[tdigit] |= rbit << toffset;
    arr[odigit] |= lbit << ooffset;
  }
}

void arrcyc(unsigned long *arr, size_t len, int offset){
  arrrev(arr, len, 0, offset);
  arrrev(arr, len, offset, len << 5);
  arrrev(arr, len, 0, len << 5);
}

void fp_kara(binode_t* dstn, binode_t* leftn, binode_t* rightn){
    unsigned long* work = scrap;
    bigint_t *dst = dstn->value, *left = leftn->value, *right = rightn->value;
    int sign = left->sign != right->sign;
    memcpy(work + bigint_size*6, left->digits, bigint_size*sizeof(unsigned long));
    memcpy(work + bigint_size*7, right->digits, bigint_size*sizeof(unsigned long));
    kara_nat(work + bigint_size*6, work + bigint_size*7, bigint_size, work);
    size_t small = bigfix_point, big = bigint_size - bigfix_point;
    size_t minpos = small, maxpos = 2*bigint_size - big;
    if(minpos>0){
        if(work[bigint_size*6+minpos-1]>=(1<<31)){
            unsigned long one = 1;
            arradd(work+bigint_size*6 + minpos, work+bigint_size*6 + minpos, bigint_size*2-minpos, &one, 1);
        }
    }
    for(size_t i=0; i<bigint_size; i++){
        size_t pos = i+minpos;
        dst->digits[i] = work[bigint_size*6 + pos];
    }
    dst->sign = sign;
}

void ip_kara(binode_t *dst, binode_t *left, binode_t *right){
  size_t opt = bigfix_point;
  bigfix_point = 0;
  fp_kara(dst, left, right);
  bigfix_point = opt;
}

void bi_printhex(binode_t *num){
  if(num->value->sign)
    printf("-");
  printf("0x");
  for(int i = bigint_size - 1; i >= 0; i--){
    printf("%08x", num->value->digits[i]);
  }
  printf( "* 2 ** -%d", bigfix_point * 32);
}

int ocmp(binode_t* big, long long smol){
    bigint_t* bin = big->value;
    for(int i=bigint_size; i>bigfix_point; i--){
        if(bin->digits[i]){
            return bin->sign?-1:1;
        }
    }
    int sign = 0;
    if(smol<0){
        sign = 1;
        smol = -smol;
    }
    else if(smol==0){
        for(int i=0; i<bigint_size; i++){
            if(bin->digits[i]){
                return bin->sign?-1:1;
            }
        }
        return 0;
    }
    if(bin->sign!=sign){
        return bin->sign?-1:1;
    }
    int one = bigfix_point;
    if(bin->digits[one]>smol){
        return bin->sign?-1:1;
    }else if(bin->digits[one]<smol){
        return bin->sign?1:-1;
    }
    for(int i=bigfix_point-1; i>=0; i--){
        if(bin->digits[i]){
            return bin->sign?-1:1;
        }
    }
    return 0;
}

int bicmp(binode_t *left, binode_t *right){
  if(hibit(left->value->digits, bigint_size) == 0 && hibit(right->value->digits, bigint_size) == 0)
    return 0; //Both are zero
  if(left->value->sign && !right->value->sign)
    return -1; //Left negative
  if(!left->value->sign && right->value->sign)
    return 1; //Right negative
  int dif = arrcmp(left->value->digits, right->value->digits, bigint_size);
  return dif * (left->value->sign ? -1 : 1); //Negate if both are negative
}

void bicpy(binode_t* dst, binode_t* src){
    if(dst != src)
      memcpy(dst->value, src->value, sizeof(bigint_t*)+sizeof(unsigned long)*bigint_size);
}

void ltobi(binode_t* dst, long long src){
    bigint_t* big = dst->value;
    big->sign=0;
    memset(big->digits,0,sizeof(unsigned long)*bigint_size);
    if(src<0){
        src=-src;
        big->sign=1;
    }
    big->digits[0] = src&0xffffffff;
    if(1<bigint_size){
        big->digits[1] = src>>32;
    }
}

void dtobi(binode_t* dst, double src){
    bigint_t* big = dst->value;
    big->sign = 0;
    if(src<0){
        src=-src;
        big->sign=1;
    }
    memset(big->digits, 0, sizeof(unsigned long)*bigint_size);
    unsigned long long place = 0x10000;
    double bigness = 1.0/place;
    bigness /= place;
    for(int i=bigfix_point; i<bigint_size; i++){
      bigness *= place;
      bigness *= place;
    }
    for(int i=bigint_size-1; i>=0; i--){
        unsigned long dig = (src/bigness);
        if(src>=bigness){
            big->digits[i] = dig;
            src -= bigness*dig;
        }
        bigness /= place;
        bigness /= place;
    }
}

void fpinc(binode_t* dst, binode_t* src){
    unsigned long one = 1;
    if(dst!=src)
        memcpy(dst->value, src->value, sizeof(bigint_t) + bigint_size*sizeof(unsigned long));
    if(dst->value->sign)
        arrsub(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
    else
        arradd(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
}

void fpdec(binode_t* dst, binode_t* src){
    unsigned long one = 1;
    if(dst!=src)
        memcpy(dst->value, src->value, sizeof(bigint_t) + bigint_size*sizeof(unsigned long));
    if(dst->value->sign)
        arradd(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
    else
        arrsub(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
}

void biadd(binode_t* dst, binode_t* left, binode_t* right){
    bigint_t* a = left->value;
    bigint_t* b = right->value;
    bigint_t* c = dst->value;
    int carry = 0;
    if(a->sign!=b->sign){
        carry = arrsub(c->digits, a->digits, bigint_size, b->digits, bigint_size);
    }else{
        arradd(c->digits, a->digits, bigint_size, b->digits, bigint_size);
    }
    if(carry){
        c->sign = 1-a->sign;
        for(int i=0; i<bigint_size; i++){
            c->digits[i] = ~c->digits[i];
        }
        unsigned long one = 1;
        arradd(c->digits, c->digits, bigint_size, &one, 1);
    }
    else c->sign = a->sign;
}

void bisub(binode_t* dst, binode_t* left, binode_t* right){
    bigint_t* a = left->value;
    bigint_t* b = right->value;
    bigint_t* c = dst->value;
    int carry = 0;
    if(a->sign==b->sign){
        carry = arrsub(c->digits, a->digits, bigint_size, b->digits, bigint_size);
    }else{
        arradd(c->digits, a->digits, bigint_size, b->digits, bigint_size);
    }
    if(carry){
        c->sign = 1-a->sign;
        for(int i=0; i<bigint_size; i++){
            c->digits[i] = ~c->digits[i];
        }
        unsigned long one = 1;
        arradd(c->digits, c->digits, bigint_size, &one, 1);
    }
    else c->sign = a->sign;
}

void mandelitr(binode_t* x, binode_t* y, binode_t* max, binode_t* ct){
    static binode_t *r=NULL, *i, *rr, *ri, *ii;
    if(r==NULL){
        r = bigint_link();
        i = bigint_link();
        rr = bigint_link();
        ri = bigint_link();
        ii = bigint_link();
    }
    ltobi(ct, 0);
    ltobi(r, 0);
    ltobi(i, 0);
    ltobi(ri, 0);
    unsigned long one = 1;
    while(arrcmp(ct->value->digits,max->value->digits,bigint_size)<0){
        fp_kara(rr, r, r);
        fp_kara(ii, i, i);
        fp_kara(ri, r, i);
        bisub(r, rr, ii);
        biadd(i, ri, ri);
        biadd(ri, rr, ii);
        biadd(r, r, x);
        biadd(i, i, y);
        if(ocmp(ri, 4)>0)break;
        arradd(ct->value->digits, ct->value->digits, bigint_size, &one, 1);
    }
}

unsigned long hibit(unsigned long *dat, size_t len){
  for(int i=len-1; i>=0; i--){
    int bits = 0;
    unsigned long digit = dat[i];
    while(digit){
      bits++;
      digit>>=1;
    }
    if(bits)
      return i*32+bits;
  }
  return 0;
}

void arrdivmod(unsigned long *quo, unsigned long *mod, unsigned long *num, unsigned long *den, size_t len, unsigned long *work){
  memset(work, 0, sizeof(unsigned long)*len); //Quotient work
  memcpy(work + len, num, sizeof(unsigned long)*len); //Mod work
  while(arrcmp(work + len, den, len) >= 0){
    memcpy(work + len*3, work + len, sizeof(unsigned long)*len); //Temp num for current
    long bita = hibit(work + len, len), bitb = hibit(den, len);
    long bitdif = bita - bitb;
    memcpy(work + len*2, den, sizeof(unsigned long)*len); //Temp den to be shifted
    arrshf(work + len*2, len, bitdif);
    while(arrcmp(work + len, work + len*2, len) < 0){
      bitdif--;
      arrshf(work + len*2, len, -1);
    }
    // long bitdif = -1;
    // while(arrcmp(work + len, den, len) >= 0){
      // arrshf(work + len, len, -1);
      // bitdif++;
    // }
    unsigned long onebit = 1 << (bitdif%32);
    size_t byteoff = bitdif/32;
    arradd(work + byteoff, work + byteoff, len - byteoff, &onebit, 1);
    memcpy(work + len*2, den, sizeof(unsigned long)*len); //Temp den to be shifted
    arrshf(work + len*2, len, bitdif);
    arrsub(work + len, work + len*3, len, work + len*2, len);
  }
  if(quo != NULL)
    memcpy(quo, work, sizeof(unsigned long)*len);
  if(mod != NULL)
    memcpy(mod, work+len, sizeof(unsigned long)*len);
}

void idivmod(binode_t* quo, binode_t* mod, binode_t* num, binode_t* den){
  int sign = num->value->sign != den->value->sign;
  unsigned long *qsrc = NULL, *msrc = NULL;
  if(quo != NULL)
    qsrc = quo->value->digits;
  if(mod != NULL)
    msrc = mod->value->digits;
  arrdivmod(qsrc, msrc, num->value->digits, den->value->digits, bigint_size, scrap);
  if(quo != NULL){
    quo->value->sign = sign;
  }
  if(mod != NULL){
    if(sign){
      mod->value->sign = 0;
      if(hibit(mod->value->digits, bigint_size)){
        bisub(mod, den, mod);
        if(quo != NULL){
          unsigned long one = 1;
          arradd(quo->value->digits, quo->value->digits, bigint_size, &one, 1);
        }
      }
    }
    mod->value->sign = den->value->sign;
  }
}

void fdiv(binode_t *quo, binode_t *num, binode_t *den){
  idivmod(quo, NULL, num, den);
  arrshf(quo->value->digits, bigint_size, bigfix_point * 32);
}

#undef SIZE_MUL