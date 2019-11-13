/*
Builds regex object from AST.
Written by Yaakov Schectman 2019.
*/

#include <stdlib.h>
#include <stdio.h>
#include "regam.h"

/* Make an AST to crange */
void regam_crange_build(regam_crange *dst, parsam_ast *src){
    if(src->symbol.type == Terminal){
        fprintf(stderr, "Error: Attempting to build crange from terminal AST\n");
        return;
    }
    dst->spec = None;
    if(src->symbol.id == Ctype){
        if(src->n == 1){ /* Single character */
            parsam_ast *tok = src->subtrees[0];
            char *lex = tok->lexeme;
            if(lex[0] == '\\'){ /* Deal with escapes */
                dst->start = dst->end = 0;
                switch(lex[1]){
                    case 's': dst->spec = Whitespace; break;
                    case 'l': dst->spec = Alpha; break;
                    case 'd': dst->spec = Digit; break;
                    case 'w': dst->spec = Alphanum; break;
                    case 'p': dst->spec = Punc; break;
                    case 'a': dst->spec = Ascii; break;
                    case 'n': dst->start = dst->end = '\n'; break;
                    case 't': dst->start = dst->end = '\t'; break;
                    case 'x': {
                        dst->start = dst->end = strtol(lex+2, NULL, 16);
                        break;
                    }
                    default: dst->start = dst->end = lex[1]; break;
                }
            }else if(lex[0] == '.'){
                dst->end = dst->start = 0;
                dst->spec= Dot;
            }else{ /* Literal character */
                dst->start = dst->end = lex[0];
            }
        }else if(src->n == 3){ /* Range of characters */
            parsam_ast *first = src->subtrees[0], *last = src->subtrees[2];
            char *lex0 = first->lexeme, *lex1 = last->lexeme;
            if(lex0[0]=='\\'){
                if(lex0[1] == 'x'){
                    dst->start = strtol(lex0+2, NULL, 16);
                }else{
                    dst->start = lex0[1];
                }
            }else{
                dst->start = lex0[0];
            }
            if(lex1[0]=='\\'){
                if(lex1[1] == 'x'){
                    dst->end = strtol(lex1+2, NULL, 16);
                }else{
                    dst->end = lex1[1];
                }
            }else{
                dst->end = lex1[0];
            }
        }else{
            fprintf(stderr, "Error: Cannot process ctype with length %d\n", src->n);
        }
    }else if(src->symbol.id == Elem){
        parsam_ast *tok = src->subtrees[0];
        char *lex = tok->lexeme;
        if(lex[0] == '\\'){ /* Deal with escapes */
            dst->start = dst->end = 0;
            switch(lex[1]){
                case 's': dst->spec = Whitespace; break;
                case 'l': dst->spec = Alpha; break;
                case 'd': dst->spec = Digit; break;
                case 'w': dst->spec = Alphanum; break;
                case 'p': dst->spec = Punc; break;
                case 'a': dst->spec = Ascii; break;
                case 'n': dst->start = dst->end = '\n'; break;
                case 't': dst->start = dst->end = '\t'; break;
                case 'x': {
                    dst->start = dst->end = strtol(lex+2, NULL, 16);
                    break;
                }
                default: dst->start = dst->end = lex[1]; break;
            }
        }else if(lex[0]=='.'){
            dst->start = dst->end = 0;
            dst->spec = Dot;
        }else{ /* Literal character */
            dst->start = dst->end = lex[0];
        }
    }else{
        fprintf(stderr, "Error: Invalid AST type for building crange\n");
    }
    if(dst->end < dst->start){
        uint32_t tmp = dst->start;
        dst->start = dst->end;
        dst->end = tmp;
    }
}

