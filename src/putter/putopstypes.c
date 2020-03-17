/*
Type conversions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "putter.h"
#include "putops.h"
#include "slavio.h"
#include "datam.h"

datam_darr* put_tostring(uservar *var){
	datam_darr *ret = datam_darr_new(4);
	uint32_t chr;
	if(var->flags & FLAG_RECUR){
		chr = '.';
		for(size_t i = 0; i < 3; i++){
			datam_darr_push(ret, &chr);
		}
	}
	else{
		var->flags |= FLAG_RECUR;
		switch(var->type){
			case big_integer:
			case big_fixed:{
				binode_t *big = var->values.big_val;
				bi_sprintdec(strbuf, big, max_digits);
				for(char *ptr = strbuf; *ptr; ptr++){
					chr = *ptr;
					datam_darr_push(ret, &chr);
				}
				break;
			}
			case native_float:{
				sprintf(strbuf, "%.*f", max_digits, (float)(var->values.float_val));
				for(char *ptr = strbuf; *ptr; ptr++){
					chr = *ptr;
					datam_darr_push(ret, &chr);
				}
				break;
			}
			case code_point:{
				chr = var->values.uni_val;
				datam_darr_push(ret, &chr);
				break;
			}
			case wstring:{
				datam_darr *oldstr = var->values.list_val;
				datam_darr_pushall(ret, oldstr);
				ret->n--;
				break;
			}
			case open_file:{
				strcpy(strbuf, "<File object>");
				for(char *ptr = strbuf; *ptr; ptr++){
					chr = *ptr;
					datam_darr_push(ret, &chr);
				}
				break;
			}
			case block_code:{
				code_block *code = var->values.code_val;
				sprintf(strbuf, "<Code block: length %ld, %ld bound>", code->members->n, code->bound_vars->n);
				for(char *ptr = strbuf; *ptr; ptr++){
					chr = *ptr;
					datam_darr_push(ret, &chr);
				}
				break;
			}
			case list:{
				chr = '[';
				datam_darr_push(ret, &chr);
				datam_darr *children = var->values.list_val;
				uservar *child;
				for(size_t i = 0; i < children->n; i++){
					if(i){
						chr = ',';
						datam_darr_push(ret, &chr);
						chr = ' ';
						datam_darr_push(ret, &chr);
					}
					datam_darr_get(children, &child, i);
					datam_darr *sub = put_tostring(child);
					sub->n--;
					datam_darr_pushall(ret, sub);
					datam_darr_delete(sub);
				}
				chr = ']';
				datam_darr_push(ret, &chr);
				break;
			}
			case hash_table:{
				chr = '{';
				datam_darr_push(ret, &chr);
				datam_hashtable *table = var->values.map_val;
				size_t ct = 0;
				for(datam_hashbucket *buck = datam_hashtable_next(table, NULL); buck != NULL; buck = datam_hashtable_next(table, buck)){
					if(ct){
						chr = ',';
						datam_darr_push(ret, &chr);
					}
					uservar *key = (uservar*)(buck->key);
					datam_darr *sub = put_tostring(key);
					sub->n--;
					datam_darr_pushall(ret, sub);
					datam_darr_delete(sub);
					key = (uservar*)(buck->value);
					sub = put_tostring(key);
					sub->n--;
					datam_darr_pushall(ret, sub);
					datam_darr_delete(sub);
					ct++;
				}
				chr = '}';
				datam_darr_push(ret, &chr);
				break;
			}
			case empty:{
				sprintf(strbuf, "<empty>");
				for(char *ptr = strbuf; *ptr; ptr++){
					chr = *ptr;
					datam_darr_push(ret, &chr);
				}
				break;
			}
		}
		var->flags &= ~FLAG_RECUR;
	}
	chr = 0;
	datam_darr_push(ret, &chr);
	return ret;
}

void put_tobigint(binode_t *dst, uservar *src){
	dst->type = TYPE_BIGINT;
	switch(src->type){
		case big_integer:
			bicpy(dst, src->values.big_val);
			break;
		case big_fixed:
			bicpy(dst, src->values.big_val);
			biconv(dst, TYPE_BIGINT);
			break;
		case native_float:
			dtobi(dst, src->values.float_val);
			biconv(dst, TYPE_BIGINT);
			break;
		case code_point:
			ltobi(dst, src->values.uni_val);
			break;
		case wstring:{
			datam_darr *str = src->values.list_val;
			uint32_t chr;
			for(size_t i = 0; i < str->n; i++){
				datam_darr_get(str, &chr, i);
				strbuf[i] = chr;
			}
			strtobi_dec(dst, strbuf, NULL);
			biconv(dst, TYPE_BIGINT);
			break;
		}
		default:{
			quit_for_error(1, "Error: Converting non-number/string to an int\n");
		}
	}
}

void put_tobigfix(binode_t *dst, uservar *src){
	dst->type = TYPE_BIGFIX;
	switch(src->type){
		case big_integer:
			bicpy(dst, src->values.big_val);
			biconv(dst, TYPE_BIGFIX);
			break;
		case big_fixed:
			bicpy(dst, src->values.big_val);
			break;
		case native_float:
			dtobi(dst, src->values.float_val);
			break;
		case code_point:
			ltobi(dst, src->values.uni_val);
			biconv(dst, TYPE_BIGFIX);
			break;
		case wstring:{
			datam_darr *str = src->values.list_val;
			uint32_t chr;
			for(size_t i = 0; i < str->n; i++){
				datam_darr_get(str, &chr, i);
				strbuf[i] = chr;
			}
			strtobi_dec(dst, strbuf, NULL);
			break;
		}
		default:{
			quit_for_error(2, "Error: Converting non-number/string to a fixed\n");
		}
	}
}

double put_tofloat(uservar *src){
	switch(src->type){
		case big_fixed:
		case big_integer:{
			binode_t *num = src->values.big_val;
			double ret = bitod(num);
			return ret;
		}
		case native_float:
			return src->values.float_val;
		case code_point:
			return (double)(src->values.uni_val);
		case wstring:{
			datam_darr *str = src->values.list_val;
			uint32_t chr;
			for(size_t i = 0; i < str->n; i++){
				datam_darr_get(str, &chr, i);
				strbuf[i] = chr;
			}
			return strtod(strbuf, NULL);
		}
		default:{
			quit_for_error(2, "Error: Converting non-num to float\n");
		}
	}
}

datam_hashtable* put_tomap(uservar *src){
	datam_hashtable *map = datam_hashtable_new(uservar_hsh, uservar_cmp, 8);
	switch(src->type){
		case list:{
			uservar *child;
			datam_darr *lst = src->values.list_val;
			for(size_t i = 0; i < lst->n; i++){
				datam_darr_get(lst, &child, i);
				datam_hashtable_put(map, child, new_variable());
			}
			break;
		}
		case hash_table:{
			datam_hashtable *base = src->values.map_val;
			datam_hashtable_addall(map, base);
			break;
		}
		default:{
			datam_hashtable_put(map, src, new_variable());
			break;
		}
	}
	return map;
}

datam_darr* put_tolist(uservar *src){
	datam_darr *ret = datam_darr_new(sizeof(uservar*));
	switch(src->type){
		case wstring:{
			int32_t ch;
			datam_darr *wstr = src->values.list_val;
			for(size_t i = 0; i < wstr->n - 1; i++){
				datam_darr_get(wstr, &ch, i);
				uservar *var = new_cp(ch);
				datam_darr_push(ret, &var);
			}
			break;
		}
		case list:{
			datam_darr_pushall(ret, src->values.list_val);
			break;
		}
		case hash_table:{
			datam_hashtable *table = src->values.map_val;
			for(datam_hashbucket *buck = datam_hashtable_next(table, NULL); buck != NULL; buck = datam_hashtable_next(table, buck)){
				uservar *key = (uservar*)(buck->key);
				datam_darr_push(ret, &key);
			}
			break;
		}
		default:{
			datam_darr_push(ret, &src);
		}
	}
	return ret;
}

int32_t truth_val(uservar *var){
	switch(var->type){
		case big_integer:
		case big_fixed:{
			return hibit(var->values.big_val->value->digits, bigint_size);
		}
		case native_float:{
			float v = var->values.float_val;
			return (v == v) && (v != 0);
		}
		case wstring:{
			return var->values.list_val->n > 1;
		}
		case list:{
			return var->values.list_val->n > 0;
		}
		case code_point:{
			return var->values.uni_val;
		}
		case block_code:{
			return var->values.code_val->members->n > 0;
		}
		default:{
			return 0;
		}
	}
}

pushable new_k_list(size_t k){
	datam_darr *lst = datam_darr_new(sizeof(uservar*));
	uservar *member = NULL;
	for(size_t i = 0; i < k; i++){
		datam_darr_push(lst, &member);
	}
	for(size_t i = k; i > 0; i--){
		member = pop_var();
		datam_darr_set(lst, &member, i - 1);
	}
	uservar *var = new_variable();
	var->type = list;
	var->values.list_val = lst;
	return pushable_var(var);
}

uservar* match_type(uservar *val, uservar *templ){
	uservar *ret;
	switch(templ->type){
		case wstring:{
			datam_darr *wstr = put_tostring(val);
			ret = new_seq(wstr, wstring);
			break;
		}
		case list:{
			datam_darr *lst = put_tolist(val);
			ret = new_seq(lst, list);
			break;
		}
		case big_fixed:{
			binode_t *num = bigint_link();
			put_tobigfix(num, val);
			ret = new_bignum(num, big_fixed);
			break;
		}
		case big_integer:{
			binode_t *num = bigint_link();
			put_tobigint(num, val);
			ret = new_bignum(num, big_integer);
			break;
		}
		case native_float:{
			float f = put_tofloat(val);
			ret = new_float(f);
			break;
		}
		case code_point:{
			float f = put_tofloat(val);
			ret = new_cp((int32_t)f);
			break;
		}
		default:{
			quit_for_error(7, "Error: Invalid conversion target\n");
		}
	}
	return ret;
}

void match_top2(){
	uservar *left, *right;
	pop_n_vars(0, 2, &right, &left);
	uservar *conv = match_type(left, right);
	push_sym(pushable_var(conv));
}

