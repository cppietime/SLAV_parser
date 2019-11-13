/*
General utility that does all the steps in one call.
Written by Yaakov Schectman 2019.
*/

#ifndef _H_SLAV
#define _H_SLAV

#include <stdio.h>
#include "datatypes/datam.h"
#include "parser/parsam.h"
#include "regex/regam.h"

/* Type containing the necessary elements */
typedef struct {
	parsam_table *table; /* Table for parsing */
	regam_nfa *dfa; /* DFA for lexing */
	datam_darr *replacements; /* List of token replacements */
} slav_lang;

/* Constructs SLaV from user defined FILE */
void slav_updog(FILE *src, slav_lang *dst);

/* Saves constructed SLaV */
void slav_sugma(FILE *dst, slav_lang *src);

/* Loads constructed SLaV */
void slav_ligma(FILE *src, slav_lang *dst);

/* Deletes saved objects */
void slav_squat(slav_lang *lang);

/* Parse file contents to AST */
parsam_ast* slav_joe(FILE *src, slav_lang *lang);

#endif