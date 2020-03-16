/*
Just gonna put some math functions or such in here I suppose
*/
#include <stdlib.h>
#include <math.h>
#include "bigint.h"

#define MAX_BITS 27

static binode_t **const_atans = NULL, **const_atanhs = NULL;
static binode_t *atan_coef = NULL, *atanh_coef = NULL,
  *factor = NULL, *res0 = NULL, *res1 = NULL,
  *tau = NULL, *pi = NULL, *hpi = NULL, *qpi = NULL, *logtwo = NULL, *rttwo = NULL,
  *tmp0 = NULL, *tmp1 = NULL, *step = NULL,
  *twopow = NULL, *twomod = NULL,
  *otmp0 = NULL, *otmp1 = NULL, *otmp2 = NULL, *otmp3 = NULL, *otmp4, *otmp5;
static int cur_bits = 0;

void setup_consts(){
  size_t bits = 1 + bigfix_point * 32;
  if(bits > MAX_BITS)
    bits = MAX_BITS;
  if(const_atans == NULL){ // Initialize consts
    const_atans = malloc(sizeof(binode_t*) * bits);
    const_atanhs = malloc(sizeof(binode_t*) * bits);
    for(size_t i = 0; i < bits; i ++){
      const_atans[i] = bigint_link();
      const_atanhs[i] = bigint_link();
    }
    atan_coef = bigint_link();
    atanh_coef = bigint_link();
    factor = bigint_link();
    res0 = bigint_link();
    res1 = bigint_link();
    tmp0 = bigint_link();
    tmp1 = bigint_link();
    otmp0 = bigint_link();
    otmp1 = bigint_link();
    otmp2 = bigint_link();
    otmp3 = bigint_link();
    otmp4 = bigint_link();
    otmp5 = bigint_link();
    step = bigint_link();
    hpi = bigint_link();
    pi = bigint_link();
    tau = bigint_link();
    qpi = bigint_link();
    logtwo = bigint_link();
    twopow = bigint_link();
    twomod = bigint_link();
    rttwo = bigint_link();
  }
  else if(bits > cur_bits){ // Add new consts
    const_atans = realloc(const_atans, sizeof(binode_t*) * bits);
    const_atanhs = realloc(const_atanhs, sizeof(binode_t*) * bits);
    for(size_t i = cur_bits; i < bits; i++){
      const_atans[i] = bigint_link();
      const_atanhs[i] = bigint_link();
    }
  }
  else if(bits < cur_bits){ // Unlink excess
    for(size_t i = bits; i < cur_bits; i++){
      bigint_unlink(const_atans[i]);
      bigint_unlink(const_atanhs[i]);
    }
    const_atans = realloc(const_atans, sizeof(binode_t*) * bits);
    const_atanhs = realloc(const_atanhs, sizeof(binode_t*) * bits);
  }
  double p2 = 1;
  dtobi(atan_coef, 1);
  dtobi(atanh_coef, 1);
  int repeat = 1;
  for(size_t i = 0; i < bits; i++){
    double atp2 = atan(p2);
    dtobi(const_atans[i], atp2);
    atp2 = .5 * log((1 + p2) / (1 - p2));
    dtobi(const_atanhs[i], atp2);
    double len = 1.0/sqrt(1 + p2 * p2);
    dtobi(factor, len);
    fp_kara(atan_coef, atan_coef, factor);
    if(i){
      len = 1.0/sqrt(1 - p2 * p2);
      dtobi(factor, len);
      fp_kara(atanh_coef, atanh_coef, factor);
      if(i == repeat){
        repeat = repeat * 3 + 1;
        fp_kara(atanh_coef, atanh_coef, factor);
      }
    }
    p2 /= 2;
  }
  dtobi(tau, M_PI * 2.0);
  bicpy(pi, tau);
  arrshf(pi->value->digits, bigint_size, -1);
  bicpy(hpi, pi);
  arrshf(hpi->value->digits, bigint_size, -1);
  bicpy(qpi, hpi);
  arrshf(qpi->value->digits, bigint_size, -1);
  dtobi(logtwo, 0);
  cur_bits = 1 + bigfix_point * 32;
  for(size_t i = 1; i < cur_bits; i ++){
      dtobi(factor, 1.0/i);
      arrshf(factor->value->digits, bigint_size, -i);
      biadd(logtwo, logtwo, factor);
  }
  dtobi(otmp0, 1);
  big_hypot_atan2(rttwo, NULL, otmp0, otmp0);
}

