/*
The running environment
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include "putter.h"
#include "bigint.h"
#include "slavio.h"
#include "slav.h"

datam_deque *heap = NULL;
datam_darr *stack = NULL;
datam_darr *shadow_stack = NULL;
datam_hashtable *global_vars = NULL;
datam_hashtable *local_vars = NULL;
datam_deque *var_frames = NULL;
size_t gc_target = 1;
uint32_t mode_flags = 0;
FILE *def_out = NULL;
FILE *def_in = NULL;
int type_out = TYPE_UTF8;
int type_in = TYPE_UTF8;
int srcfile_type = TYPE_UNKNOWN;
binode_t *op1, *op2, *op3, *op4;
char strbuf[1024];
uint32_t wstrbuf[1024];
int max_digits = 8;

int wstr_cmp(void *a, void *b){
	uint32_t *sa = (uint32_t*)a;
	uint32_t *sb = (uint32_t*)b;
	for(int i = 0; sa[i] != 0 || sb[i] != 0; i++){
		int dif = (int)(sa[i]) - (int)(sb[i]);
		if(dif)
			return dif;
	}
	return 0;
}

int32_t wstr_ind(void *data, size_t n){
	uint32_t *wstr = (uint32_t*)data;
	size_t len = wstrlen(wstr);
	int32_t ret = 0;
	for(size_t i = 0; i < len; i += n + 1){
		ret += wstr[i];
	}
	return ret;
}

void putter_init(){
	bigint_init(4);
	bigint_resize(4, 3);
	def_out = stdout;
	def_in = stdin;
	heap = datam_deque_new();
	stack = datam_darr_new(sizeof(pushable));
	shadow_stack = datam_darr_new(sizeof(uservar*));
	global_vars = datam_hashtable_new(wstr_ind, wstr_cmp, 8);
	local_vars = NULL;
	var_frames = datam_deque_new();
	gc_target = 1;
	op1 = bigint_link();
	op2 = bigint_link();
	op3 = bigint_link();
	op4 = bigint_link();
}

void putter_cleanup(){
	/* Free variable name spaces */
	for(datam_hashbucket *pair = datam_hashtable_next(global_vars, NULL); pair != NULL; pair = datam_hashtable_next(global_vars, pair)){
		uint32_t *key = (uint32_t*)(pair->key);
		if(key != NULL){
			free(key);
		}
		pair->key = NULL;
	}
	if(local_vars != NULL){
		for(datam_hashbucket *pair = datam_hashtable_next(local_vars, NULL); pair != NULL; pair = datam_hashtable_next(local_vars, pair)){
			uint32_t *key = (uint32_t*)(pair->key);
			if(key != NULL)
				free(key);
			pair->key = NULL;
		}
	}
	datam_hashtable *frame;
	for(datam_duonode *node = var_frames->head; node != NULL; node = node->next){
		frame = (datam_hashtable*)(node->data);
		if(frame != NULL){
			for(datam_hashbucket *pair = datam_hashtable_next(frame, NULL); pair != NULL; pair = datam_hashtable_next(frame, pair)){
				uint32_t *key = (uint32_t*)(pair->key);
				if(key != NULL)
					free(key);
				pair->key = NULL;
			}
		}
	}
	
	/* Clean out remaining variable references */
	pushable pushed;
	while(stack->n){
		datam_darr_pop(stack, &pushed);
		if(pushed.type == varname)
			free(pushed.values.name_val);
	}
	garbage_collect();
	
	/* Delete the global objects */
	datam_darr_delete(stack);
	
	datam_hashtable_delete(global_vars);
	if(local_vars != NULL){
		datam_hashtable_delete(local_vars);
	}
	for(datam_duonode *node = var_frames->head; node != NULL; node = node->next){
		frame = (datam_hashtable*)(node->data);
		if(frame != NULL){
			datam_hashtable_delete(frame);
		}
	}
	datam_deque_delete(var_frames);
	datam_deque_delete(heap);
	datam_darr_delete(shadow_stack);
	bigint_destroy();
}

