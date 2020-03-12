/*
NFA construction from a regex.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include <stdint.h>
#include "regam.h"

/* Regex parser source */
static char *regex_src =
"char open close star plus qmark bar bopen bclose carrot dash end\n\
regex prebar comp elem cclass ccol ctype start\n\
18 26\n\
start:regex end\n\
regex:regex bar prebar\n\
regex:prebar\n\
prebar:prebar comp\n\
prebar:comp\n\
comp:elem star\n\
comp:elem plus\n\
comp:elem qmark\n\
comp:elem\n\
elem:open regex close\n\
elem:bopen cclass bclose\n\
elem:char\n\
cclass:carrot ccol\n\
cclass:ccol\n\
ccol:ccol ctype\n\
ccol:ctype\n\
ctype:char\n\
ctype:char dash char\n\
S1 S2 E0 E0 E0 E0 E0 S3 E0 E0 E0 E0 G25 G24 G23 G19 E0 E0 E0 E0\n\
R11 R11 R11 R11 R11 R11 R11 R11 E0 E0 E0 R11 E0 E0 E0 E0 E0 E0 E0 E0\n\
S1 S2 E0 E0 E0 E0 E0 S3 E0 E0 E0 E0 G14 G24 G23 G19 E0 E0 E0 E0\n\
S4 E0 E0 E0 E0 E0 E0 E0 E0 S7 E0 E0 E0 E0 E0 E0 G11 G13 G10 E0\n\
R16 E0 E0 E0 E0 E0 E0 E0 R16 E0 S5 E0 E0 E0 E0 E0 E0 E0 E0 E0\n\
S6 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0\n\
R17 E0 E0 E0 E0 E0 E0 E0 R17 E0 R17 E0 E0 E0 E0 E0 E0 E0 E0 E0\n\
S4 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 G8 G10 E0\n\
S4 E0 E0 E0 E0 E0 E0 E0 R12 E0 E0 E0 E0 E0 E0 E0 E0 E0 G9 E0\n\
R14 E0 E0 E0 E0 E0 E0 E0 R14 E0 R14 E0 E0 E0 E0 E0 E0 E0 E0 E0\n\
R15 E0 E0 E0 E0 E0 E0 E0 R15 E0 R15 E0 E0 E0 E0 E0 E0 E0 E0 E0\n\
E0 E0 E0 E0 E0 E0 E0 E0 S12 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0\n\
R10 R10 R10 R10 R10 R10 R10 R10 E0 E0 E0 R10 E0 E0 E0 E0 E0 E0 E0 E0\n\
S4 E0 E0 E0 E0 E0 E0 E0 R13 E0 E0 E0 E0 E0 E0 E0 E0 E0 G9 E0\n\
E0 E0 S15 E0 E0 E0 S16 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0 E0\n\
R9 R9 R9 R9 R9 R9 R9 R9 E0 E0 E0 R9 E0 E0 E0 E0 E0 E0 E0 E0\n\
S1 S2 E0 E0 E0 E0 E0 S3 E0 E0 E0 E0 E0 G17 G23 G19 E0 E0 E0 E0\n\
S1 S2 R1 E0 E0 E0 R1 S3 E0 E0 E0 R1 E0 E0 G18 G19 E0 E0 E0 E0\n\
R3 R3 R3 R3 R3 R3 R3 R3 E0 E0 E0 R3 E0 E0 E0 E0 E0 E0 E0 E0\n\
R8 R8 R8 S20 S21 S22 R8 R8 E0 E0 E0 R8 E0 E0 E0 E0 E0 E0 E0 E0\n\
R5 R5 R5 R5 R5 R5 R5 R5 E0 E0 E0 R5 E0 E0 E0 E0 E0 E0 E0 E0\n\
R6 R6 R6 R6 R6 R6 R6 R6 E0 E0 E0 R6 E0 E0 E0 E0 E0 E0 E0 E0\n\
R7 R7 R7 R7 R7 R7 R7 R7 E0 E0 E0 R7 E0 E0 E0 E0 E0 E0 E0 E0\n\
R4 R4 R4 R4 R4 R4 R4 R4 E0 E0 E0 R4 E0 E0 E0 E0 E0 E0 E0 E0\n\
S1 S2 R2 E0 E0 E0 R2 S3 E0 E0 E0 R2 E0 E0 G18 G19 E0 E0 E0 E0\n\
A0 A0 A0 A0 A0 A0 S16 A0 A0 A0 A0 A0 E0 E0 E0 E0 E0 E0 E0 E0\n\
";

