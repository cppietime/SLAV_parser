/*
A library for parsing and compiling regexes using the other capabilities of this library.
Written by Yaakov Schectman 2019.
*/

#ifndef _H_REGAM
#define _H_REGAM

#define UNICODE_MAX 0x1fffff

#include <stdint.h>
#include <stdio.h>
#include "datam.h"
#include "parsam.h"

/* ================================================================================================================== */
/* Reading regex tokens from a FILE */

/* Sets the source file */
void regam_setsrc_file(FILE *file, int close);

/* Reads regex tokens from source file */
parsam_ast* regam_producer();

/* ================================================================================================================== */
/* Types/funcs for constructing a more managable regex object from the AST */

/* A character or range of characters. Ints used in case of unicode */
typedef enum {
    None, /* Ignore, use range */
    Whitespace, /* \s, \t, \n \r */
    Alpha, /* A-Za-z */
    Digit, /* 0-9 */
    Alphanum, /* A-Za-z0-9 */
    Punc, /* ,<.>/?;:'"[{]}\|`~!@#$%^&*()-_=+ */
    Ascii, /* 0-127 */
    Dot /* All */
} regam_spec;
typedef struct {
    uint32_t start; /* First character match */
    uint32_t end; /* Last character match (inclusive) */
    regam_spec spec; /* Special character types. Ignores if None */
} regam_crange;

/* A character class */
typedef struct {
    size_t n; /* Number of cranges represented */
    char exclude; /* Include(0) or Exclude(1) */
    regam_crange ranges[0]; /* Array of ranges */
} regam_cclass;

/* Forward declaration of regex type */
typedef struct _regam_regex regam_regex;

/* Individual component and modifier */
typedef enum {
    Once, /* Default */
    Optional, /* ? - zero or one */
    Any, /* * - Zero or more */
    Many /* + - One or more */
} regam_mod;
typedef struct {
    union {
        regam_regex *regex;
        regam_cclass *cclass;
    } match; /* Subexpression or cclass */
    regam_mod mod; /* Modifier */
    char subexpr; /* Cclass(0) or regex(1) */
} regam_cpiece;

/* Regexes before |-ing together */
typedef struct {
    size_t n; /* Length */
    regam_cpiece pieces[0]; /* Array of cpieces */
} regam_opiece;

/* Regexes after |-ing together */
struct _regam_regex{
    size_t n; /* Length */
    regam_opiece *pieces[0]; /* Array of pointers to opieces */
};

/* Hardcoded token IDs in regex identifier */
typedef enum {
    Literal,
    Lpar,
    Rpar,
    Star,
    Plus,
    Qmark,
    Vbar,
    Lbrack,
    Rbrack,
    Carrot,
    Dash,
    Dsign
} regam_tokens;

/* Hardcoded nonterminal IDs in regex identifier */
typedef enum {
    Regex,
    Prebar,
    Comp,
    Elem,
    Cclass,
    Ccol,
    Ctype
} regex_objs;

/* Build regex object from AST */
regam_regex* regam_regex_build(parsam_ast *tree);

/* Prints a description of a regex */
void regam_regex_print(regam_regex *regex);

/* Recursivley delete a regex object and all its child objects */
void regam_regex_delete(regam_regex *regex);


/* ================================================================================================================== */
/* Conversion of regex to NFA. */

/* List of ranges, target, and type for a transition from one state to another */
typedef struct {
    uint32_t start; /* Start transition */
    uint32_t end; /* End transition */
    uint32_t target; /* To what state does this lead */
    char exclude; /* Inclusive(0), Exclusive(1), or Epsilon (-1). 2 for ignored transitions. */
} regam_transition;

/* Single state in a regex NFA */
typedef struct {
    datam_darr *transitions; /* List of transitions. These are not pointers */
    datam_hashtable *closure; /* Epsilon closure of this state, also for DFAs */
    uint32_t id; /* ID number of this state (0 onward) */
    uint32_t link; /* Needed in cloning */
    int accept; /* ID of this accept state (0 if non-accepting) */
    char mark; /* Marked for cleaning */
} regam_nstate;

/* New empty NFA state */
regam_nstate* regam_nstate_new();

