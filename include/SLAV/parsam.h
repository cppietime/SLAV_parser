/*
This header files contains the typedefs and function prototypes for use with shift-reduce parsing.
Written by Yaakov Schectman 2019.
*/

#ifndef _H_PARSAM
#define _H_PARSAM

#include <stdio.h>
#include <stdint.h>
/* Included for hash tables */
#include "datam.h"

/* An int representing the encoding type of an input */

/* ================================================================================================================ */
/* The data types for the shift-reduce parser table itself. */

/* An enum representing the action for an entry in a shift-reduce table */
typedef enum {
    Error, /* No entry present. Encountered an error */
    Goto, /* Encountered a non-terminal that has been reduced. Goto a state */
    Shift, /* Encountered a terminal to be shifted and go to a new state */
    Reduce, /* Encountered a terminal signalling for a reduction */
    Accept, /* Finish parsing */
} parsam_action;

/* One entry in a shift-reduce table */
typedef struct {
    parsam_action action; /* Which action to do when encountering this */
    int id; /* Which state or production rule to use. Ignored for errors (maybe?) */
} parsam_cell;

/* One row in a shift-reduce table */
typedef struct {
    parsam_cell cells[0]; /* The invididual cells that comprise this row. The number of symbols is variable, so this is empty.
                            A terminals index n is at cells[n]. A non-terminal indexed n is at cells[n_terminal + n].*/
} parsam_row;

/* An enum indicating whether a symbol represents a terminal or non-terminal */
typedef enum {
    Terminal, /* A terminal symbol */
    Nonterminal /* A non-terminal */
} parsam_symtype;

/* Indicates a single symbol type */
typedef struct {
    parsam_symtype type; /* Terminal or non */
    int id; /* ID number */
} parsam_symbol;

/* A production rule */
typedef struct {
    size_t n; /* The number of subtrees this rule accepts */
    int produces; /* The id of the non-terminal this produces */
    parsam_symbol syms[0]; /* The symbols, in order, of this rule. Numbered 0 through n-1 */
} parsam_production;

/* Reads one production from a file */
parsam_production* parsam_production_read(FILE *file);

/* A shift-reduce table with production rules */
typedef struct {
    parsam_production **rules; /* Array of pointers to the production rules */
    parsam_row **states; /* Array of pointers to the rows for each state */
    datam_hashtable *symnames; /* A table assigning symbol IDs to symbol names */
    char **symids; /* An ID-indexed array of symbol names, same pointers as keys in symnames */
    size_t n_term; /* Number of terminal symbols */
    size_t n_nonterm; /* Number of non-terminal symbols */
    size_t n_rules; /* Number of production rules for this table */
    size_t n_states; /* Number of states (i.e. rows) for this table */
} parsam_table;

/* Read a shift-reduce table from a file */
parsam_table* parsam_table_read(FILE *file);

/* Read rules from a file and construct a table */
parsam_table* parsam_table_make(FILE *file);

/* Free up a constructed shift-reduce table */
void parsam_table_delete(parsam_table *table);

/* Print a table's description */
void parsam_table_print(parsam_table *table, FILE* dst);

/* ================================================================================================================ */
/* The datatypes for the Abstract Syntax Trees (i.e. ASTs) and process of parsing. */

/* Represents a single AST or AS-subtree */
typedef struct _parsam_ast parsam_ast;
struct _parsam_ast{
    parsam_ast **subtrees; /* Array of pointers to subtrees, if any. Ignored for terminals */
    uint32_t *lexeme; /* The actual characters this node in the AST contains */
    char *filename; /* The name of the file this token came from */
    parsam_symbol symbol; /* The symbol of this tree node */
    size_t n; /* Number of subtrees of this AST node */
    int rule; /* By which production rule was this symbol produced. Ignored for terminals */
    int line_no; /* Line number of token in file */
};

void strwstr(uint32_t *wstr, char *str); /* Copy normal string to unicode string */
void strnwstr(uint32_t *wstr, char *str, size_t n); /* Copy normal string to unicode string w/ length limit */
void wstrcpy(uint32_t *dst, uint32_t *src); /* Copy unicode string */
void wstrncpy(uint32_t *dst, uint32_t *src, size_t n); /* Copy unicode string */
int wstrncmp(uint32_t *dst, uint32_t *src, size_t n); /* Copy unicode string */
long wstrtol(uint32_t *wstr, uint32_t **end, int radix); /* Unicode string to long int */
void fprintw(FILE *file, uint32_t *wstr); /* Prints a unicode string as normal string to file */
void fprintwn(FILE *file, uint32_t *wstr, size_t n); /* Prints a unicode string as normal string to file with a limit */

/* Recursively frees up an AST */
void parsam_ast_delete(parsam_ast *ast);

/* Prints an AST */
void parsam_ast_print(parsam_ast *ast, parsam_table *table, FILE *dst);

/* One entry on the stack of currently parsed/parsing content */
typedef struct {
    parsam_ast *ast; /* The AST of the symbol this entry represents */
    int state; /* The state that pushed this symbol */
} parsam_stackpc;

/* A function pointer to get the next token */
typedef parsam_ast* (*parsam_generator)();

/* Parses a sequence of terminal symbols to the AST (or tries to) */
parsam_ast* parsam_parse(parsam_table *table, parsam_generator producer);

/* ================================================================================================================ */
/* Datatypes for constructing a table given rules. */

/* An entry in an item set */
typedef struct {
    parsam_production *rule; /* The rule this corresponds to */
    datam_hashtable *closure; /* A hashset of the closure items of this set */
    size_t position; /* At what position is this state (0 through rule->n) */
    int rid; /* ID of rule */
    int visited; /* A marker for calculating closure sets */
} parsam_item;

/* Makes a new item from a rule and position */
void parsam_item_init(parsam_item *dst, parsam_production *rule, int rid, size_t pos);

/* Frees an item's closure set */
void parsam_item_destroy(parsam_item *item);

/* Adds the closure set of a closure item to its closure set. Also gets the nonterminals' first sets */
void parsam_closure(parsam_item *item, parsam_table *table, uint8_t **first, parsam_item **itemspace);

/* Calculates the follow sets of nonterminal symbols */
void parsam_follow(uint8_t **follow, parsam_table *table, uint8_t **first);

/* Construct the rows for a table that currently has rules */
void parsam_construct(parsam_table *table);

#endif