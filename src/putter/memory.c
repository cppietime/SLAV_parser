/*
Memory management I guess
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "putter.h"
#include "putops.h"
#include "slavio.h"

uservar* new_variable(){
	uservar *var = malloc(sizeof(uservar));
	var->type = empty;
	var->flags = FLAG_MARKED | FLAG_HASH | FLAG_KEY;
	datam_deque_queuet(heap, var);
	return var;
}

uservar* new_bignum(binode_t *num, var_type type){
	uservar *ret = new_variable();
	ret->type = type;
	ret->values.big_val = num;
	return ret;
}

uservar* new_seq(datam_darr *seq, var_type type){
	uservar *ret = new_variable();
	ret->type = type;
	ret->values.list_val = seq;
	return ret;
}

uservar* new_cp(int32_t cp){
	uservar *ret = new_variable();
	ret->type = code_point;
	ret->values.uni_val = cp;
	return ret;
}

uservar* new_float(double flt){
	uservar *ret = new_variable();
	ret->type = native_float;
	ret->values.float_val = flt;
	return ret;
}

uservar* new_map(datam_hashtable *map){
	uservar *ret = new_variable();
	ret->type = hash_table;
	ret->values.map_val = map;
	return ret;
}

pushable pushable_var(uservar *var){
	if(var == NULL){
		quit_for_error(1, "Error: Pushing non-existent variable!\n");
	}
	return (pushable){.type = literal, .values = {.lit_val = var}};
}

void sweep_heap(){
	for(datam_duonode *head = heap->head; head != NULL; head = head->next){
		uservar *variable = (uservar*)(head->data);
		variable->flags &= ~FLAG_MARKED;
	}
}

void mark_variable(uservar *t){
	if(t == NULL)
		return;
	if((t->flags & FLAG_MARKED))
		return;
	t->flags |= FLAG_MARKED;
	switch(t->type){
		case list:{
			datam_darr *children = t->values.list_val;
			mark_list(children);
			break;
		}
		case block_code:{
			code_block *code = t->values.code_val;
			mark_block(code);
			break;
		}
		case hash_table:{
			datam_hashtable *table = t->values.map_val;
			mark_table(table);
			break;
		}
	}
}

void mark_list(datam_darr *children){
	if(children == NULL)
		return;
	uservar *child;
	size_t n = children->n;
	for(size_t i = 0; i < n; i++){
		datam_darr_get(children, &child, i);
		mark_variable(child);
	}
}

void mark_block(code_block *code){
	if(code == NULL)
		return;
	size_t n = code->bound_vars->n;
	uservar *var;
	for(int i = 0; i < n; i++){
		datam_darr_get(code->bound_vars, &var, i);
		mark_variable(var);
	}
	pushable member;
	n = code->members->n;
	for(int i = 0; i < n; i++){
		datam_darr_get(code->members, &member, i);
		if(member.type == literal){
			mark_variable(member.values.lit_val);
		}
	}
}

void mark_table(datam_hashtable *table){
	for(datam_hashbucket *pair = datam_hashtable_next(table, NULL); pair != NULL; pair = datam_hashtable_next(table, pair)){
		uservar *key = (uservar*)(pair->key);
		mark_variable(key);
		uservar *value = (uservar*)(pair->value);
		mark_variable(value);
	}
}

void clean_variable(uservar *t){
	if(t == NULL)
		return;
	switch(t->type){
		case big_integer:
		case big_fixed:
			bigint_unlink(t->values.big_val);
			break;
		case list:
			clean_list(t->values.list_val);
			break;
		case wstring:
			datam_darr_delete(t->values.list_val);
			break;
		case open_file:
			clean_file(t->values.file_val);
			break;
		case block_code:
			clean_block(t->values.code_val);
			break;
		case hash_table:
			clean_table(t->values.map_val);
			break;
	}
}

void clean_list(datam_darr *list){
	if(list == NULL)
		return;
	datam_darr_delete(list);
}

void clean_file(file_wrapper *wrap){
	if(wrap == NULL)
		return;
	if(wrap->file == def_out)
		def_out = stdout;
	if(wrap->file == def_in)
		def_in = stdin;
	if(wrap->status & STATUS_OPEN)
		fclose(wrap->file);
	if(wrap->path != NULL)
		free(wrap->path);
	free(wrap);
}

void clean_table(datam_hashtable *table){
	if(table == NULL)
		return;
	datam_hashtable_delete(table);
}

void clean_block(code_block *code){
	if(code == NULL)
		return;
	datam_darr_delete(code->bound_vars);
	size_t n = code->members->n;
	pushable member;
	for(int i = 0; i < n; i++){
		datam_darr_get(code->members, &member, i);
		if(member.type == varname){
			free(member.values.name_val);
		}
	}
	datam_darr_delete(code->members);
	free(code);
}

void garbage_collect(){
	/* Sweep/unmark everything */
	sweep_heap();
	size_t n = stack->n;
	uservar *var;
	pushable pushed;
	
	/* Mark reachable vars */
	/* Stack vars */
	for(int i = 0; i < n; i++){
		datam_darr_get(stack, &pushed, i);
		if(pushed.type == literal){
			var = pushed.values.lit_val;
			mark_variable(var);
		}
	}
	/* Shadow stack */
	n = shadow_stack->n;
	for(int i = 0; i < n; i++){
		datam_darr_get(shadow_stack, &var, i);
		mark_variable(var);
	}
	
	/* Global vars */
	for(datam_hashbucket *glob = datam_hashtable_next(global_vars, NULL); glob != NULL; glob = datam_hashtable_next(global_vars, glob)){
		var = (uservar*)(glob->value);
		if(var != NULL){
			mark_variable(var);
		}
	}
	
	/* Local vars */
	if(local_vars != NULL){
		for(datam_hashbucket *loc = datam_hashtable_next(local_vars, NULL); loc != NULL; loc = datam_hashtable_next(local_vars, loc)){
			var = (uservar*)(loc->value);
			if(var != NULL)
				mark_variable(var);
		}
	}
	
	/* Pushed local frames */
	for(datam_duonode *node = var_frames->head; node != NULL; node = node->next){
		datam_hashtable *frame = (datam_hashtable*)(node->data);
		if(frame != NULL){
			for(datam_hashbucket *loc = datam_hashtable_next(frame, NULL); loc != NULL; loc = datam_hashtable_next(frame, loc)){
				var = (uservar*)(loc->value);
				if(var != NULL)
					mark_variable(var);
			}
		}
	}
	
	/* Cleanup the yet unmarked variables */
	datam_duonode *next = NULL;
	for(datam_duonode *node = heap->head; node != NULL; node = next){
		next = node->next;
		var = (uservar*)(node->data);
		if(var == NULL)
			continue;
		if((var->flags & FLAG_MARKED))
			continue;
		clean_variable(var);
		free(var);
		datam_duonode *prev = node->prev;
		if(prev == NULL)
			heap->head = next;
		else
			prev->next = next;
		if(next == NULL)
			heap->tail = prev;
		else
			next->prev = prev;
		free(node);
		heap->length --;
	}
	
	/* Set the new GC target */
	gc_target = heap->length * 2;
}

