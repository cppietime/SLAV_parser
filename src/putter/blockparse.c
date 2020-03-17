/*
Converts a parsetree to a code block
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slav.h"
#include "slavio.h"
#include "putter.h"
#include "bigint.h"

static uint32_t pushed_cp = 0;
static int has_pushed = 0;
#define comment_delim '#'

uint32_t next_char(FILE *src){
	uint32_t ret;
	if(has_pushed){
		has_pushed = 0;
		ret = pushed_cp;
	}
	else
		ret = fget_unicode(src, srcfile_type);
	return ret;
}

void putback_char(uint32_t cp){
	has_pushed = 1;
	pushed_cp = cp;
}

static uint32_t ops_list[] = {
	/* Math */
	'+', /* Addition */
	'-', /* Subtraction */
	'*', /* Multiplication */
	'/', /* Division */
	'%', /* Modulus */
	'.', /* Append */
	'!', /* Not */
	'~', /* Absolute value */
	'>', /* Greater than */
	'<', /* Less than */
	'=', /* Equality */
	'|', /* Bitwise OR */
	'&', /* Bitwise AND */
	'^', /* Bitwise XOR */
	/* Data */
	']', /* Singleton list */
	':', /* Assign */
	'$', /* Duplicate reference */
	'@', /* Index */
	'(', /* Resolve */
	',', /* Swap top with n-th */
	'\\', /* Match type */
	'`', /* Pop and discard */
	'&', /* Bitwise AND */
	'|', /* Bitwise OR */
	'^', /* Bitwise XOR */
	/* IO */
	'\'', /* Print */
	'_', /* Input */
	/* Control */
	')', /* Execute */
	'?', /* Conditional execute */
	';', /* While loop */
	0
};

static uint32_t break_list[] = {
	'{',
	'}',
	' ',
	'\n',
	'\t',
	'\r',
	comment_delim,
	'"',
	0
};

