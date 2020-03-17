/*
IO functions
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "slav.h"
#include "bigint.h"
#include "slavio.h"
#include "putter.h"
#include "putops.h"

var_type common_math_type(var_type type1, var_type type2){
	if(type1 == type2)
		return type1;
	if(type1 == big_fixed || type2 == big_fixed)
		return big_fixed;
	if(type1 == big_integer && type2 == native_float)
		return big_fixed;
	if(type2 == big_integer && type1 == native_float)
		return big_fixed;
	if(type1 == big_integer || type2 == big_integer)
		return big_integer;
	if(type1 == native_float || type2 == native_float)
		return native_float;
	if(type1 == code_point && type2 == code_point)
		return code_point;
	return empty;
}

/* Appends strings and adds code points to create a string */
void popped_plus_basic(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	if(left->type == wstring || right->type == wstring){
		datam_darr *lstr = put_tostring(left), *rstr = put_tostring(right);
		lstr->n--;
		datam_darr_pushall(lstr, rstr);
		datam_darr_delete(rstr);
		uservar *var = new_seq(lstr, wstring);
		push_sym(pushable_var(var));
	}
	else switch(com){
		case big_fixed:{
			put_tobigfix(op1, left);
			put_tobigfix(op2, right);
			binode_t *num = bigint_link();
			num->type = TYPE_BIGFIX;
			biadd(num, op1, op2);
			uservar *var = new_bignum(num, big_fixed);
			push_sym(pushable_var(var));
			break;
		}
		case big_integer:{
			put_tobigint(op1, left);
			put_tobigint(op2, right);
			binode_t *num = bigint_link();
			num->type = TYPE_BIGINT;
			biadd(num, op1, op2);
			uservar *var = new_bignum(num, big_integer);
			push_sym(pushable_var(var));
			break;
		}
		case native_float:{
			double l = put_tofloat(left), r = put_tofloat(right);
			uservar *var = new_float(l + r);
			push_sym(pushable_var(var));
			break;
		}
		case code_point:{
			int32_t cp = left->values.uni_val + right->values.uni_val;
			uservar *sum = new_cp(cp);
			push_sym(pushable_var(sum));
			break;
		}
		default:{
			switch(left->type){
				case block_code:{
					if(right->type != block_code){
						quit_for_error(3, "Error: Cannot add code and non-code\n");
						break;
					}
					code_block *a = left->values.code_val, *b = right->values.code_val;
					code_block *sum = malloc(sizeof(code_block));
					sum->bound_vars = datam_darr_new(sizeof(uservar*));
					sum->members = datam_darr_new(sizeof(pushable));
					pushable member;
					for(size_t i = 0; i < a->members->n; i++){
						datam_darr_get(a->members, &member, i);
						member = clean_ref(member);
						datam_darr_push(sum->members, &member);
					}
					for(size_t i = 0; i < b->members->n; i++){
						datam_darr_get(b->members, &member, i);
						member = clean_ref(member);
						datam_darr_push(sum->members, &member);
					}
					datam_darr_pushall(sum->bound_vars, b->bound_vars);
					datam_darr_pushall(sum->bound_vars, a->bound_vars);
					uservar *block = new_variable();
					block->type = block_code;
					block->values.code_val = sum;
					push_sym(pushable_var(block));
					break;
				}
				case list:{
					if(right->type != list){
						quit_for_error(3, "Error: Cannot add non-list to list\n");
						break;
					}
					datam_darr *sum = datam_darr_new(sizeof(uservar*));
					datam_darr *llst = left->values.list_val;
					datam_darr_pushall(sum, llst);
					datam_darr *rlst = right->values.list_val;
					datam_darr_pushall(sum, rlst);
					uservar *var = new_seq(sum, list);
					push_sym(pushable_var(var));
					break;
				}
				default:{
					quit_for_error(3, "Error: Unsupported addition type\n");
				}
			}
		}
	}
}

void plus_basic(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	popped_plus_basic(left, right);
}

