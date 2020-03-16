#include <stdint.h>
#include <math.h>
#include "rand.h"

static uint32_t seed = 0;
uint32_t rand_max_replace = 1<<30;

void srand_replace(uint32_t s){
    seed = s;
}

//An explicit PRNG is used so that seeds can be exported
uint32_t rand_replace(){
    seed *= 1103515245L;
    seed += 12345;
    seed &= 0x7fffffff;
    return seed & 0x3fffffff;
}

double rand_uniform(){
    return (float)rand_replace()/rand_max_replace;
}

double perlin_bias(double t, double x){
    return t/((((1.0/x)-2.0)*(1.0-t))+1.0);
}

double perlin_gain(double t, double x){
    if(t<=.5){
        return perlin_bias(t*2.0, x)/2.0;
    }else{
        return perlin_bias(t*2.0-1.0, 1.0 - x)/2.0 + .5;
    }
}

static double next_gauss = NAN;

double gaussian(){
    if(next_gauss!=next_gauss){
        double U = (double)rand_replace()/rand_max_replace;
        double V = (double)rand_replace()/rand_max_replace;
        double R = sqrt(-2*log(U));
        double theta = 2*M_PI*V;
        double next_gauss = R*sin(theta);
        return R*cos(theta);
    }else{
        double ret = next_gauss;
        next_gauss = NAN;
        return ret;
    }
}