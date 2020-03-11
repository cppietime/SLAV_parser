/*
Reads regex descriptions from files.
Written by Yaakov Schectman 2019.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "regam.h"

static FILE *src_file = NULL;

void regam_setsrc_file(FILE *src, int close){
    if(close && src_file != NULL){
        fclose(src_file);
    }
    src_file = src;
}

/* Internal use functions for reading regex tokens */
static int reserve = -1;
int regam_charget(){
    if(reserve == -1){
        return fgetc(src_file);
    }else{
        int ret = reserve;
        reserve = -1;
        return ret;
    }
}
void regam_charput(int i){
    reserve = i;
}

parsam_ast* regam_producer(){
    parsam_ast *ret = malloc(sizeof(parsam_ast));
    ret->subtrees = NULL;
    ret->n = 0;
    int first = regam_charget();
    if(first == '\n' || first == '\r' || src_file == NULL || (feof(src_file) && reserve == -1)){
        ret->filename = NULL;
        ret->line_no = -1;
        ret->lexeme = malloc(8);
        strwstr(ret->lexeme, "$");
        ret->symbol = (parsam_symbol){.type = Terminal, .id = Dsign};
        return ret;
    }
    static char line[1024];
    int lptr = 0;
    line[lptr++] = first;
    switch(line[0]){
        case '(':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "(");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Lpar};
            break;
        case ')':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, ")");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Rpar};
            break;
        case '[':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "[");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Lbrack};
            break;
        case ']':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "]");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Rbrack};
            break;
        case '^':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "^");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Carrot};
            break;
        case '|':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "|");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Vbar};
            break;
        case  '-':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "-");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Dash};
            break;
        case '?':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "?");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Qmark};
            break;
        case '*':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "*");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Star};
            break;
        case '+':
            ret->lexeme = malloc(8);
            strwstr(ret->lexeme, "+");
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Plus};
            break;
        default:
            ret->symbol = (parsam_symbol){.type = Terminal, .id = Literal};
            if(line[0] == '\\'){
                line[lptr++] = regam_charget();
                if(line[1] == 'x'){
                    while(1){
                        int nex = regam_charget();
                        if((nex >= '0' && nex <= '9') || (nex >= 'A' && nex <= 'F') || (nex >= 'a' && nex <= 'f')){
                            line[lptr++] = nex;
                        }else{
                            regam_charput(nex);
                            break;
                        }
                    }
                    line[lptr++] = 0;
                    ret->lexeme = malloc(lptr * 4);
                    strwstr(ret->lexeme, line);
                }else{
                    ret->lexeme = malloc(12);
                    strnwstr(ret->lexeme, line, 2);
                    ret->lexeme[2] = 0;
                }
            }else{
                ret->lexeme = malloc(8);
                ret->lexeme[0] = line[0];
                ret->lexeme[1] = 0;
            }
    }
    return ret;
}