void popped_append(uservar *left, uservar *right){
	if(left->type == list){
		datam_darr *lst = left->values.list_val;
		datam_darr_push(lst, &right);
		push_sym(pushable_var(left));
	}
	else if(left->type == wstring){
		datam_darr *lstr = left->values.list_val;
		lstr->n--;
		datam_darr *rstr = put_tostring(right);
		datam_darr_pushall(lstr, rstr);
		datam_darr_delete(rstr);
		push_sym(pushable_var(left));
	}
	else if(left->type == block_code && right->type == block_code){
		code_block *lcode = left->values.code_val, *rcode = right->values.code_val;
		pushable p;
		for(size_t i = 0; i < rcode->members->n; i++){
			datam_darr_get(rcode->members, &p, i);
			p = clean_ref(p);
			datam_darr_push(lcode->members, &p);
		}
		uservar *bound;
		for(size_t i = rcode->bound_vars->n; i > 0; i--){
			datam_darr_get(rcode->bound_vars, &bound, i - 1);
			datam_darr_insert(lcode->bound_vars, &bound, 0);
		}
		push_sym(pushable_var(left));
	}
	else if(left->type == code_point){
		datam_darr *wstr = datam_darr_new(4);
		int32_t chr = left->values.uni_val;
		datam_darr_push(wstr, &chr);
		datam_darr *rstr = put_tostring(right);
		datam_darr_pushall(wstr, rstr);
		datam_darr_delete(rstr);
		uservar *var = new_seq(wstr, wstring);
		push_sym(pushable_var(var));
	}
	else{
		popped_plus_basic(left, right);
	}
}

void put_append(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	popped_append(left, right);
}

void popped_minus_basic(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	if(left->type == wstring){
		datam_darr *nstr = datam_darr_new(4);
		datam_darr *lstr = left->values.list_val;
		int knock = (int)put_tofloat(right);
		if(knock < 0){
			knock = lstr->n - 1 + knock;
		}
		int32_t chr;
		for(int i = 0; i < lstr->n - 1 && i < knock; i++){
			datam_darr_get(lstr, &chr, i);
			datam_darr_push(nstr, &chr);
		}
		chr = 0;
		datam_darr_push(nstr, &chr);
		uservar *var = new_seq(nstr, wstring);
		push_sym(pushable_var(var));
	}
	else if(right->type == wstring){
		int knock = (int)put_tofloat(left);
		datam_darr *nstr = datam_darr_new(4);
		datam_darr *lstr = right->values.list_val;
		if(knock < 0){
			knock = lstr->n - 1 + knock;
		}
		int32_t chr;
		for(int i = lstr->n - 1 - knock; i < lstr->n - 1; i++){
			if(i){
				datam_darr_get(lstr, &chr, i);
				datam_darr_push(nstr, &chr);
			}
		}
		chr = 0;
		datam_darr_push(nstr, &chr);
		uservar *var = new_seq(nstr, wstring);
		push_sym(pushable_var(var));
	}
	else if(left->type == list){
		int knock = (int)put_tofloat(right);
		datam_darr *nstr = datam_darr_new(sizeof(uservar*));
		datam_darr *lstr = left->values.list_val;
		if(knock < 0){
			knock = lstr->n + knock;
		}
		uservar *chr;
		for(int i = 0; i < lstr->n && i < knock; i++){
			datam_darr_get(lstr, &chr, i);
			datam_darr_push(nstr, &chr);
		}
		uservar *var = new_seq(nstr, list);
		push_sym(pushable_var(var));
	}
	else if(right->type == list){
		int knock = (int)put_tofloat(left);
		datam_darr *nstr = datam_darr_new(sizeof(uservar*));
		datam_darr *lstr = right->values.list_val;
		if(knock < 0){
			knock = lstr->n + knock;
		}
		uservar *chr;
		for(int i = lstr->n - knock; i < lstr->n; i++){
			if(i){
				datam_darr_get(lstr, &chr, i);
				datam_darr_push(nstr, &chr);
			}
		}
		uservar *var = new_seq(nstr, list);
		push_sym(pushable_var(var));
	}
	else switch(com){
		case big_fixed:{
			put_tobigfix(op1, left);
			put_tobigfix(op2, right);
			binode_t *num = bigint_link();
			num->type = TYPE_BIGFIX;
			bisub(num, op1, op2);
			uservar *dif = new_bignum(num, big_fixed);
			push_sym(pushable_var(dif));
			break;
		}
		case big_integer:{
			put_tobigint(op1, left);
			put_tobigint(op2, right);
			binode_t *num = bigint_link();
			num->type = TYPE_BIGINT;
			bisub(num, op1, op2);
			uservar *dif = new_bignum(num, big_integer);
			push_sym(pushable_var(dif));
			break;
		}
		case native_float:{
			double l = put_tofloat(left), r = put_tofloat(right);
			uservar *dif = new_float(l - r);
			push_sym(pushable_var(dif));
			break;
		}
		case code_point:{
			int32_t val = left->values.uni_val - right->values.uni_val;
			uservar *dif = new_cp(val);
			push_sym(pushable_var(dif));
			break;
		}
		default:{
			quit_for_error(3, "Error: Invalid subtraction types\n");
		}
	}
}

