/*
IO functions
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bigint.h"
#include "slavio.h"
#include "putter.h"
#include "putops.h"

/* Internal printing function for object */
void popped_print_outright(uservar *var){
	datam_darr *strobj = put_tostring(var);
	uint32_t chr;
	for(size_t i = 0; i < strobj->n; i++){
		datam_darr_get(strobj, &chr, i);
		if(chr == 0)
			break;
		fput_unicode(def_out, type_out, chr);
	}
	datam_darr_delete(strobj);
}

void print_outright(){
	uservar *var;
	pop_n_vars(1, 1, &var);
	popped_print_outright(var);
}

void assign(){
	uservar *value;
	pop_n_vars(1, 1, &value);
	pushable recip;
	datam_darr_pop(stack, &recip);
	if(recip.type == varname){ /* Assign a variable */
		uint32_t *key = recip.values.name_val;
		if(local_vars != NULL){
			if(!datam_hashtable_put(local_vars, key, value))
				free(key);
		}
		else{
			if(!datam_hashtable_put(global_vars, key, value))
				free(key);
		}
	}
	else{
		uservar *target = recip.values.lit_val;
		clean_variable(target);
		clone_variable(target, value, 0);
	}
}

void dup_top(size_t num){
	size_t end = stack->n, start = stack->n - num;
	pushable p;
	for(size_t i = start; i < end; i ++){
		datam_darr_get(stack, &p, i);
		p = clean_ref(p);
		datam_darr_push(stack, &p);
	}
}

void popped_get_n(uservar *left, uservar *right){
	if(left->type == hash_table){
		uservar *val = (uservar*)(datam_hashtable_get(left->values.map_val, right));
		if(val == NULL){
			push_sym(pushable_var(new_variable()));
		}
		else{
			push_sym(pushable_var(val));
		}
	}
	else if(left->type == wstring){
		datam_darr *lstr = left->values.list_val;
		switch(right->type){
			case big_integer:
			case big_fixed:
			case native_float:{
				int index = (int)put_tofloat(right);
				if(index < 0)
					index += lstr->n - 1;
				if(index < 0 || index >= lstr->n - 1)
					quit_for_error(10, "Error: Index %lu out of range %lu\n", index, lstr->n - 1);
				int32_t ch;
				datam_darr_get(lstr, &ch, index);
				uservar *var = new_cp(ch);
				push_sym(pushable_var(var));
				break;
			}
			case code_point:{
				int32_t ch, sch = right->values.uni_val;
				size_t i;
				for(i = 0; i < lstr->n - 1; i++){
					datam_darr_get(lstr, &ch, i);
					if(ch == sch)
						break;
				}
				uservar *var;
				if(i == lstr->n - 1)
					var = new_variable();
				else{
					binode_t *ind = bigint_link();
					ltobi(ind, i);
					var = new_bignum(ind, big_integer);
				}
				push_sym(pushable_var(var));
				break;
			}
			case wstring:{
				datam_darr *rstr = right->values.list_val;
				uint32_t *wl = lstr->data, *wr = rstr->data;
				uint32_t *pos = wstrpos(wl, wr);
				uservar *var;
				if(pos == NULL)
					var = new_variable();
				else{
					binode_t *ind = bigint_link();
					ltobi(ind, pos - wl);
					var = new_bignum(ind, big_integer);
				}
				push_sym(pushable_var(var));
				break;
			}
			default:
				quit_for_error(4, "Error: Invalid indexing operands\n");
		}
	}
	else{
		int id = (int)put_tofloat(right);
		if(left->type == list){
			uservar *var;
			datam_darr_get(left->values.list_val, &var, id);
			push_sym(pushable_var(var));
		}
		else if(left->type == open_file){
			datam_darr *wstr = read_from_file(left->values.file_val, id);
			uservar *var = new_seq(wstr, wstring);
			push_sym(pushable_var(var));
		}
		else{
			quit_for_error(4, "Error: Invalid indexing operands\n");
		}
	}
}

void get_n(){
	uservar *right, *left;
	pop_n_vars(0, 2, &right, &left);
	popped_get_n(left, right);
}

void resolve(){
	uservar *var;
	pop_n_vars(0, 1, &var);
	push_sym(pushable_var(var));
}

