#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "datam.h"
#include "modeling.h"

unimesh* unimesh_new(){
	unimesh *mesh = malloc(sizeof(unimesh));
	mesh->vertices = datam_darr_new(sizeof(vector3));
	mesh->faces = datam_darr_new(sizeof(tri_face));
	return mesh;
}

void unimesh_load_obj(unimesh *mesh, FILE *src){
	static char line[1024];
	vector3 vertex;
	tri_face face;
	mesh->vertices->n = 0;
	mesh->faces->n = 0;
	while(fgets(line, 1024, src)){
		if(*line == '#')
			continue;
		if(strncmp(line, "v ", 2)){ /* Vertex */
			char *nptr = line + 2;
			vertex.x = strtod(nptr, &nptr);
			vertex.y = strtod(nptr, &nptr);
			vertex.z = strtod(nptr, &nptr);
			datam_darr_push(mesh->vertices, &vertex);
		}
		else if(strncmp(line, "f ", 2)){ /* Face */
			char *nptr = line + 2;
			face.indices[0] = strtol(nptr, &nptr, 10) - 1;
			while(*nptr != ' ')
				nptr++;
			face.indices[1] = strtol(nptr, &nptr, 10) - 1;
			while(*nptr != ' ')
				nptr++;
			face.indices[2] = strtol(nptr, &nptr, 10) - 1;
			datam_darr_push(mesh->faces, &face);
		}
	}
}

void unimesh_destroy(unimesh *mesh){
	datam_darr_delete(mesh->vertices);
	datam_darr_delete(mesh->faces);
	free(mesh);
}

void unimesh_save_obj(FILE *dst, unimesh *mesh){
	vector3 vertex;
	tri_face face;
	for(int i = 0; i < mesh->vertices->n; i++){
		datam_darr_get(mesh->vertices, &vertex, i);
		fprintf(dst, "v %f %f %f\n", vertex.x, vertex.y, vertex.z);
	}
	for(int i = 0; i < mesh->faces->n; i++){
		datam_darr_get(mesh->faces, &face, i);
		fprintf(dst, "f %d %d %d\n", face.indices[0] + 1, face.indices[1] + 1, face.indices[2] + 1);
	}
}