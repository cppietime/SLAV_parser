#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "datam.h"
#include "slavio.h"
#include "modeling.h"

#define SETBIT(arr, ind) (arr)[(ind) / 8] |= (1 << ((ind) % 8))
#define GETBIT(arr, ind) ((arr)[(ind) / 8] >> (((ind) % 8))) & 1

/*
Private function to add a face given three vertices and figure out the right orientation
Getting the right ordering for back-culling isn't quite working yet; it only works "most" of the time
*/
void putFace(int ai, int bi, int ci, int di, unimesh *mesh, volume_grid *grid){
	vector3 a, b, c, d;
	datam_darr_get(mesh->vertices, &a, ai);
	datam_darr_get(mesh->vertices, &b, bi);
	datam_darr_get(mesh->vertices, &c, ci);
	datam_darr_get(mesh->vertices, &d, di);
	vector3 center = {
		.x = (a.x + b.x + c.x + d.x) / 4,
		.y = (a.y + b.y + c.y + d.y) / 4,
		.z = (a.z + b.z + c.z + d.z) / 4
	};
	vector3 ab = {.x = b.x - a.x, .y = b.y - a.y, .z = b.z - a.z};
	vector3 ac = {.x = c.x - a.x, .y = c.y - a.y, .z = c.z - a.z};
	vector3 cross = {
		.x = ab.y * ac.z - ab.z * ac.y,
		.y = ab.z * ac.x - ab.x * ac.z,
		.z = ab.x * ac.y - ab.y * ac.x
	};
	float mag = .25 / sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
	cross.x *= mag;
	cross.y *= mag;
	cross.z *= mag;
	float offset = -.5;
	float pos = interp_voxel(grid, (center.x + cross.x + offset), (center.y + cross.y + offset), (center.z + cross.z + offset));
	float neg = interp_voxel(grid, (center.x - cross.x + offset), (center.y - cross.y + offset), (center.z - cross.z + offset));
	tri_face face;
	int invert = pos > neg;
	face.indices[0] = ai;
	face.indices[1 + invert] = bi;
	face.indices[2 - invert] = ci;
	datam_darr_push(mesh->faces, &face);
	face.indices[1 + invert] = di;
	face.indices[2 - invert] = bi;
	datam_darr_push(mesh->faces, &face);
}

