/*
Code to convert NFA to DFA.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "regam.h"

/* From NFA */
extern datam_darr *statelist;

/* Keeping track of states by keysets */
extern datam_hashtable *keysets;

/* Comparator for transitions */
int trans_cmp(const void *va, const void *vb){
    regam_transition *a = (regam_transition*)va, *b = (regam_transition*)vb;
    if(a->exclude == -1){
        if(b->exclude == -1){
            return 0;
        }
        return -1;
    }
    if(b->exclude == -1){
        return 1;
    }
    if(a->exclude == 2){
        if(b->exclude == 2){
            return 0;
        }
        return 1;
    }
    if(b->exclude == 2){
        return -1;
    }
    int dif = (int)a->start - (int)b->start;
    if(dif != 0){
        return dif;
    }
    dif = (int)a->end - (int)b->end;
    if(dif != 0){
        return dif;
    }
    return 0;
}

void regam_transition_monofy(datam_darr *list){
    size_t i_size = list->n;
    regam_transition t;
    for(size_t i = 0; i < i_size; i ++){
        datam_darr_get(list, &t, i);
        if(t.exclude == 1){
            int before = t.start != 0, after = t.end != UNICODE_MAX;
            if(before){
                regam_transition first = {0, t.start-1, t.target, 0};
                datam_darr_set(list, &first, i);
            }
            if(after){
                regam_transition last = {t.end+1, UNICODE_MAX, t.target, 0};
                if(before){
                    datam_darr_push(list, &last);
                }else{
                    datam_darr_set(list, &last, i);
                }
            }
            if(!(before || after)){
                t.exclude = 2;
                datam_darr_set(list, &t, i);
            }
        }
    }
}

int trip_cmp(const void *va, const void *vb){
    regam_triplet *a = (regam_triplet*)va, *b = (regam_triplet*)vb;
    int dif = (int)a->pos - (int)b->pos;
    if(dif != 0){
        return dif;
    }
    dif = a->type - b->type;
    if(dif != 0){
        return dif;
    }
    return 0;
}

void regam_transition_unique(datam_darr *list){
    datam_darr_sort(list, trans_cmp); /* Sort the list of transitions */
    size_t start;
    regam_transition t;
    while(list->n > 0){ /* Remove ignorable transitions */
        datam_darr_get(list, &t, list->n - 1);
        if(t.exclude != 2){
            break;
        }
        datam_darr_pop(list, &t);
    }
    for(start = 0; start < list->n; start ++){ /* Get the starting position of non-epsilons */
        datam_darr_get(list, &t, start);
        if(t.exclude != -1 && t.exclude != 2){
            break;
        }
    }
    if(start == list->n){ /* No work to be done if there are none */
        return;
    }
    size_t num = list->n - start;
    regam_triplet *trips = malloc(sizeof(regam_triplet) * 2 * num);
    for(size_t i = start; i < list->n; i ++){ /* Split the list of transitions into triplets */
        datam_darr_get(list, &t, i);
        trips[(i - start) * 2] = (regam_triplet){t.start, t.target, 0};
        trips[(i - start) * 2 + 1] = (regam_triplet){t.end, t.target, 1};
    }
    list->n = start;
    qsort(trips, 2 * num, sizeof(regam_triplet), trip_cmp);
    datam_hashtable *current = datam_hashtable_new(datam_hashtable_i_indexer, datam_hashtable_i_cmp, 1);
    size_t n = trips[0].pos, m;
    datam_hashtable_put(current, (void*)trips[0].id, NULL);
    for(size_t i = 1; i < num * 2; i ++){
        regam_triplet trip = trips[i];
        /* Get end of range */
        if(trip.type){ /* It is an end of a range */
            m = trip.pos;
        }else{ /* Starts */
            m = trip.pos - 1;
        }
        /* Add the ranges if not empty*/
        if(m >= n){
            for(datam_hashbucket *bucket = datam_hashtable_next(current, NULL);
                bucket != NULL;
                bucket = datam_hashtable_next(current, bucket)){
                    t = (regam_transition){n, m, (uint32_t)bucket->key, 0};
                    datam_darr_push(list, &t);
            }
        }
        /* Update and remove */
        if(trip.type){ /* For an end */
            datam_hashtable_remove(current, (void*)trip.id);
        }else{ /* For a start  */
            datam_hashtable_put(current, (void*)trip.id, NULL);
        }
        n = m + 1;
    }
    datam_hashtable_delete(current);
    free(trips);
}

