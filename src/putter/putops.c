#include <stdio.h>
#include "bigint.h"
#include "slavio.h"
#include "putter.h"
#include "putops.h"

void execute_builtin(uint32_t op){
	switch(op){
		/* Math operators */
		case '+':
			plus_basic();
			break;
		case '-':
			minus_basic();
			break;
		case '*':
			times_basic();
			break;
		case '/':
			div_basic();
			break;
		case '%':
			mod_basic();
			break;
		case '.':
			put_append();
			break;
		case '!':
			var_not();
			break;
		case '~':
			abs_basic();
			break;
		case '>':{
			int32_t d = comp_top2();
			uservar *res = new_cp(d > 0);
			push_sym(pushable_var(res));
			break;
		}
		case '<':{
			int32_t d = comp_top2();
			uservar *res = new_cp(d < 0);
			push_sym(pushable_var(res));
			break;
		}
		case '=':{
			int32_t d = comp_top2();
			uservar *res = new_cp(d == 0);
			push_sym(pushable_var(res));
			break;
		}
		case '&':
			bit_and();
			break;
		case '|':
			bit_or();
			break;
		case '^':
			bit_xor();
			break;
		
		/* IO operators */
		case '\'':
			print_outright();
			break;
		case '_':
			push_sym(pushable_var(input_wstring('\n')));
			break;
		
		/* Data operators */
		case ':':
			assign();
			break;
		case '$':
			dup_top(1);
			break;
		case '@':
			get_n();
			break;
		case ';':
			resolve();
			break;
		case ',':
			swap_2nd_with_top();
			break;
		case ']':
			push_sym(new_k_list(1));
			break;
		case '\\':
			match_top2();
			break;
		case '`':
			discard_popped(pop_pushable());
			break;
		
		/* Control flow operators */
		case ')':
			execute_top();
			break;
		case '?':
			execute_2nd_if_top();
			break;
		case '(':
			execute_while();
	}
	if(heap->length >= gc_target){
		garbage_collect();
	}
}