void deconst_consts(){
  if(const_atans != NULL)
    free(const_atans);
  if(const_atanhs != NULL)
    free(const_atanhs);
}

void CORDIC(binode_t *xout, binode_t *yout, binode_t *theta, int mode, int bkd){
	biconv(xout, TYPE_BIGFIX);
	biconv(yout, TYPE_BIGFIX);
	biconv(theta, TYPE_BIGFIX);
  int repeat = 1;
  size_t start = 0;
  dtobi(step, 2);
  if(mode == MODE_HYP)
    start = 1;
  for(size_t i = start; i < cur_bits; i++){
    bicpy(tmp0, xout);
    arrshf(tmp0->value->digits, bigint_size, -i);
    bicpy(tmp1, yout);
    arrshf(tmp1->value->digits, bigint_size, -i);
    if(mode == MODE_HYP)
      tmp1->value->sign ^= 1;
    if(i < MAX_BITS && mode != MODE_LIN){
      switch(mode){
        case MODE_ANG:
          bicpy(step, const_atans[i]); break;
        case MODE_HYP:
          bicpy(step, const_atanhs[i]); break;
      }
    }
    else{
      if(mode != MODE_HYP || i * 3 + 1 != repeat || i == 0)
        arrshf(step->value->digits, bigint_size, -1);
    }
    int choice = bkd ? !(yout->value->sign) : theta->value->sign;
    if(choice){ //Negative
      biadd(theta, theta, step); //Theta = theta + atan(2^-j)
      if(mode != MODE_LIN)
        biadd(xout, xout, tmp1); // X = X + Y*2^-j
      bisub(yout, yout, tmp0); // Y = Y - X*2^-j
    }
    else{
      bisub(theta, theta, step); //Theta = theta - atan(2^-j)
      if(mode != MODE_LIN)
        bisub(xout, xout, tmp1); // X = X - Y*2^-j
      biadd(yout, yout, tmp0); // Y = Y + X*2^-j
    }
    if(i == repeat && mode == MODE_HYP){
      i--;
      repeat = repeat * 3 + 1;
    }
  }
  binode_t *coef = NULL;
  switch(mode){
    case MODE_ANG:
      coef = atan_coef; break;
    case MODE_HYP:
      coef = atanh_coef; break;
  }
  if(coef != NULL){
    fp_kara(xout, xout, coef);
    fp_kara(yout, yout, coef);
  }
}

void big_cossin(binode_t *cosdst, binode_t *sindst, binode_t *theta){
  idivmod(NULL, factor, theta, tau); //Factor = theta % (2*pi)
  int yneg = bicmp(factor, pi) > 0; //0 if positive Y, 1 if negative
  int xneg = bicmp(factor, hpi) > 0;
  if(yneg){
    bisub(factor, factor, pi); //Factor = theta % pi;
    xneg = bicmp(factor, hpi) <= 0;
  }
  int swap = bicmp(factor, qpi) > 0; //1 if factor > pi/4, else 0
  if(swap){
    bisub(factor, hpi, factor); //Factor = pi/2 - factor if factor > pi/4
  }
  dtobi(res0, 1);
  dtobi(res1, 0);
  CORDIC(res0, res1, factor, MODE_ANG, 0);
  if(swap){
    bicpy(factor, res0);
    bicpy(res0, res1);
    bicpy(res1, factor);
  }
  res0->value->sign = xneg;
  res1->value->sign = yneg;
  if(cosdst != NULL)
    bicpy(cosdst, res0);
  if(sindst != NULL)
    bicpy(sindst, res1);
}