void regam_epsilon_closure(regam_nstate *state){
    if(state->closure != NULL){ /* Closure already calculated. Leave */
        return;
    }
    state->closure = datam_hashtable_new(datam_hashtable_i_indexer, datam_hashtable_i_cmp, 1);
    datam_hashtable_put(state->closure, (void*)(state->id), NULL);
    regam_transition_monofy(state->transitions);
    regam_transition_unique(state->transitions);
    regam_transition t;
    regam_nstate *close;
    for(size_t i = 0; i < state->transitions->n; i ++){
        datam_darr_get(state->transitions, &t, i);
        if(t.exclude != -1){
            continue;
        }
        datam_darr_get(statelist, &close, t.target);
        regam_epsilon_closure(close);
        datam_hashtable_addall(state->closure, close->closure);
    }
}

/* Keyset should only contain sets that represent full closures. Newly created sets shouldn't have any epsilon transitions so we can set their
closure sets in this function entirely */
uint32_t regam_compound_id(datam_hashtable *keyset, int del){
    datam_hashbucket *bucket;
    if(keyset->n == 1){ /* For a singular set, just return the state */
        bucket = datam_hashtable_next(keyset, NULL);
        uint32_t ret = (uint32_t)bucket->key;
        if(del){
            datam_hashtable_delete(keyset);
        }
        return ret;
    }
    bucket = datam_hashtable_has(keysets, keyset);
    if(bucket == NULL){
        regam_nstate *newstate = regam_nstate_new();
        newstate->closure = datam_hashtable_new(datam_hashtable_i_indexer, datam_hashtable_i_cmp, 1);
        for(datam_hashbucket *sub = datam_hashtable_next(keyset, NULL); /* For each NFA state in the closure */
            sub != NULL;
            sub = datam_hashtable_next(keyset, sub)){
                datam_hashtable_put(newstate->closure, sub->key,  NULL); /* Add it to the closure, and... */
                uint32_t cid = (int)sub->key;
                regam_nstate *cstate;
                datam_darr_get(statelist, &cstate, cid); /* Grab that state... */
                if(cstate->accept > newstate->accept){ /* Make it accept if the closure accepts */
                    newstate->accept = cstate->accept;
                }
                for(size_t i = 0; i < cstate->transitions->n; i ++){
                    regam_transition t;
                    datam_darr_get(cstate->transitions, &t, i);
                    if(t.exclude != 0){
                        continue;
                    }
                    datam_darr_push(newstate->transitions, &t); /* Add all non-epsilon transitions to the new state */
                }
        }
        regam_transition_monofy(newstate->transitions);
        regam_transition_unique(newstate->transitions);
        datam_hashtable_put(keysets, keyset, newstate);
        return newstate->id;
    }else if(del){
        datam_hashtable_delete(keyset);
    }
    regam_nstate *closure = (regam_nstate*)bucket->value;
    return closure->id;
}

