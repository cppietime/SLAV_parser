/*
Forward definitions for operators
*/

#ifndef _H_PUTOPS
#define _H_PUTOPS

datam_darr* put_tostring(uservar *var); /* Convert to string rep */
void put_tobigint(binode_t *dst, uservar *src);
void put_tobigfix(binode_t *dst, uservar *src);
double put_tofloat(uservar *src);

pushable new_k_list(size_t k); /* Top k values into a list */

void print_outright(); /* Print as default mode */
void assign();
void dup_top(size_t n);
void get_n();
void resolve();
void execute_top();

void plus_basic(); /* + op */
void put_append(); /* . op */
void minus_basic(); /* - op */
void times_basic(); /* * op */

#endif