void execute_top(){
	uservar *var;
	pop_n_vars(0, 1, &var);
	if(var->type == block_code){
		if(local_vars != NULL){
			datam_deque_queueh(var_frames, local_vars);
		}
		local_vars = datam_hashtable_new(wstr_ind, wstr_cmp, 8);
		uservar *tmp = clone_variable(new_variable(), var, 0);
		static char lockey[] = "@@current";
		size_t len = strlen(lockey);
		uint32_t *key = malloc(4 * (len + 1));
		wstrpad(key, lockey);
		datam_hashtable_put(local_vars, key, tmp);
		code_block *code = tmp->values.code_val;
		execute_block(code);
		for(datam_hashbucket *pair = datam_hashtable_next(local_vars, NULL); pair != NULL; pair = datam_hashtable_next(local_vars, pair)){
			uint32_t *key = (uint32_t*)(pair->key);
			if(key != NULL)
				free(key);
		}
		datam_hashtable_delete(local_vars);
		local_vars = NULL;
		if(var_frames->length){
			datam_duonode *node = datam_deque_dequeueh(var_frames);
			local_vars = (datam_hashtable*)(node->data);
			free(node);
		}
	}
	else if(var->type == wstring){
		FILE *buf = tmpfile();
		datam_darr *wstr = var->values.list_val;
		uint32_t ch;
		for(size_t i = 0; i < wstr->n; i++){
			datam_darr_get(wstr, &ch, i);
			fput_unicode(buf, TYPE_UTF8, ch);
		}
		fflush(buf);
		rewind(buf);
		int old = srcfile_type;
		srcfile_type = TYPE_UTF8;
		code_block *code = parse_block(buf);
		fclose(buf);
		uservar *block = new_variable();
		block->type = block_code;
		block->values.code_val = code;
		push_sym(pushable_var(block));
	}
	else
		quit_for_error(5, "Error: Executing non-code\n");
}

void execute_2nd_if_top(){
	uservar *var;
	pop_n_vars(1, 1, &var);
	int truth = truth_val(var);
	if(truth)
		execute_top();
	else
		pop_var();
}

void swap_x_with_n(int x, int n){
	n = stack->n - 1 - n;
	x = stack->n - 1 - x;
	if(n < 0 || n >= stack->n){
		fprintf(stderr, "Warning: Index %lu is out of range, set to 0\n", stack->n - 1 - n);
		n = 0;
	}
	if(x < 0 || x >= stack->n){
		fprintf(stderr, "Warning: Index %lu is out of range, set to 0\n", stack->n - 1 - x);
		x = 0;
	}
	if(n == x)
		return;
	pushable a;
	pushable b;
	datam_darr_get(stack, &a, x);
	datam_darr_get(stack, &b, n);
	datam_darr_set(stack, &b, x);
	datam_darr_set(stack, &a, n);
}

void swap_2nd_with_top(){
	uservar *var;
	pop_n_vars(0, 1, &var);
	int ind = (int)put_tofloat(var);
	swap_x_with_n(0, ind);
}

void execute_while(){
	uservar *cond_var, *func_var;
	pop_n_vars(0, 2, &cond_var, &func_var);
	if(cond_var->type != block_code || func_var->type != block_code)
		quit_for_error(5, "Error: Executing non-code\n");
	datam_hashtable *tab = global_vars;
	if(local_vars != NULL)
		tab = local_vars;
	static char funckey[] = "@@func", condkey[] = "@@condition";
	size_t len = strlen(funckey);
	uint32_t *fkey = malloc(4 * (len + 1));
	wstrpad(fkey, funckey);
	len = strlen(condkey);
	uint32_t *ckey = malloc(4 * (len + 1));
	wstrpad(ckey, condkey);
	datam_hashtable_put(tab, fkey, func_var);
	datam_hashtable_put(tab, ckey, cond_var);
	while(1){
		uservar *should = clone_variable(new_variable(), cond_var, 0);
		push_sym(pushable_var(should));
		execute_top();
		uservar *res = pop_var();
		int truth = truth_val(res);
		if(truth){
			uservar *run = clone_variable(new_variable(), func_var, 0);
			push_sym(pushable_var(run));
			execute_top();
		}
		else break;
	}
	datam_hashtable_remove(tab, fkey);
	datam_hashtable_remove(tab, ckey);
	free(fkey);
	free(ckey);
}

uservar *input_wstring(int32_t delim){
	int32_t ch;
	datam_darr *wstr = datam_darr_new(4);
	while((ch = fgetc(def_in)) != delim){
		datam_darr_push(wstr, &ch);
	}
	ch = 0;
	datam_darr_push(wstr, &ch);
	return new_seq(wstr, wstring);
}