void minus_basic(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	popped_minus_basic(left, right);
}

void popped_times_basic(uservar *left, uservar *right){
	switch(left->type){
		case list:{
			switch(right->type){
				case list:{
					datam_darr *llst = left->values.list_val;
					datam_darr *rlst = right->values.list_val;
					datam_darr *nlst = datam_darr_new(sizeof(uservar*));
					uservar *child;
					for(size_t i = 0; i < llst->n || i < rlst->n; i++){
						if(i < llst->n){
							datam_darr_get(llst, &child, i);
							datam_darr_push(nlst, &child);
						}
						if(i < rlst->n){
							datam_darr_get(rlst, &child, i);
							datam_darr_push(nlst, &child);
						}
					}
					uservar *var = new_seq(nlst, list);
					push_sym(pushable_var(var));
					break;
				}
				case wstring:
				case code_point:{
					datam_darr *rstr = put_tostring(right);
					rstr->n--;
					datam_darr *nstr = datam_darr_new(4);
					datam_darr *lst = left->values.list_val;
					uservar *child;
					for(size_t i = 0; i < lst->n; i++){
						if(i){
							datam_darr_pushall(nstr, rstr);
						}
						datam_darr_get(lst, &child, i);
						datam_darr *cstr = put_tostring(child);
						cstr->n--;
						datam_darr_pushall(nstr, cstr);
						datam_darr_delete(cstr);
					}
					datam_darr_delete(rstr);
					int32_t chr = 0;
					datam_darr_push(nstr, &chr);
					uservar *var = new_seq(nstr, wstring);
					push_sym(pushable_var(var));
					break;
				}
				case big_fixed:
				case big_integer:
				case native_float:{
					int times = (int)(put_tofloat(right));
					int sign = 1;
					if(times < 0){
						sign = -1;
						times = -times;
					}
					datam_darr *llst = left->values.list_val;
					datam_darr *nlst = datam_darr_new(sizeof(uservar*));
					int start = (sign == -1) ? llst->n - 1 : 0;
					int end = (sign == -1) ? -1 : llst->n;
					uservar *child;
					for(int i = 0; i < times; i++){
						for(int j = start; j != end; j += sign){
							datam_darr_get(llst, &child, j);
							datam_darr_push(nlst, &child);
						}
					}
					uservar *var = new_seq(nlst, list);
					push_sym(pushable_var(var));
					break;
				}
				default:{
					quit_for_error(3, "Error: Unsupported type for multiplication by list\n");
				}
			}
			break;
		}
		case wstring:
		case code_point:{
			switch(right->type){
				case list:{
					popped_times_basic(right, left);
					break;
				}
				case big_fixed:
				case big_integer:
				case code_point:
				case native_float:{
					datam_darr *lstr = put_tostring(left);
					int times = (int)(put_tofloat(right));
					datam_darr *str = datam_darr_new(4);
					int sign = 1;
					if(times < 0){
						times = -times;
						sign = -1;
					}
					int start = (sign == -1) ? lstr->n - 2 : 0;
					int end = (sign == -1) ? -1 : lstr->n - 1;
					uint32_t chr;
					for(int i = 0; i < times; i++){
						for(int j = start; j != end; j += sign){
							datam_darr_get(lstr, &chr, j);
							datam_darr_push(str, &chr);
						}
					}
					datam_darr_delete(lstr);
					chr = 0;
					datam_darr_push(str, &chr);
					uservar *var = new_seq(str, wstring);
					push_sym(pushable_var(var));
					break;
				}
				default:{
					quit_for_error(3, "Error: Unsupported type for multiplication by string-y\n");
				}
			}
			break;
		}
		case block_code:{
			switch(right->type){
				case big_fixed:
				case big_integer:
				case native_float:
				case code_point:{
					code_block *base = left->values.code_val;
					code_block *block = malloc(sizeof(code_block));
					block->members = datam_darr_new(sizeof(pushable));
					block->bound_vars = datam_darr_new(sizeof(uservar*));
					int times = (int)put_tofloat(right);
					if(times < 0)
						times = -times;
					pushable p;
					for(int i = 0; i < times; i++){
						datam_darr_pushall(block->bound_vars, base->bound_vars);
						for(int j = 0; j < base->members->n; j++){
							datam_darr_get(base->members, &p, j);
							p = clean_ref(p);
							datam_darr_push(block->members, &p);
						}
					}
					uservar *var = new_variable();
					var->type = block_code;
					var->values.code_val = block;
					push_sym(pushable_var(var));
					break;
				}
				default:{
					quit_for_error(3, "Error: Can only multiply code blocks by numbers!\n");
				}
			}
			break;
		}
		default:{
			var_type com = common_math_type(left->type, right->type);
			if(right->type == wstring || right->type == code_point || right->type == list || right->type == block_code){
				popped_times_basic(right, left);
			}
			else switch(com){
				case big_fixed:{
					put_tobigfix(op1, left);
					put_tobigfix(op2, right);
					binode_t *num = bigint_link();
					fp_kara(num, op1, op2);
					uservar *var = new_bignum(num, big_fixed);
					push_sym(pushable_var(var));
					break;
				}
				case big_integer:{
					put_tobigint(op1, left);
					put_tobigint(op2, right);
					binode_t *num = bigint_link();
					ip_kara(num, op1, op2);
					uservar *var = new_bignum(num, big_integer);
					push_sym(pushable_var(var));
					break;
				}
				case native_float:{
					float l = put_tofloat(left), r = put_tofloat(right);
					uservar *var = new_float(l * r);
					push_sym(pushable_var(var));
					break;
				}
				default:{
					quit_for_error(3, "Error: Multiplying invalid types\n");
				}
			}
		}
	}
}

