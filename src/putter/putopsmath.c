/*
IO functions
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "slav.h"
#include "bigint.h"
#include "slavio.h"
#include "putter.h"
#include "putops.h"

/* Appends strings and adds code points to create a string */
void popped_plus_basic(uservar *left, uservar *right){
	if(left->type == wstring || right->type == wstring){
		datam_darr *lstr = put_tostring(left), *rstr = put_tostring(right);
		lstr->n--;
		datam_darr_pushall(lstr, rstr);
		datam_darr_delete(rstr);
		uservar *var = new_seq(lstr, wstring);
		push_sym(pushable_var(var));
	}
	else if(left->type == big_fixed || right->type == big_fixed){
		put_tobigfix(op1, left);
		put_tobigfix(op2, right);
		binode_t *num = bigint_link();
		num->type = TYPE_BIGFIX;
		biadd(num, op1, op2);
		uservar *var = new_bignum(num, big_fixed);
		push_sym(pushable_var(var));
	}
	else if(left->type == big_integer || right->type == big_integer){
		put_tobigint(op1, left);
		put_tobigint(op2, right);
		binode_t *num = bigint_link();
		num->type = TYPE_BIGINT;
		biadd(num, op1, op2);
		uservar *var = new_bignum(num, big_integer);
		push_sym(pushable_var(var));
	}
	else if(left->type == native_float || right->type == native_float){
		double l = put_tofloat(left), r = put_tofloat(right);
		uservar *var = new_float(l + r);
		push_sym(pushable_var(var));
	}
	else if(left->type == code_point && right->type == code_point){
		int32_t cp = left->values.uni_val + right->values.uni_val;
		uservar *sum = new_cp(cp);
		push_sym(pushable_var(sum));
	}
	else{
		switch(left->type){
			case block_code:{
				if(right->type != block_code){
					fprintf(stderr, "Error: Cannot add code and non-code\n");
					push_sym((pushable){.type = nothing});
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
					fprintf(stderr, "Error: Cannot add non-list to list\n");
					push_sym((pushable){.type = nothing});
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
		}
	}
}

void plus_basic(){
	uservar *right = pop_var();
	if(right == NULL)
		return;
	uservar *left = pop_var();
	if(left == NULL)
		return;
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
	uservar *right = pop_var();
	if(right == NULL)
		return;
	uservar *left = pop_var();
	if(left == NULL)
		return;
	popped_append(left, right);
}

void popped_minus_basic(uservar *left, uservar *right){
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
		pushable chr;
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
		pushable chr;
		for(int i = lstr->n - knock; i < lstr->n; i++){
			if(i){
				datam_darr_get(lstr, &chr, i);
				datam_darr_push(nstr, &chr);
			}
		}
		uservar *var = new_seq(nstr, list);
		push_sym(pushable_var(var));
	}
	else if(left->type == big_fixed || right->type == big_fixed){
		put_tobigfix(op1, left);
		put_tobigfix(op2, right);
		binode_t *num = bigint_link();
		num->type = TYPE_BIGFIX;
		bisub(num, op1, op2);
		uservar *dif = new_bignum(num, big_fixed);
		push_sym(pushable_var(dif));
	}
	else if(left->type == big_integer || right->type == big_integer){
		put_tobigint(op1, left);
		put_tobigint(op2, right);
		binode_t *num = bigint_link();
		num->type = TYPE_BIGINT;
		bisub(num, op1, op2);
		uservar *dif = new_bignum(num, big_integer);
		push_sym(pushable_var(dif));
	}
	else if(left->type == native_float || right->type == native_float){
		double l = put_tofloat(left), r = put_tofloat(right);
		uservar *dif = new_float(l - r);
		push_sym(pushable_var(dif));
	}
	else if(left->type == code_point && right->type == code_point){
		int32_t val = left->values.uni_val - right->values.uni_val;
		uservar *dif = new_cp(val);
		push_sym(pushable_var(dif));
	}
	else{
		fprintf(stderr, "Error: Invalid subtraction types\n");
		push_sym((pushable){.type = nothing});
	}
}

void minus_basic(){
	uservar *right = pop_var();
	if(right == NULL)
		return;
	uservar *left = pop_var();
	if(left == NULL)
		return;
	popped_minus_basic(left, right);
}

void popped_times_basic(uservar *left, uservar *right){
	switch(left->type){
		case list:{
			switch(right->type){
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
					fprintf(stderr, "Error: Unsupported type for multiplication by list\n");
					push_sym((pushable){.type = nothing});
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
					fprintf(stderr, "Error: Unsupported type for multiplication by string-y\n");
					push_sym((pushable){.type = nothing});
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
					fprintf(stderr, "Error: Can only multiply code blocks by numbers!\n");
					push_sym((pushable){.type = nothing});
				}
			}
			break;
		}
		default:{
			if(right->type == wstring || right->type == code_point || right->type == list || right->type == block_code){
				popped_times_basic(right, left);
			}
			else{
				if(left->type == big_fixed || right->type == big_fixed){
					put_tobigfix(op1, left);
					put_tobigfix(op2, right);
					fp_kara(op3, op1, op2);
					uservar *var = new_bignum(op3, big_fixed);
					push_sym(pushable_var(var));
				}
				else if(left->type == big_integer || right->type == big_integer){
					put_tobigint(op1, left);
					put_tobigint(op2, right);
					ip_kara(op3, op1, op2);
					uservar *var = new_bignum(op3, big_integer);
					push_sym(pushable_var(var));
				}
				else if(left->type == native_float || right->type == native_float){
					float l = put_tofloat(left), r = put_tofloat(right);
					uservar *var = new_float(l * r);
					push_sym(pushable_var(var));
				}
				else{
					fprintf(stderr, "Error: Multiplying invalid types\n");
					push_sym((pushable){.type = nothing});
				}
			}
		}
	}
}

void times_basic(){
	uservar *right = pop_var();
	if(right == NULL)
		return;
	uservar *left = pop_var();
	if(left == NULL)
		return;
	popped_times_basic(left, right);
}