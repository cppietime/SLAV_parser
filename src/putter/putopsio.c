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
	if(var == NULL)
		return;
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
	uservar *var = pop_var();
	popped_print_outright(var);
}

void assign(){
	uservar *value = pop_var();
	if(value == NULL)
		return;
	pushable recip;
	datam_darr_pop(stack, &recip);
	if(recip.type == varname){ /* Assign a variable */
		uint32_t *key = recip.values.name_val;
		if(local_vars != NULL){
			datam_hashtable_put(local_vars, key, value);
		}
		else{
			datam_hashtable_put(global_vars, key, value);
		}
	}
	else{
		uservar *target = recip.values.lit_val;
		clean_variable(target);
		clone_variable(target, value);
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
	int id = (int)put_tofloat(right);
	if(left->type == list){
		uservar *var;
		datam_darr_get(left->values.list_val, &var, id);
		push_sym(pushable_var(var));
	}
	else if(left->type == wstring){
		int32_t ch;
		datam_darr_get(left->values.list_val, &ch, id);
		uservar *var = new_cp(ch);
		push_sym(pushable_var(var));
	}
	else{
		fprintf(stderr, "Error: Cannot index non-sequence\n");
		push_sym((pushable){.type = nothing});
	}
}

void get_n(){
	uservar *right = pop_var();
	if(right == NULL)
		return;
	uservar *left = pop_var();
	if(left == NULL)
		return;
	popped_get_n(left, right);
}

void resolve(){
	uservar *var = pop_var();
	push_sym(pushable_var(var));
}

void execute_top(){
	uservar *var = pop_var();
	if(var == NULL || var->type != block_code){
		fprintf(stderr, "Error: Executing non-code\n");
	}else{
		if(local_vars != NULL){
			datam_deque_queueh(var_frames, local_vars);
		}
		local_vars = datam_hashtable_new(wstr_ind, wstr_cmp, 8);
		uservar *tmp = clone_variable(new_variable(), var);
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
			local_vars = (datam_hashtable*)datam_deque_dequeueh(var_frames);
		}
	}
}