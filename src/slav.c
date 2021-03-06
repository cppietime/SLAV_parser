/*
Slav.h implementation.
Written by Yaakov Schectman 2019.
*/

#include <stdio.h>
#include "slav.h"
#include "slavio.h"

void slav_updog(FILE *src, slav_lang *dst){
	long cpos = ftell(src);
	int styp = fget_utf_type(src);
	if(styp != TYPE_UTF8){
		FILE *newf = tmpfile();
		convert_utf(newf, TYPE_UTF8, src, styp);
		fflush(newf);
		rewind(newf);
		src = newf;
	}else{
		fseek(src, cpos, SEEK_SET);
	}
	dst->table = parsam_table_make(src);
	if(dst->table == NULL){
		dst->dfa = NULL;
		dst->replacements = NULL;
		return;
	}
	dst->dfa = regam_from_file(src, dst->table);
	dst->replacements = regam_repls(src, dst->table);
	regam_dfa_reduce(dst->dfa);
	if(styp != TYPE_UTF8)
		fclose(src);
}

void slav_squat(slav_lang *lang){
	parsam_table_delete(lang->table);
	regam_nfa_delete(lang->dfa);
	regam_repls_delete(lang->replacements);
}

void slav_sugma(FILE *dst, slav_lang *src){
	parsam_table_print(src->table, dst);
	regam_dfa_save(dst, src->dfa);
	regam_repls_save(dst, src->replacements, src->table);
}

void slav_ligma(FILE *src, slav_lang *dst){
	dst->table = parsam_table_read(src);
	dst->dfa = regam_dfa_load(src);
	dst->replacements = regam_repls(src, dst->table);
}

parsam_ast* slav_joe(FILE *src, slav_lang *lang){
	regam_load_lexer(lang->dfa, lang->replacements, lang->table);
	regam_load_lexsrc(src);
	return parsam_parse(lang->table, regam_get_lexeme);
}