void big_hypot_atan2(binode_t *hypot, binode_t *atan, binode_t *x, binode_t *y){
  int yneg = y->value->sign, xneg = x->value->sign;
  bicpy(res0, x);
  bicpy(res1, y);
  res0->value->sign = 0;
  res1->value->sign = 0;
  int swap = bicmp(res0, res1) > 0;
  if(swap){
    bicpy(factor, res0);
    bicpy(res0, res1);
    bicpy(res1, factor);
  }
  dtobi(factor, 0);
  CORDIC(res0, res1, factor, MODE_ANG, 1); // res1 = 0 here
  if(swap){
    bisub(factor, hpi, factor);
  }
  if(xneg){
    bisub(factor, pi, factor);
  }
  if(yneg){
    factor->value->sign ^= 1;
  }
  if(hypot != NULL)
    bicpy(hypot, res0);
  if(atan != NULL)
    bicpy(atan, factor);
}

void big_coshsinh(binode_t *coshdst, binode_t *sinhdst, binode_t *theta){
  int neg = theta->value->sign;
  bicpy(factor, theta);
  factor->value->sign = 0;
  idivmod(twopow, twomod, factor, logtwo);
  int pow2 = twopow->value->digits[0] * (twopow->value->sign ? -1 : 1);
  dtobi(res0, 1);
  dtobi(res1, 0);
  CORDIC(res0, res1, twomod, MODE_HYP, 0);
  biadd(tmp0, res0, res1); //tmp0 = e^twomod
  bisub(tmp1, res0, res1); //tmp1 = e^-twomod
  arrshf(tmp0->value->digits, bigint_size, pow2 - 1);
  arrshf(tmp1->value->digits, bigint_size, -pow2 - 1);
  if(coshdst != NULL)
    biadd(coshdst, tmp0, tmp1);
  if(sinhdst != NULL){
    bisub(sinhdst, tmp0, tmp1);
    sinhdst->value->sign ^= neg;
  }
}

void big_hhyp_atanh(binode_t *hhyp, binode_t *atanh, binode_t *x, binode_t *y){
  int neg = x->value->sign != y->value->sign;
  bicpy(res0, x);
  bicpy(res1, y);
  res0->value->sign = 0;
  res1->value->sign = 0;
  dtobi(factor, 0);
  CORDIC(res0, res1, factor, MODE_HYP, 1);
  factor->value->sign = neg;
  if(hhyp != NULL)
    bicpy(hhyp, res0);
  if(atanh != NULL)
    bicpy(atanh, factor);
}

void big_rt_log(binode_t *rtdst, binode_t *logn, binode_t *x){
  bicpy(otmp0, x);
  int pow2 = hibit(otmp0->value->digits, bigint_size);
  pow2 -= 1 + bigfix_point * 32;
  arrshf(otmp0->value->digits, bigint_size, -pow2);
  fpdec(otmp1, otmp0);
  fpinc(otmp0, otmp0);
  big_hhyp_atanh(rtdst, logn, otmp0, otmp1);
    dtobi(otmp0, pow2);
  if(rtdst != NULL){
    arrshf(rtdst->value->digits, bigint_size, -1 + pow2 / 2);
    if(pow2 & 1){
      fp_kara(rtdst, rtdst, rttwo);
      if(pow2 < 0)
        arrshf(rtdst->value->digits, bigint_size, -1);
    }
  }
  if(logn != NULL){
    arrshf(logn->value->digits, bigint_size, 1);
    fp_kara(otmp1, otmp0, logtwo);
    biadd(logn, logn, otmp1);
  }
}

void big_exp(binode_t *dst, binode_t *src){
  big_coshsinh(otmp0, otmp1, src);
  biadd(dst, otmp0, otmp1);
}

void big_log(binode_t *dst, binode_t *src){
  big_rt_log(NULL, dst, src);
}

void big_div(binode_t *dst, binode_t *left, binode_t *right){
  int sign = left->value->sign != right->value->sign;
  big_log(otmp2, left);
  big_log(otmp3, right);
  bisub(otmp2, otmp2, otmp3);
  big_exp(dst, otmp2);
  dst->value->sign = sign;
}

