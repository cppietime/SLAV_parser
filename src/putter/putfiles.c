/*
File IO functions for Putter
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "putter.h"
#include "datam.h"
#include "putops.h"
#include "slavio.h"
#include "bigint.h"

#if (defined(_WIN32) || defined(__WIN32__))
#define mkdir(A, B) mkdir(A)
#endif

file_wrapper* new_file(char* path){
	file_wrapper *ret = malloc(sizeof(file_wrapper));
	ret->path = path;
	ret->status = 0;
	if(stat(path, &(ret->fstat)) == 0){
		if(ret->fstat.st_mode & S_IFDIR)
			ret->status |= STATUS_DIR;
		ret->status |= STATUS_EXISTS;
	}
	else{
		char last = path[strlen(path) - 1];
		if(last == '/')
			ret->status |= STATUS_DIR;
	}
	ret->bitptr = 0;
	ret->bitbuf = 0;
	ret->putback = 0;
	ret->encoding = TYPE_UNKNOWN;
	ret->file = NULL;
	return ret;
}

uservar* new_filevar(datam_darr *wstr){
	size_t len = wstr->n;
	char *path = malloc(len);
	int32_t ch;
	for(size_t i = 0; i < len; i++){
		datam_darr_get(wstr, &ch, i);
		path[i] = ch;
	}
	file_wrapper *of = new_file(path);
	uservar *var = new_variable();
	var->type = open_file;
	var->values.file_val = of;
	return var;
}

void file_setmode(file_wrapper *fw, uservar *mvar){
	datam_darr *mstr = put_tostring(mvar);
	int32_t ch;
	for(size_t i = 0; i < mstr->n - 1; i++){
		datam_darr_get(mstr, &ch, i);
		switch(ch){
			case 'r':
				fw->status |= STATUS_READ; break;
			case 'w':
				fw->status |= STATUS_WRITE; break;
			case '+':
				fw->status |= STATUS_READ | STATUS_WRITE; break;
			case 'a':
				fw->status |= STATUS_APPEND; break;
			case '8':
				fw->encoding = TYPE_UTF8; break;
			case 'b':
				fw->encoding = TYPE_UTF16BE; break;
			case 'l':
				fw->encoding = TYPE_UTF16LE; break;
			case 'B':
				fw->encoding = TYPE_UTF32BE; break;
			case 'L':
				fw->encoding = TYPE_UTF32LE; break;
			case '0':
				fw->encoding = TYPE_BINARY; break;
		}
	}
	datam_darr_delete(mstr);
}

void open_filevar(file_wrapper *fw){
	static char modestr[4];
	if(fw->status & STATUS_DIR){
		int mode = 777;
		if(mkdir(fw->path, mode) != 0){
			fprintf(stderr, "Warning: Could not create directory %s with mode %d\n", fw->path, mode);
		}
		else{
			fw->status |= STATUS_EXISTS;
		}
	}
	else{
		char *ptr = modestr;
		if(fw->status & STATUS_APPEND){
			*(ptr++) = 'a';
		}
		else if(fw->status & STATUS_WRITE){
			*(ptr++) = 'w';
		}
		else{
			*(ptr++) = 'r';
		}
		*(ptr++) = 'b';
		if((fw->status & STATUS_READ) && ((fw->status & STATUS_WRITE) || (fw->status & STATUS_APPEND))){
			*(ptr++) = '+';
		}
		*ptr = 0;
		FILE *file = fopen(fw->path, modestr);
		if(file){
			fw->file = file;
			fw->status |= STATUS_OPEN | STATUS_EXISTS;
		}
		else{
			fprintf(stderr, "Warning: Could not open file with name %s\n", fw->path);
		}
	}
}

void close_filevar(file_wrapper *fw){
	if(fw->status & STATUS_OPEN){
		fclose(fw->file);
		fw->file = NULL;
		fw->status &= ~STATUS_OPEN;
	}
}

file_wrapper* file_appended(file_wrapper *base, char *path){
	size_t llen = strlen(base->path);
	size_t rlen = strlen(path);
	char *join = malloc(llen + rlen + 2);
	char *ptr = join;
	strcpy(ptr, base->path);
	ptr += llen;
	if(base->path[llen - 1] != '/')
		*(ptr++) = '/';
	strcpy(ptr, path);
	return new_file(join);
}

void write_to_file(file_wrapper *fw, uservar *val){
	if(!(fw->status & STATUS_OPEN)){
		quit_for_error(8, "Error: Writing to unopened file\n");
	}
	if(!(fw->status & STATUS_WRITE) && !(fw->status & STATUS_APPEND)){
		quit_for_error(9, "Error: Writing to read-only file\n");
	}
	datam_darr *wstr = put_tostring(val);
	size_t len = wstr->n;
	uint32_t ch;
	for(size_t i = 0; i < len - 1; i++){
		datam_darr_get(wstr, &ch, i);
		fput_unicode(fw->file, fw->encoding, ch);
	}
	datam_darr_delete(wstr);
}

uint32_t file_next(file_wrapper *fw){
	if(!(fw->status & STATUS_OPEN)){
		quit_for_error(8, "Error: Reading from unopened file\n");
	}
	if(!(fw->status & STATUS_READ)){
		quit_for_error(9, "Error: Reading from write-only file\n");
	}
	uint32_t ret;
	if(fw->putback != 0){
		ret = fw->putback;
		fw->putback = 0;
	}
	else{
		ret = fget_unicode(fw->file, fw->encoding);
	}
	if(ret == '\r' && fw->encoding != TYPE_BINARY){
		uint32_t next;
		next = fget_unicode(fw->file, fw->encoding);
		if(next == '\n')
			ret = '\n';
		else
			fw->putback = next;
	}
	return ret;
}

datam_darr* read_from_file(file_wrapper *fw, size_t n){
	if(!(fw->status & STATUS_OPEN)){
		quit_for_error(8, "Error: Reading from unopened file\n");
	}
	if(!(fw->status & STATUS_READ)){
		quit_for_error(9, "Error: Reading from write-only file\n");
	}
	datam_darr *chrs = datam_darr_new(4);
	for(size_t i = 0; i < n; i++){
		uint32_t ch = file_next(fw);
		if(ch == (uint32_t)EOF){
			fw->status |= STATUS_EOF;
			break;
		}
		datam_darr_push(chrs, &ch);
	}
	uint32_t nul = 0;
	datam_darr_push(chrs, &nul);
	return chrs;
}

datam_darr* read_until(file_wrapper *fw, uint32_t delim){
	if(!(fw->status & STATUS_OPEN)){
		quit_for_error(8, "Error: Reading from unopened file\n");
	}
	if(!(fw->status & STATUS_READ)){
		quit_for_error(9, "Error: Reading from write-only file\n");
	}
	datam_darr *ret = datam_darr_new(4);
	uint32_t ch;
	do{
		ch = file_next(fw);
		if(ch == (uint32_t)EOF){
			fw->status |= STATUS_EOF;
			break;
		}
		if(ch == delim)
			break;
		datam_darr_push(ret, &ch);
	}while(ch != delim);
	ch = 0;
	datam_darr_push(ret, &ch);
	return ret;
}

void fw_seek(file_wrapper *fw, long skip, int mode){
	if(!(fw->status & STATUS_OPEN)){
		quit_for_error(8, "Error: Seeking unopened file\n");
	}
	if(mode == SEEK_CUR && fw->putback != 0)
		skip++;
	fw->putback = 0;
	fw->status &= ~STATUS_EOF;
	fseek(fw->file, skip, mode);
}

uint32_t file_flagval(uservar *var){
	datam_darr *wstr = put_tostring(var);
	uint32_t key;
	datam_darr_get(wstr, &key, 0);
	datam_darr_delete(wstr);
	switch(key){
		case 'r': return STATUS_READ;
		case 'w': return STATUS_WRITE;
		case 'a': return STATUS_APPEND;
		case 'd': return STATUS_DIR;
		case 'e': return STATUS_EXISTS;
		case 'o': return STATUS_OPEN;
		case 'f': return STATUS_EOF;
	}
	return key;
}

void read_big(binode_t *dst, file_wrapper *fw){
	static char lbuf[1024];
	char *ptr = lbuf;
	char hasdec = 0;
	uint32_t chr;
	while((chr = file_next(fw)) != (uint32_t)EOF){
		if(!(chr >= '0' && chr <= '9') && !(chr == '.' && !hasdec && dst->type == TYPE_BIGFIX) && !(chr == '-' && ptr == lbuf)){
			fw->putback = chr;
			break;
		}
		if(chr == '.')
			hasdec = 1;
		*(ptr++) = (char)chr;
	}
	*ptr = 0;
	strtobi_dec(dst, lbuf, NULL);
}

double read_float(file_wrapper *fw){
	static char lbuf[1024];
	char *ptr = lbuf;
	char hasdec = 0;
	uint32_t chr;
	while((chr = file_next(fw)) != (uint32_t)EOF){
		if(!(chr >= '0' && chr <= '9') && !(chr == '.' && !hasdec) && !(chr == '-' && ptr == lbuf)){
			fw->putback = chr;
			break;
		}
		if(chr == '.')
			hasdec = 1;
		*(ptr++) = (char)chr;
	}
	*ptr = 0;
	return strtod(lbuf, NULL);
}

datam_darr* read_string(file_wrapper *fw){
	return read_until(fw, '\n');
}