/* Deletes a regam_nstate */
void regam_nstate_delete(regam_nstate *state);

/* NFA with start state and end states */
typedef struct {
    datam_darr *accepts; /* End states, list of state IDs */
    uint32_t start; /* Start state, ID */
} regam_nfa;

/* Free an NFA and its list */
void regam_nfa_delete(regam_nfa *nfa);

/* Initializes statics */
void regam_nfa_start();

/* Ends statics */
void regam_nfa_end();

/* Clears all marks */
void regam_nfa_unmark(char m);

/* Create a singular NFA from start -> accept for a cclass */
regam_nfa* regam_nfa_single(regam_cclass *cclass);

/* Clone an NFA for construction algorithms */
regam_nfa* regam_nfa_clone(regam_nfa *src);

/* Concatenates second onto first, modifying first and freeing second */
void regam_nfa_concat(regam_nfa *first, regam_nfa *second);

/* Unions n NFAs, modifying the first and freeing the remaining */
void regam_nfa_union(regam_nfa **list, size_t n);

/* Constructs an NFA of src corresponding to src? in place */
void regam_nfa_qmark(regam_nfa *src);

/* Constructs an NFA of src corresponding to src* in place */
void regam_nfa_star(regam_nfa *src);

/* Constructs an NFA of src corresponding to src+ in place */
void regam_nfa_plus(regam_nfa *src);

/* Constructs the NFA of a given regex */
regam_nfa* regam_nfa_build(regam_regex *regex);

/* Prints nfa states */
void regam_nfa_print(regam_nfa *nfa);

/* Tag all accept states of an NFA with a certain accept value */
void regam_nfa_tag(regam_nfa *nfa, uint32_t tag);


/* ================================================================================================================== */
/* Conversion of NFA to DFA. */

/* Reverses/splits exclusion and dot transitions as needed */
void regam_transition_monofy(datam_darr *list);

/* Type needed for separating ranges */
typedef struct {
    uint32_t pos; /* Spot signified */
    uint32_t id; /* ID of target */
    char type; /* 0 - start, 1 - end */
} regam_triplet;

/* Takes a list of transitions and makes sure it all transitions to do not overlap */
void regam_transition_unique(datam_darr *list);

/* Recursively calculates the epsilon closure of a state */
void regam_epsilon_closure(regam_nstate *state);

/* Gets the state ID of the new state for a collection of many states. May create a new state if needed */
uint32_t regam_compound_id(datam_hashtable *keyset, int del);

/* Prints the closure */
void regam_print_closure(regam_nstate *state);

/* Go from NFA to DFA */
regam_nfa* regam_to_dfa(regam_nfa *nfa);

/* Delete all irrelevant states and consolidate state no's */
void regam_dfa_reduce(regam_nfa *dfa);


/* ================================================================================================================== */
/* Match string to regex */

/* Match */
uint32_t* regam_match(uint32_t *src, size_t n, regam_nfa *dfa, uint32_t *type);


/* ================================================================================================================== */
/* Reading from file */

/* Extra params for specific tokens */
typedef struct {
    uint32_t *key; /* String to replace */
    uint32_t base; /* Token applies to */
    uint32_t target; /* Token to become */
} regam_repl;

/* Get the DFA */
regam_nfa* regam_from_file(FILE *src, parsam_table *table);

/* Get special replacements */
datam_darr* regam_repls(FILE *src, parsam_table *table);

/* Free repls list */
void regam_repls_delete(datam_darr *repls);

/* Save repls to a file */
void regam_repls_save(FILE *dst, datam_darr *repls, parsam_table *table);

/* Filter with repls */
uint32_t regam_filter(uint32_t *p0, uint32_t *p1, uint32_t type, datam_darr *repls);

/* Set the DFA and extra table for lexing */
void regam_load_lexer(regam_nfa *dfa, datam_darr *repls, parsam_table *table);

/* Load a source file */
void regam_load_lexsrc(FILE* src);

/* Get the next lexeme if possible */
parsam_ast* regam_get_lexeme();

/* Save to file*/
void regam_dfa_save(FILE *dst, regam_nfa *dfa);

/* Load from file */
regam_nfa* regam_dfa_load(FILE *src);

#endif