void times_basic(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	popped_times_basic(left, right);
}

void popped_div_basic(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	switch(com){
		case big_fixed:{
			put_tobigfix(op1, left);
			put_tobigfix(op2, right);
			big_div(op3, op1, op2);
			uservar *var = new_bignum(op3, big_fixed);
			push_sym(pushable_var(var));
			break;
		}
		case big_integer:{
			put_tobigint(op1, left);
			put_tobigint(op2, right);
			idivmod(op3, NULL, op1, op2);
			uservar *var = new_bignum(op3, big_integer);
			push_sym(pushable_var(var));
			break;
		}
		case native_float:{
			float l = put_tofloat(left), r = put_tofloat(right);
			uservar *var = new_float(l / r);
			push_sym(pushable_var(var));
			break;
		}
		case code_point:{
			int32_t l = left->values.uni_val, r = right->values.uni_val;
			uservar *var = new_cp(l / r);
			push_sym(pushable_var(var));
			break;
		}
		default:{
			quit_for_error(3, "Error: Invalid types for basic division\n");
		}
	}
}

void div_basic(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	popped_div_basic(left, right);
}

void popped_mod_basic(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	if(left->type == wstring){
		datam_darr *lstr = left->values.list_val;
		datam_darr *nstr = datam_darr_new(4);
		int skip = (int)put_tofloat(right);
		if(skip <= 0)
			skip = 1;
		uint32_t chr;
		for(size_t i = 0; i < lstr->n - 1; i += skip){
			datam_darr_get(lstr, &chr, i);
			datam_darr_push(nstr, &chr);
		}
		chr = 0;
		datam_darr_push(nstr, &chr);
		uservar *var = new_seq(nstr, wstring);
		push_sym(pushable_var(var));
	}
	else if(left->type == list){
		datam_darr *lstr = left->values.list_val;
		datam_darr *nstr = datam_darr_new(sizeof(uservar*));
		int skip = (int)put_tofloat(right);
		if(skip <= 0)
			skip = 1;
		uservar* chr;
		for(size_t i = 0; i < lstr->n ; i += skip){
			datam_darr_get(lstr, &chr, i);
			datam_darr_push(nstr, &chr);
		}
		uservar *var = new_seq(nstr, list);
		push_sym(pushable_var(var));
	}
	else switch(com){
		case big_fixed:{
			put_tobigfix(op1, left);
			put_tobigfix(op2, right);
			binode_t *mod = bigint_link();
			idivmod(NULL, mod, op1, op2);
			uservar *var = new_bignum(mod, big_fixed);
			push_sym(pushable_var(var));
			break;
		}
		case big_integer:{
			put_tobigint(op1, left);
			put_tobigint(op2, right);
			binode_t *mod = bigint_link();
			idivmod(NULL, mod, op1, op2);
			uservar *var = new_bignum(mod, big_integer);
			push_sym(pushable_var(var));
			break;
		}
		case native_float:{
			float l = put_tofloat(left), r = put_tofloat(right);
			uservar *var = new_float(fmod(l, r));
			push_sym(pushable_var(var));
			break;
		}
		case code_point:{
			int32_t l = left->values.uni_val, r = right->values.uni_val;
			uservar *var = new_cp(l % r);
			push_sym(pushable_var(var));
			break;
		}
		default:{
			quit_for_error(3, "Error: Invalid modulus types\n");
		}
	}
}