pushable parse_sym(FILE *src){
	static uint32_t tokbuffer[1024];
	static char numbuffer[1024];
	uint32_t cp = next_char(src);
	while(cp == ' ' || cp == '\n' || cp == '\r' || cp == comment_delim){ /* Skip whitespace and comments */
		while(cp == ' ' || cp == '\n' || cp == '\r') /* Skip whitespace */
			cp = next_char(src);
		while(cp == comment_delim){ /* Skip comments */
			cp = next_char(src);
			while(cp != comment_delim){
				if(cp == '\\')
					cp = next_char(src);
				else if(cp == (uint32_t)EOF){
					fprintf(stderr, "Warning: Encountered EOF mid-comment!\n");
					return (pushable){.type = nothing};
				}
				cp = next_char(src);
			}
			cp = next_char(src);
		}
	}
	if(cp == EOF){
		return (pushable){.type = nothing};
	}
	else if(cp == '{'){ /* Get a code block */
		uservar *blockvar = new_variable();
		code_block *block = parse_block(src);
		blockvar->type = block_code;
		blockvar->values.code_val = block;
		return (pushable){.type = literal, .values = {.lit_val = blockvar}};
	}
	else if(cp == '['){ /* Empty list */
		datam_darr *lst = datam_darr_new(sizeof(uservar*));
		uservar *var = new_variable();
		var->type = list;
		var->values.list_val = lst;
		return pushable_var(var);
	}
	else if(cp == '"'){ /* Get a string literal */
		uservar *strvar = new_variable();
		strvar->type = wstring;
		strvar->values.list_val = datam_darr_new(4);
		cp = next_char(src);
		while(cp != '"'){
			if(cp == (uint32_t)EOF){
				fprintf(stderr, "Warning: Encountered EOF mid-string!\n");
				return (pushable){.type = nothing};
			}
			uint32_t app = cp;
			if(cp == '\\'){ /* Escapes */
				cp = next_char(src);
				switch(cp){
					case 'n': app = '\n'; break;
					case 't': app = '\t'; break;
					case 'r': app = '\r'; break;
					case 'x':{
						app = 0;
						cp = next_char(src);
						while((cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'f') || (cp >= 'A' && cp <= 'F')){
							app *= 16;
							if(cp >= 'a')
								cp -= 'a' - 10;
							else if(cp >= 'A')
								cp -= 'A' - 10;
							else
								cp -= '0';
							app += cp;
							cp = next_char(src);
						}
						putback_char(cp);
						break;
					}
					default: app = cp;
				}
			}
			datam_darr_push(strvar->values.list_val, &app);
			cp = next_char(src);
		}
		uint32_t end = 0;
		datam_darr_push(strvar->values.list_val, &end);
		return pushable_var(strvar);;
	}
	else if(cp == '-' || cp == '.' || (cp >= '0' && cp <= '9')){
		size_t i = 0;
		int hasdec = 0;
		if(cp == '-' || cp =='.'){
			uint32_t look = next_char(src);
			if(!(look >= '0' && look <= '9') && !(look == '.' && cp =='-')){
				putback_char(look);
				return (pushable){.type = builtin, .values = {.op_val = cp}};
			}
			if(cp == '.')
				hasdec = 1;
			numbuffer[i++] = (char)cp;
			cp = look;
		}
		while((cp >= '0' && cp <= '9') || (cp == '.' && !hasdec)){
			numbuffer[i++] = (char)cp;
			if(cp == '.')
				hasdec = 1;
			cp = next_char(src);
		}
		int hase = 0;
		if(cp == 'E' || cp == 'e'){ /* Deal w/ floats */
			hase = 1;
			numbuffer[i++] = 'e';
			cp = next_char(src);
			if(cp == '-'){
				numbuffer[i++] = '-';
				cp = next_char(src);
			}
			while(cp >= '0' && cp <= '9'){
				numbuffer[i++] = cp;
				cp = next_char(src);
			}
		}
		putback_char(cp);
		numbuffer[i] = 0;
		if(hase){
			double fval = strtod(numbuffer, NULL);
			uservar *var = new_float(fval);
			return pushable_var(var);
		}
		else{
			binode_t *bignum = bigint_link();
			bignum->type = hasdec ? TYPE_BIGFIX : TYPE_BIGINT;
			strtobi_dec(bignum, numbuffer, NULL);
			uservar *numvar = new_variable();
			numvar->type = hasdec ? big_fixed : big_integer;
			numvar->values.big_val = bignum;
			return pushable_var(numvar);
		}
	}
	else{
		size_t i = 0;
		int isop = 0;
		while(!isop && cp != (uint32_t)EOF){
			isop = 0;
			uint32_t *candop = ops_list;
			for(; *candop; candop++){
				if(cp == *candop){
					isop = 1;
					break;
				}
			}
			if(isop && i == 0){
				return (pushable){.type = builtin, .values = {.op_val = cp}};
			}
			if(!isop){
				for(candop = break_list; *candop; candop++){
					if(cp == *candop){
						isop = 1;
						break;
					}
				}
			}
			if(!isop){
				tokbuffer[i++] = cp;
				cp = next_char(src);
			}
		}
		putback_char(cp);
		tokbuffer[i++] = 0;
		uint32_t *name = malloc(4 * i);
		for(size_t j = 0; j < i; j++)
			name[j] = tokbuffer[j];
		pushable reference = (pushable){.type=varname, .values = {.name_val = name}};
		return reference;
	}
}

code_block* parse_block(FILE *src){
	code_block *ret = malloc(sizeof(code_block));
	ret->bound_vars = datam_darr_new(sizeof(uservar*));
	ret->members = datam_darr_new(sizeof(pushable));
	uint32_t cp = next_char(src);
	while(cp != '}' && cp != (uint32_t)EOF){
		putback_char(cp);
		pushable tok = parse_sym(src);
		if(tok.type != nothing)
			datam_darr_push(ret->members, &tok);
		cp = next_char(src);
	}
	return ret;
}
