/*
Forward definitions for operators
*/

#ifndef _H_PUTOPS
#define _H_PUTOPS
#include "putter.h"

datam_darr* put_tostring(uservar *var); /* Convert to string rep */
void put_tobigint(binode_t *dst, uservar *src);
void put_tobigfix(binode_t *dst, uservar *src);
double put_tofloat(uservar *src);
datam_hashtable* put_tomap(uservar *src);
datam_darr* put_tolist(uservar *src);
int32_t truth_val(uservar *src);
void match_top2(); /* Convert left to type of right */

pushable new_k_list(size_t k); /* Top k values into a list */

void print_outright(); /* Print as default mode */
uservar* input_wstring(int32_t delim);

void assign();
void dup_top(size_t n);
void get_n();
void resolve();
void execute_top();
void execute_2nd_if_top();
void execute_while();
void swap_x_with_n(int x, int n);
void swap_2nd_with_top();

void plus_basic(); /* + op */
void put_append(); /* . op */
void minus_basic(); /* - op */
void times_basic(); /* * op */
void mod_basic(); /* % op */
void div_basic(); /* / op */
void var_not(); /* ! op */
void abs_basic(); /* | op */
int32_t comp_top2();
void bit_and();
void bit_or();
void bit_xor();

#endif