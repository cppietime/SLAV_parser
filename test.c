/*
An example program that makes use of the parser functionalities, parses an example program, then prints the results.
*/

#include <stdio.h>
#include "slav.h"

int main(){
	regam_nfa_start(); /* Start up the program */
	FILE *srcfil = fopen("./parser/shift.txt", "r"); /* The source file for the language */
	slav_lang lang;
	slav_updog(srcfil, &lang); /* Read and process the language */
	fclose(srcfil);
	srcfil = fopen("./parser/save.txt", "w");
	slav_sugma(srcfil, &lang); /* Save the table to save.txt */
	parsam_table_print(lang.table, stdout); /* Print the table to stdout */
	regam_nfa_print(lang.dfa); /* Print the language's lexer DFA */
	fclose(srcfil);
	slav_squat(&lang); /* Delete the language object (for now) */
	srcfil = fopen("./parser/save.txt", "r");
	slav_ligma(srcfil, &lang); /* Reload the language from save.txt where it was saved earlier */
	fclose(srcfil);
	srcfil = fopen("./parser/test.txt", "r");
	parsam_ast *res = slav_joe(srcfil, &lang); /* Parse the contents of test.txt */
	fclose(srcfil);
	parsam_ast_print(res, lang.table, stdout); /* Print the parsed object */
	parsam_ast_delete(res);
	slav_squat(&lang);
	regam_nfa_end();
	return 0;
}