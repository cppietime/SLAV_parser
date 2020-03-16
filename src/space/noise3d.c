#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "slavio.h"
#include "datam.h"
#include "modeling.h"

volume_grid* volume_grid_new(int x, int y, int z){
	volume_grid *grid = malloc(sizeof(volume_grid) + sizeof(float[x * y * z]));
	grid->x = x;
	grid->y = y;
	grid->z = z;
	return grid;
}

float get_voxel(volume_grid *grid, int x, int y, int z){
	return grid->values[x * (grid->y * grid->z) + y * grid->z + z];
}

float interp_voxel(volume_grid *grid, float x, float y, float z){
	int x0 = (int)x;
	int y0 = (int)y;
	int z0 = (int)z;
	float dx = x - x0;
	float dy = y - y0;
	float dz = z - z0;
	
	float n0 = get_voxel(grid, x0, y0, z0);
	float n1 = get_voxel(grid, x0 + 1, y0, z0);
	float x00 = n0 + (n1 - n0) * dx;
	n0 = get_voxel(grid, x0, y0 + 1, z0);
	n1 = get_voxel(grid, x0 + 1, y0 + 1, z0);
	float x01 = n0 + (n1 - n0) * dx;
	float y00 = x00 + (x01 - x00) * dy;
	
	n0 = get_voxel(grid, x0, y0, z0 + 1);
	n1 = get_voxel(grid, x0 + 1, y0, z0 + 1);
	x00 = n0 + (n1 - n0) * dx;
	n0 = get_voxel(grid, x0, y0 + 1, z0 + 1);
	n1 = get_voxel(grid, x0 + 1, y0 + 1, z0 + 1);
	x01 = n0 + (n1 - n0) * dx;
	float y01 = x00 + (x01 - x00) * dy;
	
	float lerp = y00 + (y01 - y00) * dz;
	return lerp;
}

void set_voxel(volume_grid *grid, int x, int y, int z, float val){
	grid->values[x * (grid->y * grid->z) + y * grid->z + z] = val;
}

/* private method. calculates the dot product of P - P0 and a gradient vector) */
float dotGradient(int x0, int y0, int z0, int x_res, int y_res, int z_res, float x, float y, float z, vector3 *units){
	float dx = ((float)x - x0) / x_res, dy = ((float)y - y0)/ y_res, dz = ((float)z - z0) / z_res;
	/* First get the value along the edge (0,0,0) - (1,0,0) */
	vector3 corner = units[x0 * (y_res * z_res) + y0 * z_res + z0];
	float n0 = corner.x * dx + corner.y * dy + corner.z * dz;
	corner = units[(x0 + 1) * (y_res * z_res) + y0 * z_res + z0];
	float n1 = corner.x * dx + corner.y * dy + corner.z * dz;
	float fy0 = n0 + (n1 - n0) * dx;
	
	/* Get the value along edge (0,1,0) - (1,1,0) */
	corner = units[x0 * (y_res * z_res) + (y0 + 1) * z_res + z0];
	n0 = corner.x * dx + corner.y * dy + corner.z * dz;
	corner = units[(x0 + 1) * (y_res * z_res) + (y0 + 1) * z_res + z0];
	n1 = corner.x * dx + corner.y * dy + corner.z * dz;
	float fy1 = n0 + (n1 - n0) * dx;
	
	/* Get the value along the z=0 face */
	float fz0 = fy0 + (fy1 - fy0) * dy;
	
	/* Get the value along the edge (0,0,1) - (1,0,1) */
	corner = units[x0 * (y_res * z_res) + y0 * z_res + z0 + 1];
	n0 = corner.x * dx + corner.y * dy + corner.z * dz;
	corner = units[(x0 + 1) * (y_res * z_res) + y0 * z_res + z0 + 1];
	n1 = corner.x * dx + corner.y * dy + corner.z * dz;
	fy0 = n0 + (n1 - n0) * dx;
	
	/* Get the value along edge (0,1,1) - (1,1,1) */
	corner = units[x0 * (y_res * z_res) + (y0 + 1) * z_res + z0 + 1];
	n0 = corner.x * dx + corner.y * dy + corner.z * dz;
	corner = units[(x0 + 1) * (y_res * z_res) + (y0 + 1) * z_res + z0 + 1];
	n1 = corner.x * dx + corner.y * dy + corner.z * dz;
	fy1 = n0 + (n1 - n0) * dx;
	
	/* Get the value along the z=0 face */
	float fz1 = fy0 + (fy1 - fy0) * dy;
	
	/* Return the trilinear interpolated value */
	float dot = fz0 + (fz1 - fz0) * dz;
	return dot;
}

void perlin_voxels(volume_grid *grid, int x_cel, int y_cel, int z_cel){
	int x_res = grid->x / x_cel + 1, y_res = grid->y / y_cel + 1, z_res = grid->z / z_cel + 1;
	int num_vecs = x_res * y_res * z_res;
	vector3 *units = malloc(sizeof(vector3) * num_vecs);
	for(int i = 0; i < num_vecs; i++){
		float a = gaussian();
		float b = gaussian();
		float c = gaussian();
		float norm = 1.0 / sqrt(a * a + b * b + c * c);
		units[i].x = a * norm;
		units[i].y = b * norm;
		units[i].z = c * norm;
	}
	for(int x = 0; x < grid->x; x++){
		int x0 = x / x_res;
		for(int y = 0; y < grid->y; y++){
			int y0 = y / y_res;
			for(int z = 0; z < grid->z; z++){
				int z0 = z / z_res;
				set_voxel(grid, x, y, z, dotGradient(x0, y0, z0, x_cel, y_cel, z_cel, x, y, z, units));
			}
		}
	}
	free(units);
}