/* List of states while constructing all NFAs */
datam_darr *statelist = NULL;

/* Keeping track of states by keysets */
datam_hashtable *keysets = NULL;

/* Table for parsing regexes */
parsam_table *regex_table = NULL;

/* ============================================================================================================================ */
/* Cleanup functions */

regam_nstate* regam_nstate_new(){
    regam_nstate *ret = malloc(sizeof(regam_nstate));
    ret->transitions = datam_darr_new(sizeof(regam_transition));
    ret->accept = 0;
    ret->link = 0;
    ret->id = statelist->n;
    ret->closure = NULL;
    datam_darr_push(statelist, &ret);
    return ret;
}

void regam_nstate_delete(regam_nstate *state){
    if(state->transitions != NULL){
        datam_darr_delete(state->transitions);
    }
    if(state->closure != NULL){
		datam_hashbucket *bucket = datam_hashtable_has(keysets, state->closure);
		if(bucket != NULL && (datam_hashtable*)bucket->key == state->closure){
			datam_hashtable_remove(keysets, state->closure);
		}
		datam_hashtable_delete(state->closure);
    }
    regam_nstate *repl = NULL;
    datam_darr_set(statelist, &repl, state->id);
    free(state);
}

void regam_nfa_delete(regam_nfa *nfa){
		if(nfa == NULL)
			return;
    if(nfa->accepts != NULL){
        datam_darr_delete(nfa->accepts);
    }
    free(nfa);
}

void regam_nfa_start(){
	if(regex_table != NULL){
		return;
	}
    // FILE* open = fopen("C:/users/alpac/documents/github/datamatum/parser/regex.txt", "r"); /* I'm leaving this in case I need it again */
    FILE *open = tmpfile();
    for(char *src_ptr = regex_src; *src_ptr; src_ptr++){
      fputc(*src_ptr, open);
    }
    fflush(open);
    rewind(open);
    regex_table = parsam_table_read(open);
    fclose(open);
    if(statelist != NULL){
        datam_darr_delete(statelist);
    }
    statelist = datam_darr_new(sizeof(regam_nstate*));
    regam_nstate *dummy = NULL;
    datam_darr_push(statelist, &dummy);
    if(keysets == NULL){
        keysets = datam_hashtable_new(datam_hashtable_ptrset_indexer, datam_hashtable_ptrset_comp, 2);
    }
}

void regam_nfa_end(){
    if(statelist != NULL){
        regam_nstate *nstate;
        while(statelist->n > 0){
            datam_darr_pop(statelist, &nstate);
            if(nstate != NULL){
                regam_nstate_delete(nstate);
            }
        }
        datam_darr_delete(statelist);
        statelist = NULL;
    }
    if(keysets != NULL){
        for(datam_hashbucket *bucket = datam_hashtable_next(keysets, NULL);
            bucket != NULL;
            bucket = datam_hashtable_next(keysets, bucket)){
                datam_hashtable *keyset = (datam_hashtable*)bucket->key;
                datam_hashtable_delete(keyset);
        }
        datam_hashtable_delete(keysets);
    }
    if(regex_table != NULL){
        parsam_table_delete(regex_table);
		regex_table = NULL;
    }
}

/* ============================================================================================================================ */
/* NFA construction functions */