void push_sym(pushable sym){
	switch(sym.type){
		case literal:{ /* Push a variable object */
			datam_darr_push(stack, &sym);
			break;
		}
		case varname:{ /* Just push the variable name reference (clone the name) */
			size_t len = wstrlen(sym.values.name_val);
			uint32_t *copy = malloc(4 * (len + 1));
			memcpy(copy, sym.values.name_val, (len + 1) * 4);
			sym.values.name_val = copy;
			datam_darr_push(stack, &sym);
			break;
		}
		case builtin:{ /* Perform an operation */
			execute_builtin(sym.values.op_val);
			break;
		}
	}
}

void execute_block(code_block *block){
	size_t n = block->bound_vars->n;
	uservar *var;
	size_t old_pos = shadow_stack->n;
	for(size_t i = 0; i < n; i++){
		datam_darr_get(block->bound_vars, &var, i);
		datam_darr_push(shadow_stack, &var);
	}
	n = block->members->n;
	pushable push;
	for(size_t i = 0; i < n; i++){
		datam_darr_get(block->members, &push, i);
		push_sym(push);
	}
	shadow_stack->n = old_pos;
}

pushable pop_pushable(){
	if(stack->n == 0){
		quit_for_error(4, "Error: Stack is empty!\n");
	}
	pushable popped;
	size_t i = stack->n - 1;
	datam_darr_get(stack, &popped, i);
	if(!(mode_flags & CLONE_MODE))
		stack->n--;
	if(mode_flags & VOLATILE_MODE)
		mode_flags ^= CLONE_MODE;
	return popped;
}

uservar* resolve_var(pushable tok){
	static char plain[1024];
	if(tok.type == literal)
		return tok.values.lit_val;
	uint32_t *name = tok.values.name_val;
	uservar *var = NULL;
	if(local_vars != NULL){
		var = (uservar*)(datam_hashtable_get(local_vars, name));
		if(var != NULL){
			return var;
		}
	}
	var = (uservar*)(datam_hashtable_get(global_vars, name));
	if(var != NULL){
		return var;
	}
	size_t len = wstrlen(name);
	for(size_t i = 0; i <= len; i++)
		plain[i] = name[i];
	free(name);
	quit_for_error(5, "Error: Cannot resolve variable name: %s\n", plain);
}

void discard_popped(pushable pop){
	if(pop.type == varname)
		free(pop.values.name_val);
}

uservar* pop_var(){
	pushable pop = pop_pushable();
	uservar *var = resolve_var(pop);
	discard_popped(pop);
	return var;
}

void pop_n_vars(int allow_empty, size_t n, ...){
	va_list args;
	va_start(args, n);
	for(size_t i = 0; i < n; i++){
		uservar *var = pop_var();
		if(var->type == empty && !allow_empty)
			quit_for_error(6, "Popped empty variable\n");
		uservar **target = va_arg(args, uservar**);
		*target = var;
	}
}

static FILE *open_src = NULL;

void quit_for_error(int code, const char *errmsg, ...){
	va_list args;
	va_start(args, errmsg);
	vfprintf(stderr, errmsg, args);
	putter_cleanup();
	if(open_src != NULL)
		fclose(open_src);
	exit(code);
}

void run_file(FILE *src){
	open_src = src;
	srcfile_type = fget_utf_type(src);
	putter_init();
	code_block *code = parse_block(src);
	uservar *codevar = new_variable();
	codevar->type = block_code;
	codevar->values.code_val = code;
	
	uint32_t *mainkey = malloc(20);
	memcpy(mainkey, (uint32_t[]){'m', 'a', 'i', 'n', 0}, 20);
	datam_hashtable_put(global_vars, mainkey, codevar);
	
	execute_block(code);
	
	putter_cleanup();
	open_src = NULL;
}

int main(){
	FILE *src = fopen("prog.txt", "r");
	run_file(src);
	fclose(src);
	return 0;
}