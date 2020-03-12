/*
Create a hardcode .c file for a language
*/

#include <stdio.h>
#include <stdlib.h>
#include "slavio.h"
#include "slav.h"

void slav_harbass(FILE *cfile, FILE *hfile, slav_lang *lang, char *fname, char *headername){
	FILE *save = tmpfile();
	slav_sugma(save, lang);
	fflush(save);
	rewind(save);
	fprintf(hfile,
"\
#ifndef _H_%s\n\
#define _H_%s\n\
#include \"slav.h\"\n\
void %s(slav_lang *lang);\n\
#endif\n\
", fname, fname, fname);
	fprintf(cfile,
"\
#include <stdio.h>\n\
#include <string.h>\n\
#include \"slav.h\"\n\
#include \"datam.h\"\n\
#include \"%s\"\\\\n\n\
static char *src = \"", headername);
	static char line[2048];
	while((fgets(line, 2048, save)) != NULL){
		fprintf(cfile, "%s\\\\n\n", line);
	}
	fclose(save);
	fprintf(cfile,
"\
\";\n\
void %s(slav_lang *lang){\n\
FILE *tmp = tmpfile();\n\
for(char *ptr = src; *ptr; ptr++){\n\
fputc(*ptr, tmp);\n\
}\n\
fflush(tmp);\n\
rewind(tmp);\n\
slav_ligma(tmp, lang);\n\
fclose(tmp);\n\
}\n\
", fname);
}