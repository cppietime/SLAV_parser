/*
Item set/state handling implementation.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include "parsam.h"

const size_t hashset_indices = 4;

int32_t parsam_item_indexer(void *data, size_t n){
    parsam_item *item = (parsam_item*)data;
    switch(n){
        case 0:
            return item->rule->n;
        case 1:
            return item->rule->produces;
        case 2:
            return item->position;
        case 3:
            return (int32_t)item->rule;
        default:
            return 0;
    }
}

int parsam_item_cmp(void *a, void *b){
    if(a == b){
        return 0;
    }
    parsam_item *itema = (parsam_item*)a, *itemb = (parsam_item*)b;
    if(itema->rule != itemb->rule){
        return 1;
    }
    return itema->position - itemb->position;
}

int32_t itemset_indexer(void *data, size_t n){
    datam_hashtable *table = (datam_hashtable*)data;
    int32_t ret = 0;
    for(datam_hashbucket *bucket = datam_hashtable_next(table, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(table, bucket)){
            ret += parsam_item_indexer(bucket->key, n);
    }
    return ret;
}

int itemset_cmp(void *a, void *b){
    datam_hashtable *ta = (datam_hashtable*)a, *tb = (datam_hashtable*)b;
    for(datam_hashbucket* bucket = datam_hashtable_next(ta, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(ta, bucket)){
            if(datam_hashtable_has(tb, bucket->key) == 0){
                return 1;
            }
    }
    for(datam_hashbucket* bucket = datam_hashtable_next(tb, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(tb, bucket)){
            if(datam_hashtable_has(ta, bucket->key) == 0){
                return -1;
            }
    }
    return 0;
}

int32_t int_indexer(void *data, size_t n){
    return (int)data;
}

int int_cmp(void *a, void *b){
    int ia = (int)a, ib = (int)b;
    return ia - ib;
}

void parsam_item_init(parsam_item *ret, parsam_production *rule, int rid, size_t position){
    ret->visited = 0;
    ret->rule = rule;
    ret->rid = rid;
    ret->position = position;
    ret->closure = datam_hashtable_new(parsam_item_indexer, parsam_item_cmp, hashset_indices);
}

void parsam_item_destroy(parsam_item *item){
    datam_hashtable_delete(item->closure);
}

void parsam_item_print(parsam_item *item, parsam_table *table){
    parsam_production *rule = item->rule;
    printf("(%s -> ", table->symids[rule->produces + table->n_term]);
    for(size_t i = 0; i < rule->n; i ++){
        if(i == item->position){
            printf(". ");
        }
        parsam_symbol sym = rule->syms[i];
        int id = sym.id;
        if(sym.type == Nonterminal){
            id += table->n_term;
        }
        printf("%s ", table->symids[id]);
    }
    if(item->position == rule->n){
        printf(".");
    }
    printf(")");
}

void parsam_closure(parsam_item *item, parsam_table *table, uint8_t **first, parsam_item **itemspace){
    if(!item->visited){ /* If we already calculated this closure, we may skip */
        item->visited = 1;
        datam_hashtable_put(item->closure, item, NULL); /* I is in the closure of I */
        uint8_t *first_lhs = first[item->rule->produces];
        if(item->position < item->rule->n){ /* Final states of a production have no further closures */
            parsam_symbol nsym = item->rule->syms[item->position]; /* Next symbol, look for closure of this */
            if(nsym.type == Terminal){ /* For terminals, just add to the first set */
                int nid = nsym.id;
                first_lhs[(nid / 8)] |= 1 << (nid % 8);
            }else{ /* For nonterminals, recursively calculate closures */
                for(size_t i = 0; i < table->n_rules; i ++){ /* Iterate through each rule */
                    parsam_production *rule = table->rules[i];
                    int id = rule->produces;
                    if(id == nsym.id){ /* It only applies to productions of the following item */
                        parsam_item *candidate = itemspace[i] + 0; /* New item is the initial state of the production */
                        if(datam_hashtable_has(item->closure, candidate) == 0 || 1){ /* If we already have it, we skip */
                            parsam_closure(candidate, table, first, itemspace); /* Otherwise, calculate its closure */
                            int ct = 0;
                            for(datam_hashbucket *bucket = datam_hashtable_next(candidate->closure, NULL); /* And add everything in its closure to item's closure */
                                bucket != NULL;
                                bucket = datam_hashtable_next(candidate->closure, bucket)){
                                    parsam_item *child = (parsam_item*)bucket->key;
                                    datam_hashtable_put(item->closure, child, NULL);
                                    parsam_symbol csym = child->rule->syms[0];
                                    if(csym.type == Terminal){ /* If an element in the closure set begins with a terminal, add it to the first set */
                                        int cid = csym.id;
                                        first_lhs[cid / 8] |= 1 << (cid % 8);
                                    }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* Private helper function to recursively add follow(A) to follow(B) whenever A -> wB is a rule */
void parsam_follow_help(uint8_t **follow, parsam_table *table, uint8_t *visited, int id){
    if(visited[id / 8] & (1 << (id % 8))){
        return;
    }
    visited[id / 8] |= (1 << (id * 8));
    uint8_t *follow_rhs = follow[id];
    for(size_t i = 0; i < table->n_rules; i ++){
        parsam_production *rule = table->rules[i];
        int lhs = rule->produces;
        parsam_symbol final = rule->syms[rule->n-1];
        if(final.type == Terminal){
            continue;
        }
        int rhs = final.id;
        if(rhs != id){
            continue;
        }
        parsam_follow_help(follow, table, visited, lhs);
        uint8_t *follow_lhs = follow[lhs];
        for(size_t j = 0; j < (table->n_term + 7) / 8; j ++){
            follow_rhs[j] |= follow_lhs[j];
        }
    }
}

void parsam_follow(uint8_t **follow, parsam_table *table, uint8_t **first){
    /* Go through each rule, add the non-recursive members */
    for(size_t i = 0; i < table->n_rules; i ++){
        parsam_production *rule = table->rules[i];
        for(size_t j = 0; j < rule->n - 1; j ++){ /* Get each symbol except the last one */
            parsam_symbol lsym = rule->syms[j];
            if(lsym.type == Nonterminal){ /* Skip terminals, as there is no need for follow sets */
                uint8_t *follow_lhs = follow[lsym.id];
                parsam_symbol rsym = rule->syms[j+1];
                if(rsym.type == Terminal){ /* If the next symbol is terminal, just add it*/
                    int rhs = rsym.id;
                    follow_lhs[rhs / 8] |= (1 << (rhs % 8));
                }else{ /* Otherwise, add every terminal in the next symbol's first set */
                    uint8_t *first_rhs = first[rsym.id];
                    for(size_t k = 0; k < (table->n_term + 7) / 8; k ++){
                        follow_lhs[k] |= first_rhs[k];
                    }
                }
            }
        }
    }
    /* Now do the recursive step */
    uint8_t *visited = calloc((table->n_nonterm + 7) / 8, 1);
    for(int i = 0; i < table->n_nonterm; i ++){
        parsam_follow_help(follow, table, visited, i);
    }
    free(visited);
}

/*
When entering this function for the first time, states should be an empty list, itemsets an empty hashtable of hashsets of items, table a table with rules, but not states, itemspace
an array an item array per rule, all which should have their closures calculated, follow an  array of bitsets representing which terminals are in which nonterminal's follow sets,
and itemset is the hashset of items being processed.
*/
void parsam_makestates(datam_darr *states, datam_hashtable *itemsets, parsam_table *table, parsam_item **itemspace, uint8_t **follow, datam_hashtable *itemset){
    datam_hashtable *closure = datam_hashtable_new(parsam_item_indexer, parsam_item_cmp, hashset_indices);
    for(datam_hashbucket *bucket = datam_hashtable_next(itemset, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(itemset, bucket)){
            parsam_item *item = (parsam_item*)bucket->key;
            datam_hashtable_addall(closure, item->closure);
    }
    parsam_row *state = malloc(sizeof(parsam_cell) * (table->n_term + table->n_nonterm));
    for(int i = 0; i < table->n_term + table->n_nonterm; i ++){
        state->cells[i].action = Error;
        state->cells[i].id = 0;
    }
    int present = datam_hashtable_has(itemsets, itemset) != NULL;
    datam_hashtable_put(itemsets, itemset, (void*)states->n);
    if(present){
        datam_hashtable_delete(itemset);
    }
    datam_darr_push(states, &state);
    /* Check all temrinal transitions */
    for(int i = 0; i < table->n_term; i ++){
        datam_hashtable *shift_set = datam_hashtable_new(parsam_item_indexer, parsam_item_cmp, hashset_indices);
        for(datam_hashbucket *bucket = datam_hashtable_next(closure, NULL);
            bucket != NULL;
            bucket = datam_hashtable_next(closure, bucket)){
                parsam_item *item = (parsam_item*)bucket->key;
                parsam_production *rule = item->rule;
                if(item->position == rule->n){
                    if(rule->produces == table->n_nonterm - 1){
                        state->cells[i].action = Accept;
                    }else if(follow[rule->produces][i / 8] & (1 << (i % 8))){
                        state->cells[i].action = Reduce;
                        state->cells[i].id = item->rid;
                    }
                }else if(item->position == rule->n - 1 && rule->produces == table->n_nonterm - 1){
                    state->cells[i].action = Accept;
                }else{
                    parsam_symbol sym = rule->syms[item->position];
                    if(sym.type == Terminal && sym.id == i){
                        parsam_item *step = itemspace[item->rid]+item->position+1;
                        datam_hashtable_put(shift_set, step, NULL);
                    }
                }
        }
        if(shift_set->n > 0){
            if(datam_hashtable_has(itemsets, shift_set)){
                int sid = (int)datam_hashtable_get(itemsets, shift_set);
                datam_hashtable_delete(shift_set);
                state->cells[i].action = Shift;
                state->cells[i].id = sid;
            }else{
                state->cells[i].action = Shift;
                state->cells[i].id = states->n;
                parsam_makestates(states, itemsets, table, itemspace, follow, shift_set);
            }
        }else{
            datam_hashtable_delete(shift_set);
        }
    }
    /* Check all nonterminal symbols */
    for(int i = 0; i < table->n_nonterm; i ++){
        datam_hashtable *goto_set = datam_hashtable_new(parsam_item_indexer, parsam_item_cmp, hashset_indices);
        for(datam_hashbucket *bucket = datam_hashtable_next(closure, NULL);
            bucket != NULL;
            bucket = datam_hashtable_next(closure, bucket)){
                parsam_item *item = (parsam_item*)bucket->key;
                parsam_production *rule = item->rule;
                if(item->position < rule->n){
                    parsam_symbol sym = rule->syms[item->position];
                    if(sym.type == Nonterminal && sym.id == i){
                        parsam_item *step = itemspace[item->rid]+item->position+1;
                        datam_hashtable_put(goto_set, step, NULL);
                    }
                }
        }
        if(goto_set->n > 0){
            if(datam_hashtable_has(itemsets, goto_set)){
                int sid = (int)datam_hashtable_get(itemsets, goto_set);
                datam_hashtable_delete(goto_set);
                state->cells[i + table->n_term].action = Goto;
                state->cells[i + table->n_term].id = sid;
            }else{
                state->cells[i + table->n_term].action = Goto;
                state->cells[i + table->n_term].id = states->n;
                parsam_makestates(states, itemsets, table, itemspace, follow, goto_set);
            }
        }else{
            datam_hashtable_delete(goto_set);
        }
    }
    datam_hashtable_delete(closure);
}

void parsam_construct(parsam_table *table){
    uint8_t **follow = malloc(sizeof(uint8_t*)*table->n_nonterm);
    uint8_t **first = malloc(sizeof(uint8_t*)*table->n_nonterm);
    for(size_t i = 0; i < table->n_nonterm; i ++){
        follow[i] = calloc((table->n_term + 7) / 8, 1);
        first[i] = calloc((table->n_term + 7) / 8, 1);
    }
    parsam_item **itemspace = malloc(sizeof(parsam_item*)*table->n_rules);
    for(size_t i = 0; i < table->n_rules; i ++){
        parsam_production *rule = table->rules[i];
        itemspace[i] = malloc(sizeof(parsam_item) * (rule->n+1));
        for(size_t j = 0; j < rule->n + 1; j ++){
            parsam_item_init(itemspace[i] + j, rule, i, j);
        }
    }
    for(size_t i = 0; i < table->n_rules; i ++){
        parsam_production *rule = table->rules[i];
        for(size_t j = 0; j < rule->n + 1; j ++){
            parsam_item *item = itemspace[i] + j;
            parsam_closure(item, table, first, itemspace);
        }
    }
    parsam_follow(follow, table, first);
    datam_darr *states = datam_darr_new(sizeof(parsam_row*));
    datam_hashtable *itemsets = datam_hashtable_new(itemset_indexer, itemset_cmp, hashset_indices);
    datam_hashtable *initial = datam_hashtable_new(parsam_item_indexer, parsam_item_cmp, hashset_indices);
    datam_hashtable_put(initial, itemspace[0]+0, NULL);
    parsam_makestates(states, itemsets, table, itemspace, follow, initial);
    /* Free the itemsets we made along the way */
    for(datam_hashbucket *bucket = datam_hashtable_next(itemsets, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(itemsets, bucket)){
            datam_hashtable *oneset = (datam_hashtable*)bucket->key;
            datam_hashtable_delete(oneset);
    }
    datam_hashtable_delete(itemsets);
    for(size_t i = 0; i < table->n_nonterm; i ++){
        free(follow[i]);
        free(first[i]);
    }
    for(size_t i = 0; i < table->n_rules; i ++){
        for(size_t j = 0; j < table->rules[i]->n + 1; j ++){
            parsam_item_destroy(itemspace[i] + j);
        }
        free(itemspace[i]);
    }
    free(itemspace);
    free(follow);
    free(first);
    table->n_states = states->n;
    table->states = malloc(sizeof(parsam_row*) * states->n);
    for(size_t i = 0; i < states->n; i ++){
        datam_darr_get(states, table->states+i, i);
    }
    datam_darr_delete(states);
}