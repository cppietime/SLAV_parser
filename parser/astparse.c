/*
Implementation of parsing using an existing shift-reduce parser table.
Written by Yaakov Schectman 2019.
*/

#include <stdio.h>
#include <stdlib.h>
#include "parsam.h"

void parsam_ast_delete(parsam_ast *root){
    if(root->symbol.type == Nonterminal){
        if(root->subtrees != NULL){
            for(size_t i = 0; i < root->n; i ++){
                parsam_ast_delete(root->subtrees[i]);
            }
            free(root->subtrees);
        }
    }
    if(root->lexeme != NULL){
        free(root->lexeme);
    }
    free(root);
}

parsam_ast* parsam_parse(parsam_table *table, parsam_generator producer){
    int state = 0;
    datam_darr *stack = datam_darr_new(sizeof(parsam_stackpc));
    static parsam_ast *reduced = NULL;
    static parsam_ast *next = NULL;
    int error = 0, accept = 0;
    while(!(error || accept)){
        parsam_row *row = table->states[state];
        if(reduced != NULL){
            parsam_cell cell = row->cells[reduced->symbol.id + table->n_term];
            if(cell.action == Error){
                fprintf(stderr, "No action for transition at state %d object %s!\n", state, table->symids[reduced->symbol.id + table->n_term]);
                error = 1;
            }else{
                parsam_stackpc pc = {.state = state, .ast = reduced};
                datam_darr_push(stack, &pc);
                reduced = NULL;
                state = cell.id;
            }
        }else{
            if(next == NULL){
                next = producer();
            }
            if(next == NULL){
                fprintf(stderr, "Ran out of tokens before parsing could complete!\n");
                error = 1;
            }else if(next->symbol.type == Nonterminal || next->symbol.id < table->n_term){
                int id = next->symbol.id;
                if(next->symbol.type == Nonterminal){
                    id += table->n_term;
                }
                parsam_cell cell = row->cells[id];
                switch(cell.action){
                    case Error:
                        fprintf(stderr, "No transition at state %d symbol %s", state, table->symids[id]);
                        if(next->lexeme != NULL){
                            fprintf(stderr, "(\"%s\")", next->lexeme);
                        }
                        fprintf(stderr, "!\n");
                        error = 1;
                        break;
                    case Shift:{
                        parsam_stackpc pc = {.state = state, .ast = next};
                        datam_darr_push(stack, &pc);
                        next = NULL;
                        state = cell.id;
                        break;
                    }
                    case Accept:{
                        parsam_ast_delete(next);
                        next = NULL;
                        accept = 1;
                        break;
                    }
                    case Reduce:{
                        reduced = malloc(sizeof(parsam_ast));
                        reduced->rule = cell.id;
                        parsam_production *rule = table->rules[cell.id];
                        reduced->symbol = (parsam_symbol){.id = rule->produces, .type = Nonterminal};
                        reduced->subtrees = malloc(sizeof(parsam_ast*)*rule->n);
                        reduced->n = rule->n;
                        reduced->lexeme = NULL;
                        int ist = state;
                        for(size_t i = 0; i < reduced->n; i ++){
                            parsam_stackpc pc;
                            datam_darr_pop(stack, &pc);
                            state = pc.state;
                            reduced->subtrees[reduced->n - 1 - i] = pc.ast;
                        }
						reduced->line_no = reduced->subtrees[0]->line_no;
                        break;
                    }
                    default:{
                        fprintf(stderr, "Invalid action %d for row %d at object %s\n", cell.action, state, table->symids[id]);
                        error = 1;
                    }
                }
                
            }
        }
    }
    if(accept){
        if(reduced != NULL){
            parsam_ast_delete(reduced);
        }
        if(next != NULL){
            parsam_ast_delete(next);
        }
        if(stack->n > 1){
            fprintf(stderr, "Parse tree could not be reduced to a single term!\n");
            error = 1;
        }else{
            parsam_stackpc ret;
            datam_darr_get(stack, &ret, 0);
            datam_darr_delete(stack);
            return ret.ast;
        }
    }
    if(error){
        if(reduced != NULL){
            parsam_ast_delete(reduced);
        }
        if(next != NULL){
            parsam_ast_delete(next);
        }
        parsam_stackpc pc;
        while(stack->n > 0){
            datam_darr_pop(stack, &pc);
            parsam_ast_print(pc.ast, table, stdout);
            parsam_ast_delete(pc.ast);
        }
        datam_darr_delete(stack);
        return NULL;
    }
    datam_darr_delete(stack);
    return NULL;
}

void parsam_ast_print(parsam_ast *ast, parsam_table *table, FILE* dst){
    static int tabs = 0;
    int id = ast->symbol.id;
    if(ast->symbol.type == Nonterminal){
        id += table->n_term;
    }
    char *name = table->symids[id];
    for(int i = 0; i < tabs; i ++){
        fprintf(dst, "    ");
    }
    fprintf(dst, "%s", name);
    if(ast->lexeme != NULL){
        fprintf(dst, "(%s)", ast->lexeme);
    }
    fprintf(dst, " l(%d) \t%p\n", ast->line_no, ast);
    if(ast->subtrees != NULL){
        tabs ++;
        for(size_t i = 0; i < ast->n; i ++){
            parsam_ast_print(ast->subtrees[i], table, dst);
        }
        tabs --;
    }
}