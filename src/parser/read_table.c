/*
Implementation of reading/constructing the shift-reduce table from a fully-defined file.
Written by Yaakov Schectman 2019.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "regam.h"
#include "parsam.h"
#include "slav.h"

static int str_pieces = 11;

int32_t strindex(void *data, size_t n){
    char *str = (char*)data;
    static int str_hash_mul = 31;
    size_t len = strlen(str);
    if(n >= len){
        return 0;
    }
    int32_t ret = 0;
    for(size_t i = n; i < len; i += str_pieces){
        ret *= str_hash_mul;
        ret += str[i];
    }
    return ret;
}

int substrcmp(void *a, void *b){
    char *ca = (char*)a, *cb = (char*)b;
    return strcmp(ca, cb);
}

void parsam_table_delete(parsam_table *table){
    if(table->symnames != NULL){
        for(datam_hashbucket *bucket = datam_hashtable_next(table->symnames, NULL);
            bucket != NULL;
            bucket = datam_hashtable_next(table->symnames, bucket)){
                free(bucket->key);
                free(bucket->value);
        }
        datam_hashtable_delete(table->symnames);
        table->symnames = NULL;
    }
    if(table->rules != NULL){
        for(size_t i = 0; i < table->n_rules; i ++){
            if(table->rules[i] != NULL){
                free(table->rules[i]);
            }
        }
        free(table->rules);
        table->rules = NULL;
    }
    if(table->states != NULL){
        for(size_t i = 0; i < table->n_states; i ++){
            if(table->states[i] != NULL){
                free(table->states[i]);
            }
        }
        free(table->states);
        table->states = NULL;
    }
    if(table->symids != NULL){
        free(table->symids);
    }
    free(table);
}

parsam_table *parsam_table_read(FILE *file){
    static char line[1024], temp[1024];
    parsam_table *table = malloc(sizeof(parsam_table));
    table->n_rules = 0;
    table->n_states = 0;
    table->rules = NULL;
    table->states = NULL;
    datam_hashtable *map = datam_hashtable_new(strindex, substrcmp, str_pieces);
    table->symnames = map;
    
    /* Read all terminals */
    int reading = 1;
    int lptr = 0, tptr = 0;
    int counter = 0;
    int n_term = 0, n_nonterm = 0;
    datam_darr *names = datam_darr_new(sizeof(char*));
    while(reading){
        if(feof(file)){
            fprintf(stderr, "Error: EOF encountered while reading terminals!\n");
            parsam_table_delete(table);
            return NULL;
        }
        char ch = getc(file);
        if(isspace(ch)){
            if(lptr > 0){
                line[lptr] = 0;
                char *key = malloc(lptr+1);
                strcpy(key, line);
                datam_darr_push(names, &key);
                parsam_symbol *sym = malloc(sizeof(parsam_symbol));
                sym->type = Terminal;
                n_term ++;
                sym->id = counter++;
                datam_hashtable_put(map, key, sym);
                lptr = 0;
            }
            if(ch == '\n'){
                reading = 0;
            }
        }else{
            line[lptr++] = ch;
        }
    }
    table->n_term = n_term;
    
    /* Read all non-terminals */
    reading = 1;
    lptr = 0;
    counter = 0;
    while(reading){
        if(feof(file)){
            fprintf(stderr, "Error: EOF encountered while reading non-terminals!\n");
            parsam_table_delete(table);
            return NULL;
        }
        char ch = getc(file);
        if(isspace(ch)){
            if(lptr > 0){
                line[lptr] = 0;
                char *key = malloc(lptr+1);
                strcpy(key, line);
                datam_darr_push(names, &key);
                parsam_symbol *sym = malloc(sizeof(parsam_symbol));
                sym->type = Nonterminal;
                n_nonterm ++;
                sym->id = counter++;
                datam_hashtable_put(map, key, sym);
                lptr = 0;
            }
            if(ch == '\n'){
                reading = 0;
            }
        }else{
            line[lptr++] = ch;
        }
    }
    table->n_nonterm = n_nonterm;
    table->symids = malloc(sizeof(char*) * (n_term + n_nonterm));
    for(size_t i = 0; i < n_term + n_nonterm; i ++){
        datam_darr_pop(names, table->symids + n_term + n_nonterm - 1 - i);
    }
    datam_darr_delete(names);
    
    /* Get number of rules and states */
    int n_rules, n_rows;
    int read_items = fscanf(file, "%d %d", &n_rules, &n_rows);
    if(read_items == 0 || read_items == EOF){
        fprintf(stderr, "Error: Could not read number of rules and rows! Read %d\n", read_items);
        parsam_table_delete(table);
        return NULL;
    }
    table->n_rules = n_rules;
    table->n_states = n_rows;
    while(!feof(file) && getc(file)!='\n');
    if(feof(file)){
        fprintf(stderr, "Error: EOF encountered before any rules specified!\n");
            parsam_table_delete(table);
        return NULL;
    }
    
    /* Read rules */
    parsam_production **rules = malloc(sizeof(parsam_production*) * n_rules);
    for(size_t i = 0; i < n_rules; i ++){
        rules[i] = NULL;
    }
    table->rules = rules;
    datam_darr *list_syms = datam_darr_new(sizeof(parsam_symbol));
    for(size_t i = 0; i < n_rules; i ++){
        if(feof(file)){
            fprintf(stderr, "Error: EOF encountered while reading rule %u!\n", i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        fgets(line, 1024, file);
        char* colpos = strchr(line, ':');
        if(colpos == NULL){
            fprintf(stderr, "Error: No colon found while reading rule %u!\n", i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        strncpy(temp, line, colpos-line);
        temp[colpos-line] = 0;
        parsam_symbol *sym = datam_hashtable_get(map, temp);
        if(sym == NULL){
            fprintf(stderr, "Error: Undeclared symbol %s in rule %u!\n", temp, i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        if(sym->type == Terminal){
            fprintf(stderr, "Error: Attempt to create production rule for terminal symbol %s in rule %u!\n", temp, i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        lptr = colpos + 1 - line;
        tptr = 0;
        while(1){
            if(isspace(line[lptr]) || line[lptr] == 0){
                if(tptr > 0){
                    temp[tptr] = 0;
                    parsam_symbol *term = datam_hashtable_get(map, temp);
                    if(term == NULL){
                        fprintf(stderr, "Error: Undeclared symbol %s in rule %u!\n", temp, i);
                        parsam_table_delete(table);
                        datam_darr_delete(list_syms);
                        return NULL;
                    }
                    datam_darr_push(list_syms, term);
                    tptr = 0;
                }
                if(line[lptr] == '\n' || line[lptr] == 0){
                    break;
                }
            }else{
                temp[tptr++] = line[lptr];
            }
            lptr++;
        }
        parsam_production *rule = malloc(sizeof(parsam_production) + sizeof(parsam_symbol)*list_syms->n);
        rule->produces = sym->id;
        rule->n = list_syms->n;
        for(size_t j=0; j<rule->n; j++){
            datam_darr_pop(list_syms, rule->syms + rule->n - 1 - j);
        }
        table->rules[i] = rule;
    }
    datam_darr_delete(list_syms);
    
    /* Read rows */
    parsam_row **rows = malloc(sizeof(parsam_row*) * n_rows);
    for(size_t i = 0; i < n_rows; i ++){
        rows[i] = NULL;
    }
    table->states = rows;
    for(size_t i = 0; i < n_rows; i ++){
        parsam_row *row = malloc(sizeof(parsam_cell) * (n_term + n_nonterm));
        rows[i] = row;
        lptr = tptr = 0;
        fgets(line, 1024, file);
        for(size_t j = 0; j < n_term + n_nonterm; j ++){
            char *sppos = strchr(line+lptr, ' ');
            if(sppos == NULL){
                sppos = line+strlen(line);
            }
            tptr = sppos - line + lptr;
            strncpy(temp, line+lptr, tptr);
            lptr = sppos - line + 1;
            temp[tptr] = 0;
            switch(temp[0]){
                case 'E':
                    row->cells[j].action = Error;
                    row->cells[j].id = 0;
                    break;
                case 'S':
                    row->cells[j].action = Shift;
                    row->cells[j].id = strtol(temp+1, NULL, 10);
                    break;
                case 'G':
                    row->cells[j].action = Goto;
                    row->cells[j].id = strtol(temp+1, NULL, 10);
                    break;
                case 'R':
                    row->cells[j].action = Reduce;
                    row->cells[j].id = strtol(temp+1, NULL, 10);
                    break;
                case 'A':
                    row->cells[j].action = Accept;
                    row->cells[j].id = 0;
                    break;
                default:
                    fprintf(stderr, "Error: Unrecognized action %c at rule %u cell %u!\n", temp[0], i, j);
                    parsam_table_delete(table);
                    return NULL;
            }
        }
    }
        
    return table;
}

parsam_table *parsam_table_make(FILE *file){
    static char line[1024], temp[1024];
    parsam_table *table = malloc(sizeof(parsam_table));
    table->n_rules = 0;
    table->n_states = 0;
    table->rules = NULL;
    table->states = NULL;
    datam_hashtable *map = datam_hashtable_new(strindex, substrcmp, str_pieces);
    table->symnames = map;
    
    /* Read all terminals */
    int reading = 1;
    int lptr = 0, tptr = 0;
    int counter = 0;
    int n_term = 0, n_nonterm = 0;
    datam_darr *names = datam_darr_new(sizeof(char*));
    
    while(reading){
        if(feof(file)){
            fprintf(stderr, "Error: EOF encountered while reading terminals!\n");
            parsam_table_delete(table);
            return NULL;
        }
        char ch = getc(file);
        if(isspace(ch)){
            if(lptr > 0){
                line[lptr] = 0;
                char *key = malloc(lptr+1);
                strcpy(key, line);
                datam_darr_push(names, &key);
                parsam_symbol *sym = malloc(sizeof(parsam_symbol));
                sym->type = Terminal;
                n_term ++;
                sym->id = counter++;
                datam_hashtable_put(map, key, sym);
                lptr = 0;
            }
            if(ch == '\n'){
                reading = 0;
            }
        }else{
            line[lptr++] = ch;
        }
    }
    table->n_term = n_term;
    
    /* Read all non-terminals */
    reading = 1;
    lptr = 0;
    counter = 0;
    while(reading){
        if(feof(file)){
            fprintf(stderr, "Error: EOF encountered while reading non-terminals!\n");
            parsam_table_delete(table);
            return NULL;
        }
        char ch = getc(file);
        if(isspace(ch)){
            if(lptr > 0){
                line[lptr] = 0;
                char *key = malloc(lptr+1);
                strcpy(key, line);
                datam_darr_push(names, &key);
                parsam_symbol *sym = malloc(sizeof(parsam_symbol));
                sym->type = Nonterminal;
                n_nonterm ++;
                sym->id = counter++;
                datam_hashtable_put(map, key, sym);
                lptr = 0;
            }
            if(ch == '\n'){
                reading = 0;
            }
        }else{
            line[lptr++] = ch;
        }
    }
    table->n_nonterm = n_nonterm;
    table->symids = malloc(sizeof(char*) * (n_term + n_nonterm));
    for(size_t i = 0; i < n_term + n_nonterm; i ++){
        datam_darr_pop(names, table->symids + n_term + n_nonterm - 1 - i);
    }
    datam_darr_delete(names);
    
    /* Get number of rules and states */
    int n_rules, n_rows;
    int read_items = fscanf(file, "%d", &n_rules);
    if(read_items == 0 || read_items == EOF){
        fprintf(stderr, "Error: Could not read number of rules and rows! Read %d\n", read_items);
        parsam_table_delete(table);
        return NULL;
    }
    table->n_rules = n_rules;
    table->n_states = n_rows;
    while(!feof(file) && getc(file)!='\n');
    if(feof(file)){
        fprintf(stderr, "Error: EOF encountered before any rules specified!\n");
            parsam_table_delete(table);
        return NULL;
    }
    
    /* Read rules */
    parsam_production **rules = malloc(sizeof(parsam_production*) * n_rules);
    for(size_t i = 0; i < n_rules; i ++){
        rules[i] = NULL;
    }
    table->rules = rules;
    datam_darr *list_syms = datam_darr_new(sizeof(parsam_symbol));
    for(size_t i = 0; i < n_rules; i ++){
        if(feof(file)){
            fprintf(stderr, "Error: EOF encountered while reading rule %u!\n", i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        fgets(line, 1024, file);
        char* colpos = strchr(line, ':');
        if(colpos == NULL){
            fprintf(stderr, "Error: No colon found while reading rule %u!\n", i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        strncpy(temp, line, colpos-line);
        temp[colpos-line] = 0;
        parsam_symbol *sym = datam_hashtable_get(map, temp);
        if(sym == NULL){
            fprintf(stderr, "Error: Undeclared symbol %s in rule %u!\n", temp, i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        if(sym->type == Terminal){
            fprintf(stderr, "Error: Attempt to create production rule for terminal symbol %s in rule %u!\n", temp, i);
            parsam_table_delete(table);
            datam_darr_delete(list_syms);
            return NULL;
        }
        lptr = colpos + 1 - line;
        tptr = 0;
        while(1){
            if(isspace(line[lptr]) || line[lptr] == 0){
                if(tptr > 0){
                    temp[tptr] = 0;
                    parsam_symbol *term = datam_hashtable_get(map, temp);
                    if(term == NULL){
                        fprintf(stderr, "Error: Undeclared symbol %s in rule %u!\n", temp, i);
                        parsam_table_delete(table);
                        datam_darr_delete(list_syms);
                        return NULL;
                    }
                    datam_darr_push(list_syms, term);
                    tptr = 0;
                }
                if(line[lptr] == '\n' || line[lptr] == 0){
                    break;
                }
            }else{
                temp[tptr++] = line[lptr];
            }
            lptr++;
        }
        parsam_production *rule = malloc(sizeof(parsam_production) + sizeof(parsam_symbol)*list_syms->n);
        rule->produces = sym->id;
        rule->n = list_syms->n;
        for(size_t j=0; j<rule->n; j++){
            datam_darr_pop(list_syms, rule->syms + rule->n - 1 - j);
        }
        table->rules[i] = rule;
    }
    datam_darr_delete(list_syms);
    
    parsam_construct(table);
    
    return table;
}

void parsam_table_print(parsam_table *table, FILE* dst){
    for(size_t i = 0; i < table->n_term; i ++){
        char *symname = table->symids[i];
        parsam_symbol *symbol = datam_hashtable_get(table->symnames, symname);
        if(i>0){
            fprintf(dst, " ");
        }
        fprintf(dst, "%s", symname);
    }
    fprintf(dst, "\n");
    for(size_t i = 0; i < table->n_nonterm; i ++){
        char *symname = table->symids[i + table->n_term];
        parsam_symbol *symbol = datam_hashtable_get(table->symnames, symname);
        if(i>0){
            fprintf(dst, " ");
        }
        fprintf(dst, "%s", symname);
    }
    fprintf(dst, "\n%d %d\n", table->n_rules, table->n_states);
    for(size_t i = 0; i < table->n_rules; i ++){
        parsam_production *rule = table->rules[i];
        fprintf(dst, "%s:", table->symids[rule->produces + table->n_term]);
        for(size_t j = 0; j < rule->n; j ++){
            parsam_symbol sym = rule->syms[j];
            int id = sym.id;
            if(sym.type == Nonterminal){
                id += table->n_term;
            }
            if(j>0){
                fprintf(dst, " ");
            }
            fprintf(dst, "%s", table->symids[id]);
        }
        fprintf(dst, "\n");
    }
    for(size_t i = 0; i < table->n_states; i ++){
        parsam_row *row = table->states[i];
        for(size_t j = 0; j < table->n_term + table->n_nonterm; j ++){
            parsam_cell cell = row->cells[j];
            char c = 'E';
            switch(cell.action){
                case Accept:
                    c = 'A';
                    break;
                case Shift:
                    c = 'S';
                    break;
                case Reduce:
                    c = 'R';
                    break;
                case Goto:
                    c = 'G';
                    break;
            }
            if(j>0){
                fprintf(dst, " ");
            }
            fprintf(dst, "%c%d", c, cell.id);
        }
        fprintf(dst, "\n");
    }
}

parsam_ast* producer(){
    static int num = 0;
    static char *toks[] = {
        "a",
        "(",
        "\\s",
        "C",
        "*",
        ")",
        "+",
        "q",
        "|",
        "(",
        "0",
        ")",
        "|",
        "z",
        "[",
        "^",
        "A",
        "-",
        "\\x69",
        "d",
        "]",
        "$"
    };
    static int ids[] = {
        0,
        1,
        0,
        0,
        3,
        2,
        4,
        0,
        6,
        1,
        0,
        2,
        6,
        0,
        7,
        9,
        0,
        10,
        0,
        0,
        8,
        11
    };
    if(num>=22){
        return NULL;
    }
    parsam_ast *ret = malloc(sizeof(parsam_ast));
    ret->subtrees = NULL;
    char *lex = toks[num];
    uint32_t *nlex = malloc(4 * (strlen(lex) + 1));
    strwstr(nlex, lex);
    ret->lexeme = nlex;
    ret->symbol = (parsam_symbol){.type = Terminal, .id = ids[num]};
    num++;
    return ret;
}

uint32_t adv_unicode(char **src){
  int first = **src;
  (*src)++;
	/* Check BOM */
	if(first == 0xef){
		char *reset = *src;
		int bom = 1;
		if(*((*src)++) != 0xbb)
			bom = 0;
		if(*((*src)++) != 0xbf)
			bom = 0;
		if(bom)
			first = *((*src)++);
		else
			*src = reset;
	}
  uint32_t uni = first;
  if((first & 0xe0) == 0xc0){
    int sec = **src;
    (*src)++;
    uni = first & 0x1f;
    uni <<= 6;
    uni += sec & 0x3f;
  }
  else if((first & 0xf0) == 0xe0){
    int sec = **src;
    (*src)++;
    int third = **src;
    (*src)++;
    uni = first & 0x0f;
    uni <<= 6;
    uni += sec & 0x3f;
    uni <<= 6;
    uni += third & 0x3f;
  }
  else if((first & 0xf8) == 0xf0){
    int sec = **src;
    (*src)++;
    int third = **src;
    (*src)++;
    int fourth = **src;
    (*src)++;
    uni = first & 0x07;
    uni <<= 6;
    uni += sec & 0x3f;
    uni <<= 6;
    uni += third & 0x3f;
    uni <<= 6;
    uni += fourth & 0x3f;
  }
  return uni;
}

void strwstr(uint32_t *wstr, char *str){
  int i = 0;
  while(*str){
    wstr[i++] = adv_unicode(&str);
  }
  wstr[i] = 0;
}

void strnwstr(uint32_t *wstr, char *str, size_t n){
  int i = 0;
  while(*str && i < n){
    wstr[i++] = adv_unicode(&str);
  }
  wstr[i] = 0;
}

void wstrcpy(uint32_t *dst, uint32_t *src){
  for(uint32_t *ptr = src; *ptr; ptr++){
    *(dst++) = *ptr;
  }
  *dst = 0;
}

void wstrncpy(uint32_t *dst, uint32_t *src, size_t n){
  int i;
  for(i=0; src[i] && i < n; i++){
    dst[i] = src[i];
  }
  dst[i] = 0;
}

int wstrncmp(uint32_t *a, uint32_t *b, size_t n){
  for(int i = 0; i < n; i++){
    int dif = (int)(a[i]) - (int)(b[i]);
    if(dif)
      return dif;
  }
  return 0;
}

void fprintw(FILE *file, uint32_t *wstr){
  for(uint32_t *ptr = wstr; *ptr; ptr++){
		if(*ptr <= 127 && *ptr != '\n' && *ptr != '\r')
			fputc(*ptr, file);
		else
			fprintf(file, "[\\x%x]", *ptr);
  }
}

void fprintwn(FILE *file, uint32_t *wstr, size_t n){
  for(uint32_t *ptr = wstr; *ptr && ptr < wstr + n; ptr++){
		if(*ptr <= 127 && *ptr != '\n' && *ptr != '\r')
			fputc(*ptr, file);
		else
			fprintf(file, "[\\x%x]", *ptr);
  }
}

long wstrtol(uint32_t *wstr, uint32_t **end, int radix){
  static char onechar[2] = {0, 0};
  long ret = 0;
  uint32_t *ptr;
  for(ptr = wstr; *ptr; ptr++){
		onechar[0] = *ptr;
		if( (*ptr >= '0' && *ptr <= '9') || (*ptr >= 'A' && *ptr <= 'F') || (*ptr >= 'a' && *ptr <= 'f') ){
			ret *= radix;
			ret += strtol(onechar, NULL, radix);
		}
		else break;
  }
  if(end != NULL)
    *end = ptr;
	return ret;
}

// int main(){
	// regam_nfa_start();
	// FILE *srcfil = fopen("C:/users/alpac/documents/github/datamatum/parser/shift.txt", "r");
	// slav_lang lang;
	// slav_updog(srcfil, &lang);
	// fclose(srcfil);
	// srcfil = fopen("C:/users/alpac/documents/github/datamatum/parser/save.txt", "w");
	// slav_sugma(srcfil, &lang);
	// parsam_table_print(lang.table, stdout);
	// regam_nfa_print(lang.dfa);
	// fclose(srcfil);
	// slav_squat(&lang);
	// srcfil = fopen("C:/users/alpac/documents/github/datamatum/parser/save.txt", "r");
	// slav_ligma(srcfil, &lang);
	// fclose(srcfil);
	// srcfil = fopen("C:/users/alpac/documents/github/datamatum/parser/test.txt", "r");
	// parsam_ast *res = slav_joe(srcfil, &lang);
	// fclose(srcfil);
	// parsam_ast_print(res, lang.table, stdout);
	// parsam_ast_delete(res);
	// slav_squat(&lang);
	// regam_nfa_end();
	// return 0;
// }
