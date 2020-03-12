/*
Some functionality for 3d modeling. WIP
*/

#ifndef _H_SLAVMOD
#define _H_SLAVMOD

#include "datam.h"

/* A triangular face in a mesh */
typedef struct _tri_face{
	int indices[3];
} tri_face;

/* A vertex or vector of three coordinates */
typedef struct _vector3{
	float x, y, z;
} vector3;

/* A mesh with no extra data for faces/vertices */
typedef struct _unimesh{
	datam_darr *vertices;
	datam_darr *faces;
} unimesh;

unimesh* unimesh_new(); /* Allocate and return a new unimesh object */
void unimesh_load_obj(unimesh *mesh, FILE *src); /* Load a mesh from a wavefront obj file */
void unimesh_destroy(unimesh *mesh); /* Destroy a unimesh and its data */
void unimesh_save_obj(FILE *dst, unimesh *mesh); /* Save a mesh to a wavefront obj file */

/* A 3-d voxel grid. Values are stored in x-y-z order */
typedef struct _volume_grid{
	int x, y, z;
	float values[];
} volume_grid;

volume_grid* volume_grid_new(int x, int y, int z); /* Create and return a new empty grid. This can just be freed when done */
float get_voxel(volume_grid *grid, int x, int y, int z); /* Get the voxel value at (x, y, z) */
float interp_voxel(volume_grid *grid, float x, float y, float z); /* Linearly interpolate an in-between point */
void set_voxel(volume_grid *grid, int x, int y, int z, float val); /* Set the voxel at (x, y, z) to val */
void perlin_voxels(volume_grid *grid, int x_cel, int y_cel, int z_cel); /* Fill a voxel grid with perlin noise with (x, y, z) resolution */

void voxel_surface_net(unimesh *mesh, volume_grid *grid, float thresh); /* Convert a volume grid to a mesh via surface nets */

#endif