regam_nfa* regam_nfa_single(regam_cclass *src){
    regam_nfa *ret = malloc(sizeof(regam_nfa));
    ret->accepts = datam_darr_new(sizeof(uint32_t));
    ret->start = statelist->n;
    regam_nstate *start = regam_nstate_new();
    uint32_t accept = statelist->n;
    datam_darr_push(ret->accepts, &accept);
    regam_nstate *end = regam_nstate_new();
    end->accept = 1;
    int dot = 0; /* Once we encounter a dot we must end looping. Ideally this wouldn't happen but hey we might deal with morons */
    for(size_t i = 0; i < src->n && !dot; i ++){
        regam_crange *range = src->ranges + i;
        switch(range->spec){
            case Whitespace:{
                regam_transition nl = {'\t', '\n', end->id, src->exclude};
                datam_darr_push(start->transitions, &nl);
                regam_transition space = {' ', ' ', end->id, src->exclude};
                datam_darr_push(start->transitions, &space);
                break;
            }
            case Alpha:{
                regam_transition upper = {'A', 'Z', end->id, src->exclude};
                datam_darr_push(start->transitions, &upper);
                regam_transition lower = {'a', 'z', end->id, src->exclude};
                datam_darr_push(start->transitions, &lower);
                break;
            }
            case Digit:{
                regam_transition nums = {'0', '9', end->id, src->exclude};
                datam_darr_push(start->transitions, &nums);
                break;
            }
            case Alphanum:{
                regam_transition nums = {'0', '9', end->id, src->exclude};
                datam_darr_push(start->transitions, &nums);
                regam_transition upper = {'A', 'Z', end->id, src->exclude};
                datam_darr_push(start->transitions, &upper);
                regam_transition lower = {'a', 'z', end->id, src->exclude};
                datam_darr_push(start->transitions, &lower);
                break;
            }
            case Punc:{
                regam_transition p1 = {33, 47, end->id, src->exclude};
                datam_darr_push(start->transitions, &p1);
                regam_transition p2 = {58, 64, end->id, src->exclude};
                datam_darr_push(start->transitions, &p2);
                regam_transition p3 = {91, 96, end->id, src->exclude};
                datam_darr_push(start->transitions, &p3);
                regam_transition p4 = {123, 126, end->id, src->exclude};
                datam_darr_push(start->transitions, &p4);
                break;
            }
            case Ascii:{
                regam_transition ascii = {0, 127, end->id, src->exclude};
                datam_darr_push(start->transitions, &ascii);
                break;
            }
            case Dot:{
                start->transitions->n = 0;
                regam_transition empty = {0, UNICODE_MAX, end->id, src->exclude};
                datam_darr_push(start->transitions, &empty);
                dot = 1;
                break;
            }
            case None:{
                regam_transition tran = {range->start, range->end, end->id, src->exclude};
                datam_darr_push(start->transitions, &tran);
                break;
            }
        }
    }
    return ret;
}

void regam_nfa_unmark(char m){
    regam_nstate *tmp;
    for(size_t i = 1; i < statelist->n; i ++){
        datam_darr_get(statelist, &tmp, i);
        if(tmp != NULL){
            tmp->mark = m;
        }
    }
}

size_t regam_nfa_tctr(regam_nstate *start){
    if(start->mark){
        return 0;
    }
    size_t ret = 1;
    start->mark = 1;
    regam_transition t;
    for(size_t i = 0; i < start->transitions->n; i ++){
        datam_darr_get(start->transitions, &t, i);
        regam_nstate *next;
        datam_darr_get(statelist, &next, t.target);
        ret += regam_nfa_tctr(next);
    }
    return ret;
}

void regam_nstate_clone(uint32_t srcid){
    regam_nstate *src;
    datam_darr_get(statelist, &src, srcid);
    if(!src->mark){
        return;
    }
    src->mark = 0;
    src->link = statelist->n;
    regam_nstate *clone = regam_nstate_new();
    clone->accept = src->accept;
    regam_transition t;
    for(size_t i = 0; i < src->transitions->n; i ++){
        datam_darr_get(src->transitions, &t, i);
        regam_nstate *target;
        regam_nstate_clone(t.target);
        datam_darr_get(statelist, &target, t.target);
        t.target = target->link;
        datam_darr_push(clone->transitions, &t);
    }
}