void voxel_surface_net(unimesh *mesh, volume_grid *grid, float thresh){
	unsigned char *edges = calloc((3 * grid->x * grid->y * grid->z + 7) / 8, 1); /* We need three edges for each vertex, one in each direction. This gives us a little bit of extra,
																																							but not too much for me to worry about it right now. The vertex position x 3 gives the bit index
																																							of the edge from that vertex to that vertex + 1 in the x direction. Add 1 for y, 2 for z.*/
	// memset(edges, 0xffffffff, (3 * grid->x * grid->y * grid->z + 7) / 8);
	/* In z-direction */
	for(int x = 0; x < grid->x; x++){
		for(int y = 0; y < grid->y; y++){
			float prev_val = get_voxel(grid, x, y, 0);
			for(int z = 1; z < grid->z; z++){
				float next_val = get_voxel(grid, x, y, z);
				int index = (x * (grid->y * grid->z) + y * grid->z + z - 1) * 3 + 2;
				if((prev_val < thresh && next_val > thresh) || (prev_val > thresh && next_val < thresh)){
					// printf("Edge at (%d,%d,%d)-(%d,%d,%d) = %d\n", x, y, z - 1, x, y , z, index);
					SETBIT(edges, index);
				}
				prev_val = next_val;
			}
		}
	}
	/* In y-direction */
	for(int x = 0; x < grid->x; x++){
		for(int z = 0; z < grid->z; z++){
			float prev_val = get_voxel(grid, x, 0, z);
			for(int y = 1; y < grid->y; y++){
				float next_val = get_voxel(grid, x, y, z);
				int index = (x * (grid->y * grid->z) + (y - 1) * grid->z + z) * 3 + 1;
				if((prev_val < thresh && next_val > thresh) || (prev_val > thresh && next_val < thresh)){
					// printf("Edge at (%d,%d,%d)-(%d,%d,%d) = %d\n", x, y - 1, z, x, y, z, index);
					SETBIT(edges, index);
				}
				prev_val = next_val;
			}
		}
	}
	/* In x-direction */
	for(int y = 0; y < grid->y; y++){
		for(int z = 0; z < grid->z; z++){
			float prev_val = get_voxel(grid, 0, y, z);
			for(int x = 1; x < grid->x; x++){
				float next_val = get_voxel(grid, x, y, z);
			int index = ((x - 1) * (grid->y * grid->z) + y * grid->z + z) * 3;
				if((prev_val < thresh && next_val > thresh) || (prev_val > thresh && next_val < thresh)){
					// printf("Edge at (%d,%d,%d)-(%d,%d,%d) = %d\n", x - 1, y, z, x, y, z, index);
					SETBIT(edges, index);
				}
				prev_val = next_val;
			}
		}
	}
	
	
	vector3 *centers = malloc(sizeof(vector3) * (grid->x + 1) * (grid->y + 1) * (grid->z + 1)); /* For every point in the center of the cell bounded by 8 vertices, we need the
																																																effective center of it found by averaging the midpoints of edges that cross
																																																the threshold */
	for(int x = 0; x < grid->x + 1; x++){
		for(int y = 0; y < grid->y + 1; y++){
			for(int z = 0; z < grid->z + 1; z++){
				int index = x * (grid->y + 1) * (grid->z + 1) + y * (grid->z + 1) + z;
				
				int v000 = (x - 1) * grid->y * grid->z + (y - 1) * grid->z + z - 1;
				int v100 = x * grid->y * grid->z + (y - 1) * grid->z + z - 1;
				int v010 = (x - 1) * grid->y * grid->z + y * grid->z + z - 1;
				int v001 = (x - 1) * grid->y * grid->z + (y - 1) * grid->z + z;
				int v110 = x * grid->y * grid->z + y * grid->z + z - 1;
				int v101 = x * grid->y * grid->z + (y - 1) * grid->z + z;
				int v011 = (x - 1) * grid->y * grid->z + y * grid->z + z;
				int count = 0;
				centers[index].x = 0;
				centers[index].y = 0;
				centers[index].z = 0;
				if(x > 0 && y > 0 && z > 0){
					if(GETBIT(edges, v000 * 3)){ /* 0,0,0 - 1,0,0 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y-1, z-1, x, y-1, z-1, v000 * 3);
						centers[index].y -= 1;
						centers[index].z -= 1;
						count++;
					}
					if(GETBIT(edges, v000 * 3 + 1)){ /* 0,0,0 - 0,1,0 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y-1, z-1, x-1, y, z-1, v000 * 3 + 1);
						centers[index].x -= 1;
						centers[index].z -= 1;
						count++;
					}
					if(GETBIT(edges, v000 * 3 + 2)){ /* 0,0,0 - 0,0,1 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y-1, z-1, x-1, y-1, z, v000 * 3 + 2);
						centers[index].x -= 1;
						centers[index].y -= 1;
						count++;
					}
				}
				if(x < grid->x && y > 0 && z > 0){
					if(GETBIT(edges, v100 * 3 + 1)){ /* 1,0,0 - 1,1,0 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x, y-1, z-1, x, y, z-1, v100 * 3 + 1);
						centers[index].x += 1;
						centers[index].z -= 1;
						count++;
					}
					if(GETBIT(edges, v100 * 3 + 2)){ /* 1,0,0 - 1,0,1 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x, y-1, z-1, x, y-1, z, v100 * 3 + 2);
						centers[index].x += 1;
						centers[index].y -= 1;
						count++;
					}
				}
				if(x > 0 && y < grid->y && z > 0){
					if(GETBIT(edges, v010 * 3)){ /* 0,1,0 - 1,1,0 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y, z-1, x, y, z-1, v010 * 3);
						centers[index].y += 1;
						centers[index].z -= 1;
						count++;
					}
					if(GETBIT(edges, v010 * 3 + 2)){ /* 0,1,0 - 0,1,1 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y, z-1, x-1, y, z, v010 * 3 + 2);
						centers[index].x -= 1;
						centers[index].y += 1;
						count++;
					}
				}
				if(x > 0 && y > 0 && z < grid->z){
					if(GETBIT(edges, v001 * 3)){ /* 0,0,1 - 1,0,1 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y-1, z, x, y-1, z, v001 * 3);
						centers[index].y -= 1;
						centers[index].z += 1;
						count++;
					}
					if(GETBIT(edges, v001 * 3 + 1)){ /* 0,0,1 - 0,1,1 */
						// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y-1, z, x-1, y, z, v001 * 3 + 1);
						centers[index].x -= 1;
						centers[index].z += 1;
						count++;
					}
				}
				if(x < grid->x && y < grid->y && z > 0 && GETBIT(edges, v110 * 3 + 2)){ /* 1,1,0 - 1,1,1 */
					// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x, y, z-1, x, y, z, v110 * 3 + 2);
					centers[index].x += 1;
					centers[index].y += 1;
					count++;
				}
				if(x < grid->x && y > 0 && z < grid->z && GETBIT(edges, v101 * 3 + 1)){ /* 1,0,1 - 1,1,1 */
					// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x, y-1, z, x, y, z, v101 * 3 + 1);
					centers[index].x += 1;
					centers[index].z += 1;
					count++;
				}
				if(x > 0 && y < grid->y && z < grid->z && GETBIT(edges, v011 *3)){ /* 0,1,1 - 1,1,1 */
					// printf("(%d,%d,%d) hits edge (%d, %d, %d)-(%d, %d, %d) = %d\n", x, y, z, x-1, y, z, x, y, z, v011 * 3);
					centers[index].y += 1;
					centers[index].z += 1;
					count++;
				}
				if(count){
					if(count){
						centers[index].x /= count;
						centers[index].y /= count;
						centers[index].z /= count;
					}
					// centers[index].x = 0;
					// centers[index].y = 0;
					// centers[index].z = 0;
					
					centers[index].x += x;
					centers[index].y += y;
					centers[index].z += z;
					datam_darr_push(mesh->vertices, centers + index);
					
					centers[index].x = mesh->vertices->n - 1;
				}
			}
		}
	}
	
	/* TODO add the option to close the ends */
	
	/* In z-direction */
	for(int x = 0; x < grid->x; x++){
		for(int y = 0; y < grid->y; y++){
			for(int z = 0; z < grid->z - 1; z++){
				int index = (x * (grid->y * grid->z) + y * grid->z + z) * 3 + 2;
				if(GETBIT(edges, index)){
					int i0 = x * (grid->y + 1) * (grid->z + 1) + y * (grid->z + 1) + z + 1;
					int i3 = (x + 1) * (grid->y + 1) * (grid->z + 1) + y * (grid->z + 1) + z + 1;
					int i2 = x * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z + 1;
					int i1 = (x + 1) * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z + 1;
					// printf("Z-face indexed %d (%d, %d, %d): %d -> %d -> %d -> %d:\n", index, x, y, z, i0, i1, i2, i3);
					// printf("\t(%d,%d,%d)-(%d,%d,%d)-(%d,%d,%d)-(%d,%d,%d)\n", x, y, z+1, x+1, y, z+1, x, y+1, z+1, x+1, y+1, z+1);
					putFace((int)centers[i0].x, (int)centers[i1].x, (int)centers[i2].x, (int)centers[i3].x, mesh, grid);
				}
			}
		}
	}
	/* In y-direction */
	for(int x = 0; x < grid->x; x++){
		for(int z = 0; z < grid->z; z++){
			for(int y = 0; y < grid->y - 1; y++){
				int index = (x * (grid->y * grid->z) + y * grid->z + z) * 3 + 1;
				if(GETBIT(edges, index)){
					int i0 = x * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z;
					int i3 = (x + 1) * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z;
					int i2 = x * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z + 1;
					int i1 = (x + 1) * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z + 1;
					// printf("Y-face indexed %d (%d, %d, %d): %d -> %d -> %d -> %d\n", index, x, y, z, i0, i1, i2, i3);
					// printf("\t(%d,%d,%d)-(%d,%d,%d)-(%d,%d,%d)-(%d,%d,%d)\n", x, y+1, z, x+1, y+1, z, x, y+1, z+1, x+1, y+1, z+1);
					putFace((int)centers[i0].x, (int)centers[i1].x, (int)centers[i2].x, (int)centers[i3].x, mesh, grid);
				}
			}
		}
	}
	/* In x-direction */
	for(int y = 0; y < grid->y; y++){
		for(int z = 0; z < grid->z; z++){
			for(int x = 0; x < grid->x - 1; x++){
				int index = (x * (grid->y * grid->z) + y * grid->z + z) * 3;
				if(GETBIT(edges, index)){
					int i0 = (x + 1) * (grid->y + 1) * (grid->z + 1) + y * (grid->z + 1) + z;
					int i3 = (x + 1) * (grid->y + 1) * (grid->z + 1) + y * (grid->z + 1) + z + 1;
					int i2 = (x + 1) * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z;
					int i1 = (x + 1) * (grid->y + 1) * (grid->z + 1) + (y + 1) * (grid->z + 1) + z + 1;
					// printf("X-face indexed %d (%d, %d, %d): %d -> %d -> %d -> %d\n", index, x, y, z, i0, i1, i2, i3);
					// printf("\t(%d,%d,%d)-(%d,%d,%d)-(%d,%d,%d)-(%d,%d,%d)\n", x+1, y, z, x+1, y, z+1, x+1, y+1, z, x+1, y+1, z+1);
					putFace((int)centers[i0].x, (int)centers[i1].x, (int)centers[i2].x, (int)centers[i3].x, mesh, grid);
				}
			}
		}
	}
	
	free(centers);
	free(edges);
}

int main(){
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	srand_replace(now.tv_nsec / 13 + now.tv_sec);
	srand(now.tv_nsec / 13 + now.tv_sec);
	unimesh *mesh = unimesh_new();
	printf("Made mesh\n");
	volume_grid *grid = volume_grid_new(4, 4, 4);
	printf("Made grid\n");
	perlin_voxels(grid, 2, 2, 2);
	printf("Made noise\n");
	voxel_surface_net(mesh, grid, 0);
	printf("Rendered to model\n");
	free(grid);
	FILE *save = fopen("model.obj", "w");
	unimesh_save_obj(save, mesh);
	fclose(save);
	printf("Saved\n");
	unimesh_destroy(mesh);
	return 0;
}