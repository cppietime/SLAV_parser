/*
Implementation for matching regex DFAs to strings.
Written by Yaakov Schectman 2019.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "regam.h"

/* From nfa.c */
extern datam_darr *statelist;

/* Source FILE */
static FILE *srcfile = NULL;

/* DFA for producer */
static regam_nfa *prod_dfa = NULL;

/* Replacements */
static datam_darr *repl_list = NULL;

/* Table for tokens */
static parsam_table *parse_table = NULL;

/* Line number */
static int line_no = 0;

void regam_load_lexer(regam_nfa *dfa, datam_darr *repls, parsam_table *table){
	prod_dfa = dfa;
	repl_list = repls;
	parse_table = table;
}

void regam_load_lexsrc(FILE *src){
	line_no = 0;
	srcfile = src;
}

uint32_t transfor(regam_nstate *state, uint32_t sym){
    regam_transition t;
    for(size_t i = 0; i < state->transitions->n; i ++){
        datam_darr_get(state->transitions, &t, i);
        if(t.start <= sym && t.end >= sym){
            return t.target;
        }
    }
    return 0;
}

uint32_t* regam_match(uint32_t *src, size_t n, regam_nfa *dfa, uint32_t *type){
    uint32_t sid = dfa->start;
    uint32_t *lacc = NULL;
    size_t pos;
    regam_nstate *state;
    for(pos = 0; pos <= n; pos ++){
        datam_darr_get(statelist, &state, sid);
        if(state->accept != 0){
            if(type != NULL){
                *type = state->accept - 1;
            }
            lacc = src + pos;
        }
		if(pos == n){
			break;
		}
        uint32_t sym = src[pos];
        sid = transfor(state, sym);
        if(sid == 0){
            break;
        }
    }
    return lacc;
}

uint32_t regam_filter(uint32_t *p0, uint32_t *p1, uint32_t type, datam_darr *repls){
    regam_repl repl;
    for(size_t i = 0; i < repls->n; i ++){
        datam_darr_get(repls, &repl, i);
        if(type == repl.base && !wstrncmp(p0, repl.key, p1 - p0)){
            return repl.target;
        }
    }
    return type;
}

static uint32_t buffer[4096];
static size_t buf_ptr = 0;
static size_t buf_size = 0;

parsam_ast *token_over(){
	parsam_ast *ret = malloc(sizeof(parsam_ast));
	ret->lexeme = NULL;
	ret->symbol = (parsam_symbol){.type = Terminal, .id = parse_table->n_term - 1};
	ret->subtrees = NULL;
	ret->filename = NULL;
	ret->line_no = line_no;
	return ret;
}

parsam_ast* regam_get_lexeme(){
	while(1){
		if(srcfile == NULL){
			return token_over();
		}
		if(buf_ptr > 0 && buf_ptr < buf_size){
			memmove(buffer, buffer + buf_ptr,4 * (buf_size - buf_ptr));
		}
		buf_size -= buf_ptr;
		buf_ptr = 0;
		if(feof(srcfile) && buf_size == 0){
			return token_over();
		}
    for(int i = 0; i < 4096 - buf_size; i++){
      uint32_t unicode = 0;
      int first = fgetc(srcfile);
			if(first == 0xef){ /* Check for BOM */
				long pos = ftell(srcfile);
				int bom = 1;
				if(fgetc(srcfile) != 0xbb)
					bom = 0;
				if(fgetc(srcfile) != 0xbf)
					bom = 0;
				if(bom)
					first = fgetc(srcfile);
				else
					fseek(srcfile, pos, SEEK_SET);
			}
      unicode = first; /* ASCII */
      if((first & 0xe0) == 0xc0){ /* 2-byte */
        int second = fgetc(srcfile);
        unicode = (first & 0x1f) << 6;
        unicode |= second & 0x3f;
      }
      else if((first & 0xf0) == 0xe0){ /* 3-byte */
        int second = fgetc(srcfile);
        int third = fgetc(srcfile);
        unicode = (first & 0x0f) << 6;
        unicode |= second & 0x3f;
        unicode <<= 6;
        unicode |= third & 0x3f;
      }
      else if((first & 0xf8) == 0xf0){ /* 4-byte */
        int second = fgetc(srcfile);
        int third = fgetc(srcfile);
        int fourth = fgetc(srcfile);
        unicode = (first & 0x07) << 6;
        unicode |= second & 0x3f;
        unicode <<= 6;
        unicode |= third & 0x3f;
        unicode <<= 6;
        unicode |= fourth & 0x3f;
      }
      if(feof(srcfile)){
				if(buffer[buf_size - 1] != 10)
					buffer[buf_size++] = 10;
        break;
			}
      buffer[buf_size++] = unicode;
    }
		if(buf_size == 0){
			return token_over();
		}
		uint32_t ttype;
		uint32_t *eptr = regam_match(buffer, buf_size, prod_dfa, &ttype);
		if(eptr == NULL){
			fprintf(stderr, "Error @ line %d: Unrecognized token: \"", line_no);
			fprintwn(stderr, buffer, buf_size);
			fprintf(stderr, "\"\n");
			return NULL;
		}
		for(uint32_t *ch = buffer; ch < eptr; ch ++){
			if(*ch == '\n'){
				line_no ++;
			}
		}
		buf_ptr = eptr - buffer;
		ttype = regam_filter(buffer, eptr, ttype, repl_list);
		if(ttype >= parse_table->n_term){
			continue;
		}
		parsam_ast *ret = malloc(sizeof(parsam_ast));
		ret->symbol = (parsam_symbol){.type = Terminal, .id = ttype};
		ret->lexeme = malloc(4 * (eptr - buffer + 1));
		wstrncpy(ret->lexeme, buffer, eptr - buffer);
		ret->lexeme[eptr - buffer] = 0;
		ret->subtrees = NULL;
		ret->line_no = line_no;
		ret->filename = NULL; /* This too... */
		return ret;
	}
}