void regam_dfaize(regam_nfa *dst, regam_nfa *src, regam_nstate *state){
    regam_epsilon_closure(state); /* Get the closure and, if necessary, the new state it makes */
    uint32_t cid = regam_compound_id(state->closure, 0);
    regam_nstate *cstate = state;
    if(cid != state->id){ /* If it's a new state, we need to use that state */
        datam_darr_get(statelist, &cstate, cid);
    }
    if(state->id == src->start){ /* If this state was the src's start state, put it as the dst's start state */
        dst->start = cid;
    }
    uint32_t aid, prob;
    for(size_t i = 0; i < src->accepts->n; i ++){ /* If the closure contains any accepting states, add it to the accepts list */
        datam_darr_get(src->accepts, &aid, i);
        if(datam_hashtable_has(state->closure, (void*)aid) != NULL){
            int pres = 0;
            for(size_t j = 0; j < dst->accepts->n; j ++){
                datam_darr_get(dst->accepts, &prob, j);
                if(prob == cid){
                    pres = 1;
                    break;
                }
            }
            if(!pres){
                datam_darr_push(dst->accepts, &cid);
            }
        }
    }
    /* Now I need to somehow make all the transitions from the state */
    datam_hashtable *targetset; /* Empty set to see where we going */
    datam_darr *new_trans = datam_darr_new(sizeof(regam_transition)); /* A new list of unified transitions */
    regam_transition t0, t1;
    for(size_t i = 0; i < cstate->transitions->n; ){
        datam_darr_get(cstate->transitions, &t0, i);
        if(t0.exclude == -1){ /* Skip the epsilons. There shouldn't actually be any here anyway so I'll print an error */
            fprintf(stderr, "Error: Epsilon transition out of closed state!\n");
            continue;
        }
        targetset = datam_hashtable_new(datam_hashtable_i_indexer, datam_hashtable_i_cmp, 1);
        regam_nstate *tstate;
        datam_darr_get(statelist, &tstate, t0.target); /* Get that next state */
        regam_epsilon_closure(tstate); /* Get the epsilon closure */
        datam_hashtable_addall(targetset, tstate->closure); /* Put the first state's closure into the transition */
        size_t j;
        for(j = i+1; j < cstate->transitions->n; j ++){
            datam_darr_get(cstate->transitions, &t1, j);
            if(t1.start != t0.start){ /* If this is a new transition, we hault here */
                break;
            }
            datam_darr_get(statelist, &tstate, t1.target); /* Otherwise we will get the closure and add it to this transition */
            regam_epsilon_closure(tstate);
            datam_hashtable_addall(targetset, tstate->closure);
        }
        for(datam_hashbucket *bucket = datam_hashtable_next(targetset, NULL);
            bucket != NULL;
            bucket = datam_hashtable_next(targetset, bucket)){
                uint32_t id = (uint32_t)bucket->key;
        }
        i = j; /* Where we start next */
        int process = datam_hashtable_has(keysets, targetset) == NULL; /* Will need this for later */
        uint32_t target_id = regam_compound_id(targetset, 1); /* Get the ID of the compound set, create it if it does not exist */
        if(target_id == cid && cid != state->id){
            process = 1;
        }
        t0.target = target_id;
        datam_darr_push(new_trans, &t0); /* Add the pointed to state to the transitions list */
        if(process){ /* If the state is newly created, we process it */
            datam_darr_get(statelist, &tstate, target_id);
            regam_dfaize(dst, src, tstate);
        }
    }
    datam_darr_delete(cstate->transitions);
    cstate->transitions = new_trans; /* Replace transitions with the new unified one */
}

regam_nfa *regam_to_dfa(regam_nfa *nfa){
    regam_nstate *start;
    datam_darr_get(statelist, &start, nfa->start);
    regam_nfa *dfa = malloc(sizeof(regam_nfa));
    dfa->accepts = datam_darr_new(sizeof(uint32_t));
    regam_dfaize(dfa, nfa, start);
    return dfa;
}

void regam_print_closure(regam_nstate *state){
    printf("Closure of %d(%d): ", state->id, state->closure->n);
    for(datam_hashbucket* bucket = datam_hashtable_next(state->closure, NULL);
        bucket != NULL;
        bucket = datam_hashtable_next(state->closure, bucket)){
            uint32_t cid = (uint32_t)bucket->key;
            printf("%d, ", cid);
    }
    printf("\n");
}

static size_t ctr = 0;
void regam_dfa_marker(regam_nstate *start){
    if(start->mark != 0){
        return;
    }
    start->mark = ctr++;
    regam_transition t;
    for(size_t i = 0; i < start->transitions->n; i ++){
        datam_darr_get(start->transitions, &t, i);
        regam_nstate *next;
        datam_darr_get(statelist, &next, t.target);
        regam_dfa_marker(next);
    }
}