void big_cos(binode_t *dst, binode_t *src){
  big_cossin(dst, NULL, src);
}

void big_sin(binode_t *dst, binode_t *src){
  big_cossin(NULL, dst, src);
}

void big_tan(binode_t *dst, binode_t *src){
  big_cossin(otmp3, otmp2, src);
  big_div(dst, otmp2, otmp3);
}

void big_acos(binode_t *dst, binode_t *src){
  dtobi(otmp2, 1);
	int sign = src->value->sign;
	big_abs(otmp3, src);
	big_abs(otmp4, src);
	if(bicmp(otmp3, otmp2) >= 0){
		dtobi(dst, 0);
	}else{
		big_hhyp_atanh(otmp3, NULL, otmp2, otmp3); //otmp0 = sqrt(1 - src^2)
		big_hypot_atan2(NULL, dst, otmp4, otmp3);
	}
	if(sign){
		bisub(dst, pi, dst);
	}
}

void big_asin(binode_t *dst, binode_t *src){
  dtobi(otmp2, 1);
	int sign = src->value->sign;
	big_abs(otmp3, src);
	big_abs(otmp4, src);
	if(bicmp(otmp3, otmp2) >= 0){
		bicpy(dst, hpi);
	}else{
		big_hhyp_atanh(otmp3, NULL, otmp2, otmp3);
		big_hypot_atan2(NULL, dst, otmp3, otmp4);
	}
	dst->value->sign = sign;
}

void big_atan(binode_t *dst, binode_t *src){
  dtobi(otmp2, 1);
  big_hypot_atan2(NULL, dst, otmp2, src);
}

void big_atan2(binode_t *dst, binode_t *x, binode_t *y){
  big_hypot_atan2(NULL, dst, x, y);
}

void big_hypot(binode_t *dst, binode_t *x, binode_t *y){
  big_hypot_atan2(dst, NULL, x, y);
}

void big_cosh(binode_t *dst, binode_t *src){
  big_coshsinh(dst, NULL, src);
}

void big_csinh(binode_t *dst, binode_t *src){
  big_coshsinh(NULL, dst, src);
}

void big_tanh(binode_t *dst, binode_t *src){
  big_coshsinh(otmp3, otmp2, src);
  big_div(dst, otmp2, otmp3);
}

void big_acosh(binode_t *dst, binode_t *src){
  dtobi(otmp2, 1);
  big_hhyp_atanh(otmp3, NULL, src, otmp2); //otmp3 = sqrt(src^2 - 1)
  biadd(otmp2, otmp3, src); //otmp2 = src + sqrt(src^2 - 1)
  big_log(dst, otmp2);
}

void big_asinh(binode_t *dst, binode_t *src){
  dtobi(otmp2, 1);
  big_hypot_atan2(otmp3, NULL, src, otmp2); //otmp3 = sqrt(src^2 - 1)
  biadd(otmp2, otmp3, src); //otmp2 = src + sqrt(src^2 - 1)
  big_log(dst, otmp2);
}

void big_atanh(binode_t *dst, binode_t *src){
  dtobi(step, 1);
  biadd(otmp2, step, src);
  bisub(otmp3, step, src);
  big_log(otmp2, otmp2);
  big_log(otmp3, otmp3);
  bisub(dst, otmp2, otmp3);
  arrshf(dst->value->digits, bigint_size, -1);
}

void big_recip(binode_t *dst, binode_t *src){
	int sign = src->value->sign;
	bicpy(otmp2, src);
	otmp2->value->sign = 0;
	big_log(otmp3, otmp2);
	otmp3->value->sign ^= 1;
	big_exp(dst, otmp3);
	dst->value->sign = sign;
}

void big_pow(binode_t *dst, binode_t *base, binode_t *ex){
	big_log(otmp2, base);
	fp_kara(otmp2, otmp2, ex);
	big_exp(dst, otmp2);
}

void big_abs(binode_t *dst, binode_t *src){
	bicpy(dst, src);
	dst->value->sign = 0;
}

void big_negate(binode_t *dst, binode_t *src){
	bicpy(dst, src);
	dst->value->sign ^= 1;
}