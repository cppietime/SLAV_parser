#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include <limits.h>
#include "bigint.h"

#define SIZE_MUL 15

size_t bigint_size = 1;
size_t bigfix_point = 0;
binode_t* bigint_head = NULL;
digit_t *scrap = NULL,
    *nttspace = NULL,
    *ssmspace = NULL,
    *naispace = NULL;

digit_t n2n5(digit_t n){
    digit_t n2 = 1L<<n;
    while(n%2==0){
        n>>=1;
    }
    n2 *= n;
    if(n2%5!=0) n2 *= 5;
    return n2;
}

void farrprint(FILE *file, digit_t* digs, size_t len){
    fprintf(file, "0x");
    for(int i=len-1; i>=0; i--){
        fprintf(file, "%.8lx", digs[i]);
    }
}

void sarrprint(char *dst, digit_t* digs, size_t len){
    sprintf(dst, "0x");
    for(int i=len-1; i>=0; i--){
        sprintf(dst, "%.8lx", digs[i]);
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
    naispace = malloc(sizeof(digit_t)*size*2);
    size_t kara_space = size*SIZE_MUL;
    scrap = malloc(sizeof(digit_t)*kara_space);
    size_t ntt_space = 0;
    size_t mul_space = 0;
    int ct=0;
    digit_t lbits = size<<5;
    while(1){
        digit_t bits = 1;
        digit_t x = 0;
        while(bits<lbits){
            x++;
            bits<<=1;
        }
        digit_t n = x>>1, k = x-n, n2 = n2n5(n), y = n + (2<<k);
        y += (n2 - (y%n2))%n2;
        digit_t Y = 1;
        while(Y<y)Y<<=1;
        if(ct==0){
            ntt_space = Y>>2;
            ct=1;
        }
        if(n<7)n=7;
        mul_space += sizeof(digit_t)*4*Y<<(n-5);
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
        bigint_t* new = calloc(sizeof(bigint_t) + sizeof(digit_t)*digs, 1);
        node->value = new;
        new->sign = old->sign;
        memcpy(new->digits, old->digits, sizeof(digit_t)*bigint_size);
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
						if(node->type == TYPE_BIGFIX){
							int offset = f - bigfix_point;
							bigint_t* old = node->value;
							bigint_t* new = calloc(sizeof(bigint_t) + sizeof(digit_t)*digs, 1);
							digit_t* interim = calloc(sizeof(digit_t)*digs, 1);
							memcpy(interim, old->digits, sizeof(digit_t)*bigint_size);
							node->value = new;
							new->sign = old->sign;
							if(offset>=0){
									memcpy(new->digits + offset, interim, sizeof(digit_t)*(bigint_size-offset));
							}else{
									offset = -offset;
									memcpy(new->digits, interim + offset, sizeof(digit_t)*(bigint_size-offset));
							}
							free(interim);
							free(old);
						}
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
    bigint_t* ret = calloc(sizeof(bigint_t) + sizeof(digit_t)*bigint_size, 1);
    binode_t* node = malloc(sizeof(binode_t));
    node->value = ret;
    node->next = bigint_head;
		node->type = TYPE_BIGINT;
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

void biconv(binode_t *num, int target){
	if(num->type == target)
		return;
	switch(num->type){
		case TYPE_BIGINT:{
			arrshf(num->value->digits, bigint_size, bigfix_point * 32);
			break;
		}
		case TYPE_BIGFIX:{
			digit_t incr = hibit(num->value->digits, bigfix_point) == bigfix_point * 32;
			arrshf(num->value->digits, bigint_size, -bigfix_point * 32);
			if(num->value->sign)
				arrsub(num->value->digits, num->value->digits, bigint_size, &incr, 1);
			else
				arradd(num->value->digits, num->value->digits, bigint_size, &incr, 1);
			break;
		}
	}
	num->type = target;
}

int arradd(digit_t* dst, digit_t* left, size_t llen, digit_t* right, size_t rlen){
    long long int carry = 0, tmp = 0;
    for(int i=0; i<llen; i++){
        tmp = left[i] + carry;
        if(i<rlen)tmp += right[i];
        carry = tmp>>32;
        dst[i] = tmp&0xffffffff;
    }
    return carry;
}

int arrsub(digit_t* dst, digit_t* left, size_t llen, digit_t* right, size_t rlen){
    long long int carry = 0, tmp = 0;
    for(int i=0; i<llen; i++){
        tmp = left[i] + carry;
        if(i<rlen)tmp -= right[i];
        carry = tmp>>32;
        dst[i] = tmp&0xffffffff;
    }
    return carry;
}

void arrmul(digit_t* dst, digit_t* src, digit_t fac, size_t len){
    unsigned long long carry = 0, tmp = 0;
    for(int i=0; i<len; i++){
        tmp = (unsigned long long)src[i]*(unsigned long long)fac + carry;
        dst[i] = tmp&0xffffffff;
        carry = tmp>>32;
    }
}

void naive_mul(digit_t* dst, digit_t* left, digit_t* right, size_t len){
    memset(dst, 0, sizeof(digit_t)*2*len);
    static digit_t tmp[2];
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

void kara_nat(digit_t* left, digit_t* right, size_t len, digit_t* work){
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
            memcpy(left, naispace, sizeof(digit_t)*len);
            memcpy(right, naispace+len, sizeof(digit_t)*len);
        }
        return;
    }
    depth++;
    int acarry = arradd(work, left, len/2, left+len/2, len/2);
    memcpy(work+len, work, len/2*sizeof(digit_t));
    int bcarry = arradd(work+len/2, right, len/2, right+len/2, len/2);
    memcpy(work+len+len/2, work+len/2, len/2*sizeof(digit_t));
    kara_nat(work, work+len/2, len/2, work+len*2);
    int carry = 0;
    if(acarry){
        carry += arradd(work+len/2, work+len/2, len-len/2, work+len+len/2, len/2);
    }
    if(bcarry){
        carry += arradd(work+len/2, work+len/2, len-len/2, work+len, len/2);
    }
    if(acarry && bcarry){
        digit_t one = 1;
        carry++;
    }
    kara_nat(left, right, len/2, work+len);
    kara_nat(left+len/2, right+len/2, len/2, work+len);
    for(size_t i=0; i<len/2; i++){
        digit_t tmp = left[i+len/2];
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

int arrcmp(digit_t* left, digit_t* right, size_t len){
    for(int i=len-1; i>=0; i--){
        if(left[i] > right[i])
          return 1;
        if(right[i] > left[i])
          return -1;
    }
    return 0;
}

void arrshf(digit_t* dst, size_t len, int bits){
    if(bits>0){ //left
        while(bits>=32){
            for(int i=len-1; i>0; i--){
                dst[i] = dst[i-1];
            }
            dst[0]=0;
            bits-=32;
        }
        if(bits){
          digit_t carry = 0;
          for(size_t i=0; i<len; i++){
              digit_t tmp = dst[i]>>(32-bits);
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
          digit_t carry = 0;
          for(int i=len-1; i>=0; i--){
              digit_t tmp = dst[i]<<(32-bits);
              tmp &= 0xffffffff;
              dst[i] >>= bits;
              dst[i] += carry;
              carry = tmp;
          }
        }
    }
}

void arrrev(digit_t *arr, size_t len, int start, int end){
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

void arrcyc(digit_t *arr, size_t len, int offset){
  arrrev(arr, len, 0, offset);
  arrrev(arr, len, offset, len << 5);
  arrrev(arr, len, 0, len << 5);
}

void fp_kara(binode_t* dstn, binode_t* leftn, binode_t* rightn){
    digit_t* work = scrap;
    bigint_t *dst = dstn->value, *left = leftn->value, *right = rightn->value;
    int sign = left->sign != right->sign;
    memcpy(work + bigint_size*6, left->digits, bigint_size*sizeof(digit_t));
    memcpy(work + bigint_size*7, right->digits, bigint_size*sizeof(digit_t));
    kara_nat(work + bigint_size*6, work + bigint_size*7, bigint_size, work);
    size_t small = bigfix_point, big = bigint_size - bigfix_point;
    size_t minpos = small, maxpos = 2*bigint_size - big;
    if(minpos>0){
        if(work[bigint_size*6+minpos-1]>=(1<<31)){
            digit_t one = 1;
            arradd(work+bigint_size*6 + minpos, work+bigint_size*6 + minpos, bigint_size*2-minpos, &one, 1);
        }
    }
    for(size_t i=0; i<bigint_size; i++){
        size_t pos = i+minpos;
        dst->digits[i] = work[bigint_size*6 + pos];
    }
    dst->sign = sign;
		dstn->type = TYPE_BIGFIX;
}

void ip_kara(binode_t *dst, binode_t *left, binode_t *right){
  size_t opt = bigfix_point;
  bigfix_point = 0;
  fp_kara(dst, left, right);
  bigfix_point = opt;
	dst->type = TYPE_BIGINT;
}

void bi_printhex(FILE *dst, binode_t *num){
  if(num->value->sign)
    fprintf(dst, "-");
  fprintf(dst, "0x");
  for(int i = bigint_size - 1; i >= 0; i--){
    fprintf(dst, "%08lx", num->value->digits[i]);
  }
	if(num->type == TYPE_BIGFIX)
		fprintf(dst, "* 2 ** -%ld", bigfix_point * 32);
}

void strtobi_dec(binode_t *dst, char *src, char **eptr){
	dst->value->sign = 0;
	char *ptr = src;
	if(*ptr == '-'){
		dst->value->sign = 1;
		ptr++;
	}
	for(; *ptr >= '0' && *ptr <= '9'; ptr++){
		arrmul(dst->value->digits, dst->value->digits, 10, bigint_size);
		digit_t add = *ptr - '0';
		arradd(dst->value->digits, dst->value->digits, bigint_size, &add, 1);
	}
	if(dst->type == TYPE_BIGFIX){
		arrshf(dst->value->digits, bigint_size, bigfix_point * 32);
		if(*ptr == '.'){
			int sign = dst->value->sign;
			memcpy(scrap, dst->value->digits + bigfix_point, (bigint_size - bigfix_point) * 4);
			double lval = strtod(ptr, &ptr);
			dtobi(dst, lval);
			dst->value->sign = sign;
			memcpy(dst->value->digits + bigfix_point, scrap, (bigint_size - bigfix_point) * 4);
		}
	}
	if(eptr != NULL)
		*eptr = ptr;
}

void bi_printdec(FILE *dst, binode_t *num, size_t lim){
	if(hibit(num->value->digits, bigint_size) == 0){
		fprintf(dst, "0");
		return;
	}
	static binode_t *ten = NULL, *tmp, *digit;
	static char buffer[1024];
	if(ten == NULL){
		ten = bigint_link();
		tmp = bigint_link();
		digit = bigint_link();
	}
	ltobi(ten, 10);
	digit->type = num->type;
	bicpy(tmp, num);
	tmp->value->sign = 0;
	if(num->value->sign)
		fputc('-', dst);
	int ptr = 0;
	if(num->type == TYPE_BIGFIX)
		arrshf(tmp->value->digits, bigint_size, -bigfix_point * 32);
	tmp->type = TYPE_BIGINT;
	while(hibit(tmp->value->digits, bigint_size) > 0){
		idivmod(tmp, digit, tmp, ten);
		buffer[ptr++] = '0' + digit->value->digits[0];
	}
	for(; ptr; ptr --){
		fputc(buffer[ptr - 1], dst);
	}
	if(num->type == TYPE_BIGFIX){
		fputc('.', dst);
		ptr = 0;
		ltobi(ten, 10);
		bicpy(tmp, num);
		tmp->value->sign = 0;
		while(hibit(tmp->value->digits, bigint_size) > 0 && lim){
			memset(tmp->value->digits + bigfix_point, 0, 4);
			ip_kara(tmp, tmp, ten);
			bicpy(digit, tmp);
			int c = '0' + digit->value->digits[bigfix_point];
			fputc(c, dst);
			lim--;
		}
	}
}

void bi_sprintdec(char *dst, binode_t *num, size_t lim){
	if(hibit(num->value->digits, bigint_size) == 0){
		*dst++ = '0';
		*dst = 0;
		return;
	}
	static binode_t *ten = NULL, *tmp, *digit;
	static char buffer[1024];
	if(ten == NULL){
		ten = bigint_link();
		tmp = bigint_link();
		digit = bigint_link();
	}
	ltobi(ten, 10);
	digit->type = num->type;
	bicpy(tmp, num);
	tmp->value->sign = 0;
	if(num->value->sign)
		*dst++ = '-';
	int ptr = 0;
	if(num->type == TYPE_BIGFIX)
		arrshf(tmp->value->digits, bigint_size, -bigfix_point * 32);
	tmp->type = TYPE_BIGINT;
	while(hibit(tmp->value->digits, bigint_size) > 0){
		idivmod(tmp, digit, tmp, ten);
		buffer[ptr++] = '0' + digit->value->digits[0];
	}
	for(; ptr; ptr --){
		// fputc(buffer[ptr - 1], dst);
		*dst++ = buffer[ptr - 1];
	}
	if(num->type == TYPE_BIGFIX){
		// fputc('.', dst);
		*dst++ = '.';
		ptr = 0;
		ltobi(ten, 10);
		bicpy(tmp, num);
		tmp->value->sign = 0;
		while(hibit(tmp->value->digits, bigint_size) > 0 && lim){
			memset(tmp->value->digits + bigfix_point, 0, 4);
			ip_kara(tmp, tmp, ten);
			bicpy(digit, tmp);
			int c = '0' + digit->value->digits[bigfix_point];
			// fputc(c, dst);
			*dst++ = c;
			lim--;
		}
	}
	*dst = 0;
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
      memcpy(dst->value, src->value, sizeof(bigint_t)+sizeof(digit_t)*bigint_size);
		dst->type = src->type;
}

void ltobi(binode_t* dst, long long src){
    bigint_t* big = dst->value;
    big->sign=0;
    memset(big->digits,0,sizeof(digit_t)*bigint_size);
    if(src<0){
        src=-src;
        big->sign=1;
    }
    big->digits[0] = src&0xffffffff;
    if(1<bigint_size){
        big->digits[1] = src>>32;
    }
		dst->type = TYPE_BIGINT;
}

void dtobi(binode_t* dst, double src){
    bigint_t* big = dst->value;
    big->sign = 0;
    if(src<0){
        src=-src;
        big->sign=1;
    }
    memset(big->digits, 0, sizeof(digit_t)*bigint_size);
    unsigned long long place = 0x10000;
    double bigness = 1.0/place;
    bigness /= place;
    for(int i=bigfix_point; i<bigint_size; i++){
      bigness *= place;
      bigness *= place;
    }
    for(int i=bigint_size-1; i>=0; i--){
        digit_t dig = (src/bigness);
        if(src>=bigness){
            big->digits[i] = dig;
            src -= bigness*dig;
        }
        bigness /= place;
        bigness /= place;
    }
		dst->type = TYPE_BIGFIX;
}

double bitod(binode_t *src){
	double ret = 0;
	if(src->type == TYPE_BIGINT){
		ret = src->value->digits[0];
		if(bigint_size > 1){
			double k = src->value->digits[1] << 16L;
			k *= 1 << 16L;
			ret += k;
		}
	}else{
		double mul = 1;
		for(size_t i = bigfix_point; i < bigint_size; i++){
			double k = src->value->digits[i] * mul;
			ret += k;
			mul *= 0x10000;
			mul *= 0x10000;
		}
		mul = 1;
		for(size_t i = bigfix_point; i > 0; i--){
			mul /= 0x10000;
			mul /= 0x10000;
			if(i - 1 >= bigint_size){
				continue;
			}
			double k = src->value->digits[i - 1] * mul;
			ret += k;
		}
	}
	if(src->value->sign)
		ret = -ret;
	return ret;
}

void fpinc(binode_t* dst, binode_t* src){
    digit_t one = 1;
    if(dst!=src)
        memcpy(dst->value, src->value, sizeof(bigint_t) + bigint_size*sizeof(digit_t));
    if(dst->value->sign)
        arrsub(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
    else
        arradd(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
		dst->type = TYPE_BIGFIX;
}

void fpdec(binode_t* dst, binode_t* src){
    digit_t one = 1;
    if(dst!=src)
        memcpy(dst->value, src->value, sizeof(bigint_t) + bigint_size*sizeof(digit_t));
    if(dst->value->sign)
        arradd(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
    else
        arrsub(dst->value->digits+bigfix_point, dst->value->digits+bigfix_point, bigint_size-bigfix_point, &one, one);
		dst->type = TYPE_BIGFIX;
}

void biadd(binode_t* dst, binode_t* left, binode_t* right){
		if(left->type != right->type){
			fprintf(stderr, "Error: Adding mismatching numeric types!\n");
		}
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
            c->digits[i] = (~c->digits[i]) & 0xffffffff;
        }
        digit_t one = 1;
        arradd(c->digits, c->digits, bigint_size, &one, 1);
    }
    else c->sign = a->sign;
		dst->type = left->type;
}

void bisub(binode_t* dst, binode_t* left, binode_t* right){
		if(left->type != right->type){
			fprintf(stderr, "Error: Subtracting mismatching numeric types!\n");
		}
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
            c->digits[i] = (~c->digits[i]) & 0xffffffff;
        }
        digit_t one = 1;
        arradd(c->digits, c->digits, bigint_size, &one, 1);
    }
    else c->sign = a->sign;
		dst->type = left->type;
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
    digit_t one = 1;
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

digit_t hibit(digit_t *dat, size_t len){
  for(int i=len-1; i>=0; i--){
    int bits = 0;
    digit_t digit = dat[i];
    while(digit){
      bits++;
      digit>>=1;
    }
    if(bits)
      return i*32+bits;
  }
  return 0;
}

void arrdivmod(digit_t *quo, digit_t *mod, digit_t *num, digit_t *den, size_t len, digit_t *work){
  memset(work, 0, sizeof(digit_t)*len); //Quotient work
  memcpy(work + len, num, sizeof(digit_t)*len); //Mod work
  while(arrcmp(work + len, den, len) >= 0){
    memcpy(work + len*3, work + len, sizeof(digit_t)*len); //Temp num for current
    long bita = hibit(work + len, len), bitb = hibit(den, len);
    long bitdif = bita - bitb;
    memcpy(work + len*2, den, sizeof(digit_t)*len); //Temp den to be shifted
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
    digit_t onebit = 1 << (bitdif%32);
    size_t byteoff = bitdif/32;
    arradd(work + byteoff, work + byteoff, len - byteoff, &onebit, 1);
    memcpy(work + len*2, den, sizeof(digit_t)*len); //Temp den to be shifted
    arrshf(work + len*2, len, bitdif);
    arrsub(work + len, work + len*3, len, work + len*2, len);
  }
  if(quo != NULL)
    memcpy(quo, work, sizeof(digit_t)*len);
  if(mod != NULL)
    memcpy(mod, work+len, sizeof(digit_t)*len);
}

void idivmod(binode_t* quo, binode_t* mod, binode_t* num, binode_t* den){
	if(num->type != den->type){
		fprintf(stderr, "Warning! Dividng mismatching type!\n");
	}
  int sign = num->value->sign != den->value->sign;
  digit_t *qsrc = NULL, *msrc = NULL;
  if(quo != NULL)
    qsrc = quo->value->digits;
  if(mod != NULL)
    msrc = mod->value->digits;
  arrdivmod(qsrc, msrc, num->value->digits, den->value->digits, bigint_size, scrap);
  if(quo != NULL){
    quo->value->sign = sign;
  }
  if(mod != NULL){
		mod->type = num->type;
    if(sign){
      mod->value->sign = 0;
      if(hibit(mod->value->digits, bigint_size)){
        bisub(mod, den, mod);
        if(quo != NULL){
          digit_t one = 1;
          arradd(quo->value->digits, quo->value->digits, bigint_size, &one, 1);
        }
      }
    }
    mod->value->sign = den->value->sign;
  }
}

#undef SIZE_MUL