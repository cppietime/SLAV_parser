/*
Oh boy this should be fun. I'm about to write a deadass interpreter for a code golfing language with all sorts of shit aww jeez
*/

#ifndef _H_PUTTER
#define _H_PUTTER

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "datam.h"
#include "bigint.h"
#include "slav.h"
#include "putter_lang.h"

/* ====================================================== Data types used by Putter ======================================================== */

/* Forward variable definition */
typedef struct _uservar uservar;

/* Types of pushables */
typedef enum _push_type{
	literal, /* A literal value */
	builtin, /* Built-in operators */
	varname, /* Name of a variable */
	nothing, /* Just nothing */
	num_total_push_types
} push_type;

/* Components of code blocks */
typedef struct _pushable{
	push_type type;
	union {
		uservar *lit_val;
		uint32_t op_val;
		uint32_t *name_val;
	} values;
} pushable;

#define STATUS_OPEN 1

/* Wrapped file */
typedef struct _file_wrapper{
	FILE *file; /* Contained file */
	int status; /* Status bits */
	int bitptr; /* Ptr to bit writing/reading */
	int bitbuf; /* Buffer for bit writing/reading */
} file_wrapper;

/* Chunk of executable code */
typedef struct _code_block{
	datam_darr *bound_vars; /* Curried variables */
	datam_darr *members; /* Member pushable items (tokens), n in total */
} code_block;

/* Types of variables */
typedef enum _var_type{
	empty, /* Empty variable */
	code_point, /* 32-bit unsigned integer for unicode characters */
	big_integer, /* arbitrary precision integer */
	big_fixed, /* arbitrary precision fixed-point binary */
	native_float, /* normal fixed-precision float */
	list, /* dynamic array list */
	wstring, /* Wide string, an array list of code points instead of objects */
	open_file, /* a wrapped file object */
	block_code, /* "executable" code */
	hash_table, /* implementation of a hash table, requires a hashing function to be passed.
						For functionality, the keys will have to be non-mutated, but that will be up to the programmer, I guess... */
						
	num_total_var_types /* this must always be last! It is equal to the number of used variable types. */
} var_type;

#define FLAG_MARKED 1
#define FLAG_RECUR 2
#define FLAG_HASH 4
#define FLAG_KEY 8

/* Variables */
struct _uservar{
	union {
		uint32_t uni_val;
		binode_t *big_val;
		double float_val;
		datam_darr *list_val;
		file_wrapper *file_val;
		code_block *code_val;
		datam_hashtable *map_val;
	} values;
	var_type type;
	uint32_t flags;
};

/* ============================================================== Global extern objects for runtime =================================================== */

/* The deque of all allocated objects */
extern datam_deque *heap;

/* Stack of currently-accessible objects */
extern datam_darr *stack;

/* Stack of stack-frames to return to for function calls */
extern datam_darr *shadow_stack;

/* Wstring -> variable map for global variables */
extern datam_hashtable *global_vars;

/* Wstring -> variable map for local variables */
extern datam_hashtable *local_vars;

/* Stack of local variable frames */
extern datam_deque *var_frames;

/* Quota until garbage collection kicks off */
extern size_t gc_target;

#define CLONE_MODE 1
#define VOLATILE_MODE 2
/* Flags for execution mode at the moment */
extern uint32_t mode_flags;

/* Default I/O files */
extern FILE *def_out, *def_in;
extern int type_out, type_in;

/* Operation holders */
extern binode_t *op1, *op2, *op3, *op4;

/* Encoding */
extern int srcfile_type;

/* Conversion buffers */
extern char strbuf[];
extern uint32_t wstrbuf[];

/* Max digits print */
extern int max_digits;

/* =============================================================== Runtime setup/cleanup functions ===================================================== */

/* Initialize the putter runtime */
void putter_init();

/* End and clean up the putter runtime */
void putter_cleanup();

/* Run the garbage collection */
void garbage_collect();

/* Print an error and leave */
void quit_for_error(int code, const char *errmsg, ...);

/* ============================================================= Garbage collection functions =========================================================== */

/* Reset the mark on all heap objects */
void sweep_heap();

/* Mark a variable as needed */
void mark_variable(uservar *t);
void mark_list(datam_darr *list);
void mark_table(datam_hashtable *table);
void mark_block(code_block *code);

/* Collect/free a variable */
void clean_variable(uservar *t);
void clean_big(uservar *t);
void clean_file(file_wrapper *t);
void clean_list(datam_darr *list);
void clean_wstr(datam_darr *list);
void clean_table(datam_hashtable *table);
void clean_block(code_block *code);

/* =========================================================== Object functions ======================================================================== */

uservar* new_variable();
uservar* new_bignum(binode_t *num, var_type type);
uservar* new_seq(datam_darr *seq, var_type type);
uservar* new_cp(int32_t cp);
uservar* new_float(double flt);

/* Clone name if needed */
pushable clean_ref(pushable base);

/* Cloning functions */
uservar* clone_variable(uservar *base, uservar *t, int deep);

/* Pushable from var */
pushable pushable_var(uservar *var);

/* Wstring funcs */
int wstr_cmp(void *a, void *b);
int32_t wstr_ind(void *data, size_t n);

/* Obj funcs */
var_type common_math_type(var_type left, var_type right);
int32_t uservar_cmp(void *left, void *right);
int32_t uservar_hsh(void *vvar, size_t n);

/* ============================================================== Parsing functions ==================================================================== */

/* Convert syntax tree to code block */
code_block* process_parsetree(parsam_ast *tree);

/* Read a code block */
code_block* parse_block(FILE *src);

/* Read a symbol */
pushable parse_sym(FILE *src);

/* ============================================================== Execution functions ================================================================== */

/* Run a code block */
void execute_block(code_block *code);

/* Pushes a pushable */
void push_sym(pushable push);

/* Perform a builtin */
void execute_builtin(uint32_t op);

/* Pop respecting current mode */
pushable pop_pushable();

/* Get the variable value of a pushable */
uservar* resolve_var(pushable tok);

/* Discard if this is a name */
void discard_popped(pushable pop);

/* Pop a variable respecting mode and discard memory */
uservar* pop_var();

/* Get a list of variables in right-left order */
void pop_n_vars(int allow_empty, size_t n, ...);

#endif