/* Build a character class of one or more chars */
regam_cclass* regam_cclass_build(parsam_ast *src){
    if(src->symbol.type == Terminal || src->symbol.id != Elem){
        fprintf(stderr, "Error: Cannot build cclass from non-element AST\n");
        return NULL;
    }
    if(src->n == 3 && src->subtrees[0]->symbol.id == Lbrack){ /* [ cclass ] */
        parsam_ast *cclass = src->subtrees[1];
        parsam_ast *ccol;
        char exclude = 0;
        if(cclass->n == 1){
            ccol = cclass->subtrees[0];
        }else{
            ccol = cclass->subtrees[1];
            exclude = 1;
        }
        size_t len = 1;
        for(parsam_ast *subcol = ccol; subcol->n == 2; subcol = subcol->subtrees[0]){
            if(subcol->symbol.type == Terminal || subcol->symbol.id != Ccol){
                fprintf(stderr, "Error: Parsing non-ccol AST as ccol\n");
                return NULL;
            }
            len ++;
        }
        regam_cclass *ret = malloc(sizeof(regam_cclass) + sizeof(regam_crange) * len);
        ret->n = len;
        ret->exclude = exclude;
        for(size_t i = 0; i < len; i ++){
            parsam_ast *ctype;
            if(i == len-1){
                ctype = ccol->subtrees[0];
            }else{
                ctype = ccol->subtrees[1];
            }
            regam_crange_build(ret->ranges + len - 1 - i, ctype);
            ccol = ccol->subtrees[0];
        }
        return ret;
    }else if(src->n == 1){ /* C */
        regam_cclass *ret = malloc(sizeof(regam_cclass) + sizeof(regam_crange));
        ret->n = 1;
        ret->exclude = 0;
        regam_crange_build(ret->ranges, src);
        return ret;
    }else{ /* Error */
        fprintf(stderr, "Error: Invalid element\n");
        return NULL;
    }
}

/* Build the regam_cpiece from an AST */
void regam_cpiece_build(regam_cpiece *dst, parsam_ast *src){
    if(src->symbol.type == Terminal || src->symbol.id != Comp){
        fprintf(stderr, "Error: Non-Comp AST attempting to build cpiece\n");
        return;
    }
    parsam_ast *elem = src->subtrees[0];
    if(elem->n == 3 && elem->subtrees[0]->symbol.id == Lpar){
        dst->match.regex = regam_regex_build(elem->subtrees[1]);
        dst->subexpr = 1;
    }else{
        dst->match.cclass = regam_cclass_build(elem);
        dst->subexpr = 0;
    }
    if(src->n == 1){
        dst->mod = Once;
    }else{
        parsam_ast *mod = src->subtrees[1];
        switch(mod->symbol.id){
            case Star: dst->mod = Any; break;
            case Plus: dst->mod = Many; break;
            case Qmark: dst->mod = Optional; break;
            default: fprintf(stderr, "Error: Incorrect modifier on cpiece\n");
        }
    }
}

/* Builds the pre-|-d pieces of a regex */
regam_opiece* regam_opiece_build(parsam_ast *src){
    if(src->symbol.type == Terminal || src->symbol.id != Prebar){
        fprintf(stderr, "Error: Non-prebar for opiece instead got %d:%d\n", src->symbol.type == Nonterminal, src->symbol.id);
        return NULL;
    }
    size_t len = 1;
    for(parsam_ast *prebar = src; prebar->n == 2; prebar = prebar->subtrees[0]){
        if(prebar->symbol.type == Terminal || prebar->symbol.id != Prebar){
            fprintf(stderr, "Error: Non-Prebar sub prebar\n");
            return NULL;
        }
        len ++;
    }
    regam_opiece *ret = malloc(sizeof(regam_opiece) + sizeof(regam_cpiece) * len);
    ret->n = len;
    for(size_t i = 0; i < len; i ++){
        parsam_ast *comp;
        if(i == len - 1){
            comp = src->subtrees[0];
        }else{
            comp = src->subtrees[1];
        }
        regam_cpiece_build(ret->pieces + len - 1 - i, comp);
        src = src->subtrees[0];
    }
    return ret;
}