regam_nfa* regam_nfa_clone(regam_nfa *src){
    regam_nfa *ret = malloc(sizeof(regam_nfa));
    ret->accepts = datam_darr_new(sizeof(uint32_t));
    regam_nstate *src_start;
    datam_darr_get(statelist, &src_start, src->start);
    regam_nfa_unmark(0);
    regam_nstate_clone(src->start);
    ret->start = src_start->link;
    uint32_t accept;
    regam_nstate *astate;
    for(size_t i = 0; i < src->accepts->n; i ++){
        datam_darr_get(src->accepts, &accept, i);
        datam_darr_get(statelist, &astate, accept);
        accept = astate->link;
        datam_darr_push(ret->accepts, &accept);
    }
}

void regam_nfa_concat(regam_nfa *first, regam_nfa *second){
    uint32_t accept;
    regam_nstate *end1;
    for(size_t i = 0; i < first->accepts->n; i ++){
        datam_darr_get(first->accepts, &accept, i);
        datam_darr_get(statelist, &end1, accept);
        end1->accept = 0;
        regam_transition t = {0, 0, second->start, -1};
        datam_darr_insert(end1->transitions, &t, 0);
    }
    first->accepts->n = 0;
    for(size_t i = 0; i < second->accepts->n; i ++){
        datam_darr_get(second->accepts, &accept, i);
        datam_darr_push(first->accepts, &accept);
    }
    regam_nfa_delete(second);
}

void regam_nfa_union(regam_nfa **list, size_t n){
    regam_nstate *start = regam_nstate_new();
    uint32_t in_id, out_id;
    regam_nstate *in_state, *o_state;
    for(size_t i = 0; i < n; i ++){
        in_id = list[i]->start;
        regam_transition t = {0, 0, in_id, -1};
        datam_darr_push(start->transitions, &t);
        datam_darr_get(statelist, &in_state, in_id);
        if(i > 0){
            for(size_t j = 0; j < list[i]->accepts->n; j ++){
                datam_darr_get(list[i]->accepts, &out_id, j);
                datam_darr_push(list[0]->accepts, &out_id);
            }
            regam_nfa_delete(list[i]);
        }else{
            o_state = in_state;
        }
    }
    list[0]->start = start->id;
}

void regam_nfa_qmark(regam_nfa *src){
    uint32_t oid;
    regam_nstate *oldacc;
    if(src->accepts->n > 1){
        regam_nstate *astate = regam_nstate_new();
        for(size_t i = 0; i < src->accepts->n; i ++){
            datam_darr_get(src->accepts, &oid, i);
            datam_darr_get(statelist, &oldacc, oid);
            if(i == 0){
                astate->accept = oldacc->accept;
            }
            oldacc->accept = 0;
            regam_transition t = {0, 0, astate->id, -1};
            datam_darr_insert(oldacc->transitions, &t, 0);
        }
        src->accepts->n = 0;
        oid = astate->id;
        datam_darr_push(src->accepts, &oid);
    }
    datam_darr_get(src->accepts, &oid, 0);
    datam_darr_get(statelist, &oldacc, src->start);
    regam_transition t = {0, 0, oid, -1};
    datam_darr_insert(oldacc->transitions, &t, 0);
}

void regam_nfa_star(regam_nfa *src){
    uint32_t oid;
    regam_nstate *oldacc, *nstate;
    if(src->accepts->n > 1){
        regam_nstate *astate = regam_nstate_new();
        for(size_t i = 0; i < src->accepts->n; i ++){
            datam_darr_get(src->accepts, &oid, i);
            datam_darr_get(statelist, &oldacc, oid);
            if(i == 0){
                astate->accept = oldacc->accept;
            }
            oldacc->accept = 0;
            regam_transition t = {0, 0, astate->id, -1};
            datam_darr_insert(oldacc->transitions, &t, 0);
        }
        src->accepts->n = 0;
        oid = astate->id;
        datam_darr_push(src->accepts, &oid);
    }
    datam_darr_get(src->accepts, &oid, 0);
    datam_darr_get(statelist, &nstate, oid); /* Singular end state */
    datam_darr_get(statelist, &oldacc, src->start); /* Start state */
    regam_transition t = {0, 0, oid, -1};
    datam_darr_insert(oldacc->transitions, &t, 0);
    t = (regam_transition){0, 0, src->start, -1};
    datam_darr_insert(nstate->transitions, &t, 0);
}

