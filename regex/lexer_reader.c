/*
Reads and constructs regex DFA from file that has already been partially read for parsing.
Written by Yaakov Schectman 2019.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "regam.h"

/* TODO I still have to do this */
extern parsam_table *regex_table;

regam_nfa* regam_from_file(FILE *src, parsam_table *table){
    if(regex_table == NULL){
        fprintf(stderr, "Error: regex table  not created\n");
        return NULL;
    }
    regam_setsrc_file(src, 0);
    static char line[1024];
    size_t lptr = 0;
    datam_darr *nfas = datam_darr_new(sizeof(regam_nfa*));
    while(1){
        lptr = 0;
        while(1){
            char ch = fgetc(src);
            if(ch == ':' || ch == '\n' || ch == 0 || ch == EOF || ch == '\r'){
                break;
            }
            line[lptr++] = ch;
        }
        line[lptr] = 0;
        if(line[0] == '\n' || line[0] == '\r' || line[0] == 0 || line[0] == EOF || !strcmp(line, "OVER")){
            break;
        }
        uint32_t tid = table->n_term + 1;
        if(strcmp(line, "IGNORE")){ /* i.e. if not "IGNORE" */
            parsam_symbol *sym = (parsam_symbol*)datam_hashtable_get(table->symnames, line);
			if(sym == NULL){
				fprintf(stderr, "Error: invalid token name %s\n", line);
			}else{
				tid = sym->id + 1;
			}
        }
        parsam_ast *ast_regex = parsam_parse(regex_table, regam_producer);
        if(ast_regex == NULL){
            fprintf(stderr, "Error: Invalid regex\n");
            datam_darr_delete(nfas);
            return NULL;
        }
        regam_regex *regex = regam_regex_build(ast_regex);
        parsam_ast_delete(ast_regex);
        regam_nfa *nfa = regam_nfa_build(regex);
        regam_regex_delete(regex);
        regam_nfa_tag(nfa, tid);
        datam_darr_push(nfas, &nfa);
    }
    if(nfas->n == 0){
        fprintf(stderr, "Warning: No tokens found\n");
        datam_darr_delete(nfas);
        return NULL;
    }
    regam_nfa_union((regam_nfa**)nfas->data, nfas->n);
    regam_nfa *nfa;
    datam_darr_get(nfas, &nfa, 0);
    datam_darr_delete(nfas);
    regam_nfa *dfa = regam_to_dfa(nfa);
    regam_nfa_delete(nfa);
    return dfa;
}

datam_darr* regam_repls(FILE *src, parsam_table *table){
    datam_darr *repls = datam_darr_new(sizeof(regam_repl));
    static char line[1024];
    while(1){
		line[0] = 0;
        fgets(line, 1024, src);
        if(line[0] == 0 || line[0] == '\n' || line[0] == '\r' || line[0] == EOF){
            break;
        }
		char *end = line + strlen(line) - 1;
		while(end > line && isspace(*end)){
			end--;
		}
		*(end+1) = 0;
        char *sp1 = strchr(line, ' ');
        char *sp2 = strchr(sp1+1, ' ');
        *sp1 = 0;
        *sp2 = 0;
        regam_repl repl;
		parsam_symbol *s1, *s2;
        s1 = (parsam_symbol*)datam_hashtable_get(table->symnames, line);
        s2 = (parsam_symbol*)datam_hashtable_get(table->symnames, sp1+1);
		repl.base = s1->id;
		repl.target = s2->id;
        char *key = malloc(strlen(sp2+1)+1);
        strcpy(key, sp2+1);
        repl.key = key;
        datam_darr_push(repls, &repl);
    }
    return repls;
}

void regam_repls_delete(datam_darr *repls){
	regam_repl repl;
	for(size_t i = 0; i < repls->n; i ++){
		datam_darr_get(repls, &repl, i);
		free(repl.key);
	}
	datam_darr_delete(repls);
}

void regam_repls_save(FILE *dst, datam_darr *repls, parsam_table *table){
	regam_repl repl;
	for(size_t i = 0; i < repls->n; i ++){
		datam_darr_get(repls, &repl, i);
		fprintf(dst, "%s %s %s\n", table->symids[repl.base], table->symids[repl.target], repl.key);
	}
}