/* Builds the regex */
regam_regex* regam_regex_build(parsam_ast *src){
    if(src->symbol.type == Terminal || src->symbol.id != Regex){
        fprintf(stderr, "Error: Non-regex AST\n");
        return NULL;
    }
    size_t len = 1;
    for(parsam_ast *regex = src; regex->n == 3; regex = regex->subtrees[0]){
        if(regex->symbol.type == Terminal || regex->symbol.id != Regex){
            fprintf(stderr, "Error: Non-Regex sub regex\n");
            return NULL;
        }
        len ++;
    }
    regam_regex *ret = malloc(sizeof(regam_regex) + sizeof(regam_opiece*) * len);
    ret->n = len;
    for(size_t i = 0; i < len; i ++){
        parsam_ast *prebar;
        if(i == len - 1){
            prebar = src->subtrees[0];
        }else{
            prebar = src->subtrees[2];
        }
        ret->pieces[len - 1 - i] = regam_opiece_build(prebar);
        src = src->subtrees[0];
    }
    return ret;
}

/* =========================================================================================================================== */
/* Below are functions used to print regex objects */

void regam_crange_print(regam_crange *crange){
    switch(crange->spec){
        case Whitespace: printf("\\s");break;
        case Digit: printf("\\d");break;
        case Alpha: printf("\\l");break;
        case Alphanum: printf("\\w");break;
        case Punc: printf("\\p");break;
        case Ascii: printf("\\a");break;
        case Dot: printf("{.}");break;
        default:{
            if(crange->start == crange->end){
                printf("%c", crange->start);
            }else{
                printf("%c-%c", crange->start, crange->end);
            }
        }
    }
}

void regam_cclass_print(regam_cclass *cclass){
    if(cclass->n > 1 || cclass->exclude || cclass->ranges[0].start != cclass->ranges[0].end){
        printf("[");
    }
    if(cclass->exclude){
        printf("^");
    }
    for(size_t i = 0; i < cclass->n; i ++){
        regam_crange *crange = cclass->ranges + i;
        regam_crange_print(crange);
    }
    if(cclass->n > 1 || cclass->exclude || cclass->ranges[0].start != cclass->ranges[0].end){
        printf("]");
    }
}

void regam_cpiece_print(regam_cpiece *cpiece){
    if(cpiece->subexpr){
        printf("(");
        regam_regex_print(cpiece->match.regex);
        printf(")");
    }else{
        regam_cclass_print(cpiece->match.cclass);
    }
    switch(cpiece->mod){
        case Optional: printf("?");break;
        case Many: printf("+");break;
        case Any: printf("*");break;
    }
}

void regam_opiece_print(regam_opiece *opiece){
    for(size_t i = 0; i < opiece->n; i ++){
        regam_cpiece *cpiece = opiece->pieces + i;
        regam_cpiece_print(cpiece);
    }
}

void regam_regex_print(regam_regex *regex){
    for(size_t i = 0; i < regex->n; i ++){
        regam_opiece *opiece = regex->pieces[i];
        regam_opiece_print(opiece);
        if(i < regex->n - 1){
            printf("|");
        }
    }
}

/* ======================================================================================================================================== */
/* Below are functions used to delete regex objects */

void regam_cpiece_delete(regam_cpiece *cpiece){
    if(cpiece->subexpr){
        regam_regex_delete(cpiece->match.regex);
    }else{
        free(cpiece->match.cclass);
    }
}

void regam_opiece_delete(regam_opiece *opiece){
    for(size_t i = 0; i < opiece->n; i ++){
        regam_cpiece *cpiece = opiece->pieces + i;
        regam_cpiece_delete(cpiece);
    }
    free(opiece);
}

void regam_regex_delete(regam_regex *regex){
    for(size_t i = 0; i < regex->n; i ++){
        regam_opiece *opiece = regex->pieces[i];
        regam_opiece_delete(opiece);
    }
    free(regex);
}