void regam_nfa_plus(regam_nfa *src){
    uint32_t oid;
    regam_nstate *oldacc, *nstate;
    if(src->accepts->n > 1){
        regam_nstate *astate = regam_nstate_new();
        for(size_t i = 0; i < src->accepts->n; i ++){
            datam_darr_get(src->accepts, &oid, i);
            datam_darr_get(statelist, &oldacc, oid);
            if(i == 0){
                astate->accept = oldacc->accept;
            }
            oldacc->accept = 0;
            regam_transition t = {0, 0, astate->id, -1};
            datam_darr_insert(oldacc->transitions, &t, 0);
        }
        src->accepts->n = 0;
        oid = astate->id;
        datam_darr_push(src->accepts, &oid);
    }
    datam_darr_get(src->accepts, &oid, 0);
    datam_darr_get(statelist, &nstate, oid); /* Singular end state */
    regam_transition t = (regam_transition){0, 0, src->start, -1};
    datam_darr_insert(nstate->transitions, &t, 0);
}

regam_nfa* regam_nfa_cpiece(regam_cpiece *cpc){
    regam_nfa *piece;
    if(cpc->subexpr){
        piece = regam_nfa_build(cpc->match.regex);
    }else{
        piece = regam_nfa_single(cpc->match.cclass);
    }
    switch(cpc->mod){
        case Optional: regam_nfa_qmark(piece); break;
        case Any: regam_nfa_star(piece); break;
        case Many: regam_nfa_plus(piece); break;
    }
    return piece;
}

regam_nfa* regam_nfa_opiece(regam_opiece *opc){
    regam_nfa *first = regam_nfa_cpiece(opc->pieces);
    for(size_t i = 1; i < opc->n; i ++){
        regam_cpiece *cpc = opc->pieces + i;
        regam_nfa *next = regam_nfa_cpiece(cpc);
        regam_nfa_concat(first, next);
    }
    return first;
}

regam_nfa* regam_nfa_build(regam_regex *regex){
    regam_nfa **opcs = malloc(sizeof(regam_nfa*)*regex->n);
    for(size_t i = 0; i < regex->n; i ++){
        opcs[i] = regam_nfa_opiece(regex->pieces[i]);
    }
    if(regex->n > 1){
        regam_nfa_union(opcs, regex->n);
    }
    regam_nfa *ret = opcs[0];
    free(opcs);
    return ret;
}

void regam_nfa_print(regam_nfa *nfa){
    regam_nfa_unmark(0);
    regam_nstate *stst;
    datam_darr_get(statelist, &stst, nfa->start);
    regam_nfa_tctr(stst);
    for(size_t i = 0; i < statelist->n; i ++){
        datam_darr_get(statelist, &stst, i);
        if(stst == NULL){
            continue;
        }
        if(stst->mark == 0){
            continue;
        }
        printf("%d: ", stst->id);
        for(size_t j = 0; j < stst->transitions->n; j ++){
            regam_transition t;
            datam_darr_get(stst->transitions, &t, j);
            printf("->%d@", t.target);
            if(t.exclude == -1){
                printf("e ");
            }else if(t.exclude == 0){
                printf("[%d-%d] ", t.start, t.end);
            }else{
                printf("[^%d-%d] ", t.start, t.end);
            }
        }
        if(stst->accept != 0){
            printf("ACC %d", stst->accept);
        }
        if(i == nfa->start){
            printf("START");
        }
        printf("\n");
    }
}

void regam_nfa_tag(regam_nfa *nfa, uint32_t tag){
    regam_nstate *state;
	uint32_t sid;
    for(size_t i = 0; i < nfa->accepts->n; i ++){
        datam_darr_get(nfa->accepts, &sid, i);
		datam_darr_get(statelist, &state, sid);
        state->accept = tag;
    }
}