void mod_basic(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	popped_mod_basic(left, right);
}

void popped_not(uservar *truth){
	int32_t tval = truth_val(truth);
	tval = !tval;
	uservar *var = new_cp(tval);
	push_sym(pushable_var(var));
}

void var_not(){
	uservar *var;
	pop_n_vars(0, 1, &var);
	popped_not(var);
}

void popped_abs_basic(uservar *var){
	uservar *ret = NULL;
	switch(var->type){
		case big_fixed:
		case big_integer:{
			binode_t *t = var->values.big_val, *d = bigint_link();
			bicpy(d, t);
			d->value->sign = 0;
			ret = new_bignum(d, var->type);
			break;
		}
		case native_float:{
			float a = fabs(var->values.float_val);
			ret = new_float(a);
			break;
		}
		case code_point:{
			int32_t a = var->values.uni_val;
			if(a < 0)
				a = -a;
			ret = new_cp(a);
			break;
		}
		case wstring:
		case list:{
			size_t n = var->values.list_val->n - (var->type == wstring ? 1 : 0);
			binode_t *d = bigint_link();
			ltobi(d, n);
			ret = new_bignum(d, big_integer);
			break;
		}
		case block_code:{
			size_t n = var->values.code_val->members->n;
			binode_t *d = bigint_link();
			ltobi(d, n);
			ret = new_bignum(d, big_integer);
			break;
		}
		default:{
			quit_for_error(3, "Error: Invalid type for absolute value!\n");
		}
	}
	push_sym(pushable_var(ret));
}

void abs_basic(){
	uservar *var;
	pop_n_vars(0, 1, &var);
	popped_abs_basic(var);
}

int32_t comp_top2(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	return uservar_cmp(left, right);
}

