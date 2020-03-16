#include <stdio.h>
#include "bigint.h"
#include "slavio.h"
#include "putter.h"
#include "putops.h"

void execute_builtin(uint32_t op){
	switch(op){
		case '\'':
			print_outright();
			break;
		case '+':
			plus_basic();
			break;
		case '-':
			minus_basic();
			break;
		case ']':
			push_sym(new_k_list(1));
			break;
		case '.':
			put_append();
			break;
		case '*':
			times_basic();
			break;
		case ':':
			assign();
			break;
		case '$':
			dup_top(1);
			break;
		case '@':
			get_n();
			break;
		case '(':
			resolve();
			break;
		case ')':
			execute_top();
			break;
	}
	if(heap->length >= gc_target){
		garbage_collect();
	}
}