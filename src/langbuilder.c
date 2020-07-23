/*
Code to produce the c- and h- files for a language.
*/

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "slav.h"

int main(int argc, char **argv){
	static char path[1024] = ".";
	static char input[1024] = "slavlang.txt", prefix[1024] = "slavlang", buffer[1024], fname[1024] = "slavlang_get";
	int c;
	while((c = getopt(argc, argv, "d:i:o:f:")) != -1){
		switch(c){
			case 'd':
				strcpy(path, optarg); break;
			case 'i':
				strcpy(input, optarg); break;
			case 'o':
				strcpy(prefix, optarg); break;
			case 'f':
				strcpy(fname, optarg); break;
		}
	}
	char *eptr = path + strlen(path) - 1;
	while(eptr > path && *eptr == '/'){
		*(eptr--) = 0;
	}
	if(input[0] != '.' && input[0] != '/'){
		strcpy(buffer, input);
		sprintf(input, "%s/%s", path, buffer);
	}
	if(prefix[0] != '.' && input[0] != '/'){
		strcpy(buffer, prefix);
		sprintf(prefix, "%s/%s", path, buffer);
	}
	printf("Args: %s: %s: %s: %s\n", path, input, prefix, fname);
	regam_nfa_start();
	FILE *in_file = fopen(input, "r");
	slav_lang lang;
	slav_updog(in_file, &lang);
	printf("Updogged\n");
	fclose(in_file);
	if(lang.table == NULL){
		fprintf(stderr, "Error: Could not parse language!\n");
		slav_squat(&lang);
		return 1;
	}
	printf("Constructed language\n");
	sprintf(input, "%s.c", prefix);
	sprintf(buffer, "%s.h", prefix);
	FILE *cfile = fopen(input, "w");
	FILE *hfile = fopen(buffer, "w");
	slav_hardbass(cfile, hfile, &lang, fname, buffer);
	fclose(cfile);
	fclose(hfile);
	slav_squat(&lang);
	regam_nfa_end();
	return 0;
}