void popped_bit_and(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	switch(com){
		case big_fixed:
		case big_integer:{
			binode_t *lnum = left->values.big_val, *rnum = right->values.big_val;
			binode_t *bits = bigint_link();
			bicpy(bits, lnum);
			for(size_t i = 0; i < bigint_size; i++){
				bits->value->digits[i] &= rnum->value->digits[i];
			}
			uservar *var = new_bignum(bits, com);
			push_sym(pushable_var(var));
			break;
		}
		case code_point:{
			int32_t l = left->values.uni_val, r = right->values.uni_val;
			uservar *var = new_cp(l & r);
			push_sym(pushable_var(var));
			break;
		}
		case list:{
			datam_darr *llst = left->values.list_val, *rlst = right->values.list_val;
			datam_darr *nlst = datam_darr_new(sizeof(uservar*));
			for(size_t i = 0; i < llst->n || i < rlst->n; i++){
				uservar *lmem, *rmem;
				if(i < llst->n){
					datam_darr_get(llst, &lmem, i);
				}
				else{
					lmem = new_variable();
					lmem->type = empty;
				}
				if(i < rlst->n){
					datam_darr_get(rlst, &rmem, i);
				}
				else{
					rmem = new_variable();
					rmem->type = empty;
				}
				datam_darr *child = datam_darr_new(sizeof(uservar*));
				datam_darr_push(child, &lmem);
				datam_darr_push(child, &rmem);
				uservar *comp = new_seq(child, list);
				datam_darr_push(nlst, &comp);
			}
			uservar *var = new_seq(nlst, list);
			push_sym(pushable_var(var));
			break;
		}
		default:{
			quit_for_error(5, "Error: Invalid AND operands\n");
		}
	}
}

void bit_and(){
	uservar *left, *right;
	pop_n_vars(0, 2, &right, &left);
	popped_bit_and(left, right);
}

void popped_bit_or(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	switch(com){
		case big_fixed:
		case big_integer:{
			binode_t *lnum = left->values.big_val, *rnum = right->values.big_val;
			binode_t *bits = bigint_link();
			bicpy(bits, lnum);
			for(size_t i = 0; i < bigint_size; i++){
				bits->value->digits[i] |= rnum->value->digits[i];
			}
			uservar *var = new_bignum(bits, com);
			push_sym(pushable_var(var));
			break;
		}
		case code_point:{
			int32_t l = left->values.uni_val, r = right->values.uni_val;
			uservar *var = new_cp(l | r);
			push_sym(pushable_var(var));
			break;
		}
		case list:{
			datam_darr *llst = left->values.list_val, *rlst = right->values.list_val;
			datam_darr *nlst = datam_darr_new(sizeof(uservar*));
			uservar *lmem, *rmem;
			for(size_t i = 0; i < llst->n; i++){
				datam_darr_get(llst, &lmem, i);
				for(size_t j = 0; j < rlst->n; j++){
					datam_darr_get(rlst, &rmem, j);
					datam_darr *child = datam_darr_new(sizeof(uservar*));
					datam_darr_push(child, &lmem);
					datam_darr_push(child, &rmem);
					uservar *prod = new_seq(child, list);
					datam_darr_push(nlst, &prod);
				}
			}
			uservar *var = new_seq(nlst, list);
			push_sym(pushable_var(var));
			break;
		}
		default:{
			quit_for_error(5, "Error: Invalid OR operands\n");
		}
	}
}

void bit_or(){
	uservar *left, *right;
	pop_n_vars(0, 2, &right, &left);
	popped_bit_or(left, right);
}

void popped_bit_xor(uservar *left, uservar *right){
	var_type com = common_math_type(left->type, right->type);
	switch(com){
		case big_fixed:
		case big_integer:{
			binode_t *lnum = left->values.big_val, *rnum = right->values.big_val;
			binode_t *bits = bigint_link();
			bicpy(bits, lnum);
			for(size_t i = 0; i < bigint_size; i++){
				bits->value->digits[i] ^= rnum->value->digits[i];
			}
			uservar *var = new_bignum(bits, com);
			push_sym(pushable_var(var));
			break;
		}
		case code_point:{
			int32_t l = left->values.uni_val, r = right->values.uni_val;
			uservar *var = new_cp(l ^ r);
			push_sym(pushable_var(var));
			break;
		}
		default:{
			quit_for_error(5, "Error: Invalid XOR operands\n");
		}
	}
}

void bit_xor(){
	uservar *left, *right;
	pop_n_vars(0, 2, &right, &left);
	popped_bit_xor(left, right);
}

