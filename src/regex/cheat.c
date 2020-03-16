/* Execution of NFAs */

#include <stdint.h>
#include "slav.h"
#include "datam.h"
#include "slavio.h"

extern datam_darr *statelist;

/* Another method to get the closure for a state that doesn't yet have it */
void cheat_calc_closure(regam_nstate *state){
	if(state->closure != NULL)
		return;
	state->closure = datam_hashtable_new(datam_hashtable_i_indexer, datam_hashtable_i_cmp, 1);
	datam_hashtable_put(state->closure, (void*)(state->id), NULL);
	regam_transition trans;
	for(size_t i = 0; i < state->transitions->n; i++){
		datam_darr_get(state->transitions, &trans, i);
		if(trans.exclude != -1)
			continue;
		uint32_t next = trans.target;
		regam_nstate *nextstate;
		datam_darr_get(statelist, &nextstate, next);
		cheat_calc_closure(nextstate);
		datam_hashtable_addall(state->closure, nextstate->closure);
	}
}

/* Add the closures of all in one set to another */
void cheat_calc_closures(datam_hashtable *dst, datam_hashtable *src){
	for(datam_hashbucket *bucket = datam_hashtable_next(src, NULL); bucket != NULL; bucket = datam_hashtable_next(src, bucket)){
		uint32_t id = (uint32_t)(bucket->key);
		regam_nstate *state;
		datam_darr_get(statelist, &state, id);
		cheat_calc_closure(state);
		datam_hashtable_addall(dst, state->closure);
	}
}

/* Add the transitions from a symbol for all in one set to another */
void cheat_calc_transitions(datam_hashtable *dst, datam_hashtable *src, uint32_t sym){
	for(datam_hashbucket *bucket = datam_hashtable_next(src, NULL); bucket != NULL; bucket = datam_hashtable_next(src, bucket)){
		uint32_t id = (uint32_t)(bucket->key);
		regam_nstate *state;
		datam_darr_get(statelist, &state, id);
		regam_transition trans;
		for(size_t i = 0; i < state->transitions->n; i++){
			datam_darr_get(state->transitions, &trans, i);
			if(trans.exclude == 0 && sym >= trans.start && sym <= trans.end){
				regam_nstate *target;
				datam_darr_get(statelist, &target, trans.target);
				cheat_calc_closure(target);
				datam_hashtable_addall(dst, target->closure);
			}
		}
	}
}

/* Get whether this state accepts anything */
uint32_t cheat_calc_accept(datam_hashtable *src){
	uint32_t ret = 0;
	regam_nstate *state;
	for(datam_hashbucket *bucket = datam_hashtable_next(src, NULL); bucket != NULL; bucket = datam_hashtable_next(src, bucket)){
		uint32_t sid = (uint32_t)(bucket->key);
		datam_darr_get(statelist, &state, sid);
		if(state->accept > ret)
			ret = state->accept;
	}
	return ret;
}

/* Execute an NFA w/ epsilons and shit */
uint32_t* regam_cheat(uint32_t *src, size_t n, regam_nfa *nfa, uint32_t *dst){
		datam_hashtable *state = datam_hashtable_new(datam_hashtable_i_indexer, datam_hashtable_i_cmp, 1),
										*swap = datam_hashtable_new(datam_hashtable_i_indexer, datam_hashtable_i_cmp, 1);
		regam_nstate *start;
		datam_darr_get(statelist, &start, nfa->start);
		cheat_calc_closure(start);
		datam_hashtable_addall(state, start->closure);
		uint32_t type = 0;
		uint32_t *stop = NULL;
		size_t pos;
		for(pos = 0; pos <= n; pos++){
			uint32_t cur_acc = cheat_calc_accept(state);
			if(cur_acc > 0){
				type = cur_acc - 1;
				stop = src + pos;
			}
			if(pos == n){
				break;
			}
			uint32_t sym = src[pos];
			cheat_calc_transitions(swap, state, sym);
			datam_hashtable_clear(state);
			cheat_calc_closures(state, swap);
			datam_hashtable_clear(swap);
			if(state->n == 0){
				break;
			}
		}
		if(dst != NULL)
			*dst = type;
		datam_hashtable_delete(state);
		datam_hashtable_delete(swap);
		return stop;
}