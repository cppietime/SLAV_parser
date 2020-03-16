/*
Code to convert NFA to DFA.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "regam.h"
#include "slavio.h"

extern datam_darr *statelist;

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
			fputc(' ', dst);
			datam_darr_get(state->transitions, &t, j);
			fprintf(dst, "%d %d %d %d", t.exclude, t.start, t.end, t.target);
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
		bin_fgets(line, 1024, src);
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
			t.exclude = strtol(eptr, &eptr, 10);
			t.start = strtol(eptr, &eptr, 10);
			t.end = strtol(eptr, &eptr, 10);
			t.target = strtol(eptr, &eptr, 10);
			datam_darr_push(state->transitions, &t);
		}
		ctr ++;
	}
	return ret;
}