void regam_dfa_markall(regam_nfa *dfa){
    regam_nfa_unmark(0);
    regam_nstate *start;
    datam_darr_get(statelist, &start, dfa->start);
    ctr = 1;
    regam_dfa_marker(start);
}

void regam_dfa_reduce(regam_nfa *dfa){
    regam_dfa_markall(dfa);
    regam_nstate *state, *tstate;
    regam_nstate **newlist = malloc(sizeof(regam_nstate*) * ctr);
    newlist[0] = NULL;
    regam_transition t;
    datam_darr_get(statelist, &state, dfa->start);
    dfa->start = state->mark;
    uint32_t tid;
    for(size_t i = 0; i < dfa->accepts->n; i ++){
        datam_darr_get(dfa->accepts, &tid, i);
        datam_darr_get(statelist, &state, tid);
        tid = state->mark;
        datam_darr_set(dfa->accepts, &tid, i);
    }
    for(size_t i = 0; i < statelist->n; i ++){
        datam_darr_get(statelist, &state, i);
        if(state == NULL){
            continue;
        }
        if(state->mark == 0){
            regam_nstate_delete(state);
			state = NULL;
			datam_darr_set(statelist, &state, i);
            continue;
        }
        state->id = state->mark;
        for(size_t j = 0; j < state->transitions->n; j ++){
            datam_darr_get(state->transitions, &t, j);
            tid = t.target;
            datam_darr_get(statelist, &tstate, tid);
            t.target = tstate->mark;
            datam_darr_set(state->transitions, &t, j);
        }
        newlist[state->mark] = state;
    }
    statelist->n = 1;
    for(size_t i = 1; i < ctr; i ++){
        state = newlist[i];
        datam_darr_push(statelist, &state);
    }
    free(newlist);
}

void regam_dfa_save(FILE *dst, regam_nfa *dfa){
	regam_dfa_reduce(dfa);
	regam_nstate *state;
	regam_transition t;
	for(size_t i = 1; i < statelist->n; i ++){
		datam_darr_get(statelist, &state, i);
		if(state == NULL){
			break;
		}
		fprintf(dst, "%d %d", state->accept, i == dfa->start);
		for(size_t j = 0; j < state->transitions->n; j ++){
			// if(j + 1 < state->transitions->n){
				fputc(' ', dst);
			// }
			datam_darr_get(state->transitions, &t, j);
			if(t.exclude != 0){
				continue;
			}
			fprintf(dst, "%d %d %d", t.start, t.end, t.target);
		}
		fputc('\n', dst);
	}
	fprintf(dst, "ENDDFA\n");
}

regam_nfa* regam_dfa_load(FILE* src){
	regam_nfa *ret = malloc(sizeof(regam_nfa));
	ret->accepts = datam_darr_new(sizeof(uint32_t));
	regam_nstate *state;
	regam_transition t;
	static char line[1024];
	char *eptr;
	uint32_t ctr = 1;
	while(1){
		fgets(line, 1024, src);
		if(!strncmp(line, "ENDDFA", 6)){
			break;
		}
		if(ctr >= statelist->n){
			state = regam_nstate_new();
		}else{
			datam_darr_get(statelist, &state, ctr);
			if(state == NULL){
				state = malloc(sizeof(regam_nstate));
				*state = (regam_nstate){.transitions = datam_darr_new(sizeof(regam_transition)), .accept = 0, .id = ctr, .closure = NULL};
			}else{
				state->id = ctr;
				state->transitions->n = 0;
			}
		}
		state->accept = strtol(line, &eptr, 10);
		if(state->accept != 0){
			datam_darr_push(ret->accepts, &ctr);
		}
		int start = strtol(eptr, &eptr, 10);
		if(start){
			ret->start = ctr;
		}
		while(1){
			if(*eptr == '0' || *eptr == '\n' || *eptr == '\r'){
				break;
			}
			t.start = strtol(eptr, &eptr, 10);
			t.end = strtol(eptr, &eptr, 10);
			t.target = strtol(eptr, &eptr, 10);
			t.exclude = 0;
			datam_darr_push(state->transitions, &t);
		}
		ctr ++;
	}
	return ret;
}