/* Get a clean refernce to a pushable if a var name */
pushable clean_ref(pushable base){
	if(base.type == varname){
		size_t len = wstrlen(base.values.name_val);
		uint32_t *newname = malloc(4 * (len + 1));
		memcpy(newname, base.values.name_val, 4 * (len + 1));
		base.values.name_val = newname;
	}
	return base;
}

uservar* clone_variable(uservar *ret, uservar *t, int deep){
	switch(t->type){
		case code_point:
		case empty:
		case native_float:
			memcpy(ret, t, sizeof(uservar));
			break;
		case open_file:{
			file_wrapper *tf = t->values.file_val;
			char *npath = malloc(strlen(tf->path) + 1);
			strcpy(npath, tf->path);
			file_wrapper *fw = new_file(npath);
			fw->encoding = tf->encoding;
			fw->status = tf->status;
			fw->status &= ~STATUS_OPEN;
			ret->type = open_file;
			ret->values.file_val = fw;
			break;
		}
		case big_integer:
		case big_fixed:{
			binode_t *base = t->values.big_val;
			binode_t *copy = bigint_link();
			bicpy(copy, base);
			ret->type = t->type;
			ret->values.big_val = copy;
			break;
		}
		case wstring:{
			datam_darr *newstr = datam_darr_new(4);
			datam_darr *oldstr = t->values.list_val;
			datam_darr_pushall(newstr, oldstr);
			ret->type = wstring;
			ret->values.list_val = newstr;
			break;
		}
		case list:{
			datam_darr *newlst = datam_darr_new(sizeof(uservar*));
			datam_darr *oldlst = t->values.list_val;
			uservar *child;
			for(size_t i = 0; i < oldlst->n; i++){
				datam_darr_get(oldlst, &child, i);
				uservar *copy = child;
				if(deep)
					copy = clone_variable(new_variable(), child, deep);
				datam_darr_push(newlst, &copy);
			}
			ret->type = list;
			ret->values.list_val = newlst;
			break;
		}
		case block_code:{
			code_block *oldcode = t->values.code_val;
			code_block *newcode = malloc(sizeof(code_block));
			newcode->members = datam_darr_new(sizeof(pushable));
			newcode->bound_vars = datam_darr_new(sizeof(uservar*));
			pushable push;
			for(size_t i = 0; i < oldcode->members->n; i++){
				datam_darr_get(oldcode->members, &push, i);
				push = clean_ref(push);
				datam_darr_push(newcode->members, &push);
			}
			uservar *var;
			for(size_t i = 0; i < oldcode->bound_vars->n; i++){
				datam_darr_get(oldcode->bound_vars, &var, i);
				uservar *copy = clone_variable(new_variable(), var, deep);
				datam_darr_push(newcode->bound_vars, &copy);
			}
			ret->type = block_code;
			ret->values.code_val = newcode;
			break;
		}
		default:{
			fprintf(stderr, "Error: Unsupported type so far for cloning: %d\n", t->type);
		}
	}
	return ret;
}