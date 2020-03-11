/*
Helper functions for random number generation and manipulation
*/

#ifndef _H_RAND
#define _H_RAND

#include <stdint.h>

extern uint32_t rand_max_replace; /* Maximum random value */

void srand_replace(uint32_t); /* Set random sand */
uint32_t rand_replace(); /* Get a random integer */
double rand_uniform(); /* Random double in [0,1] */
double perlin_bias(double t, double x); /* Perlin bias function */
double perlin_gain(double t, double x); /* Perlin gain function */
double gaussian(); /* Random gaussian, mean = 0, sigma = 1 */

#endif